#include "epoch_timer.h"

#include <xc.h>

#include "../platform/board.h"
#include "epoch_output.h"

#define SLOT_MIDPOINT_TICKS (EPOCH_SLOT_TICKS / 2UL)

static volatile bool measurement_due;
static volatile uint32_t epoch_sequence;
static volatile uint32_t handler_late;
static volatile uint16_t current_slot;
static volatile uint32_t milliseconds;
// epoch_timer_resume(): slot boundaries made up, and stalls that spanned an
// epoch.
static uint32_t resumed_slots;
static uint32_t resume_spans;

// Requested by the foreground, loaded at the next epoch.
static volatile uint32_t requested_period = TIMING_NOMINAL_TICKS;
static volatile bool requested_period_pending;
// Loaded in PR3:PR2: the length of the interval now running.
static volatile uint32_t running_period = TIMING_NOMINAL_TICKS;

static void load_period(uint32_t period_ticks)
{
    uint32_t register_value;

    if (period_ticks == 0U)
    {
        period_ticks = 1U;
    }
    register_value = period_ticks - 1U;
    PR3 = (uint16_t)(register_value >> 16);
    PR2 = (uint16_t)register_value;
    running_period = period_ticks;
}


// Configured here, started by the first epoch.
static void scheduler_init(void)
{
    T4CON = 0; // 16-bit, 1:1, FCY, stopped
    TMR4 = 0;
    PR4 = (uint16_t)(EPOCH_SLOT_TICKS - 1UL);

    IFS1bits.T4IF = 0;
    IPC6bits.T4IP = TIMING_EPOCH_INTERRUPT_PRIORITY;
    IEC1bits.T4IE = 1;
}


void epoch_timer_init(void)
{
    scheduler_init();

    T2CON = 0;
    TMR2 = 0;
    TMR3 = 0;
    load_period(TIMING_NOMINAL_TICKS);

    IFS0bits.T3IF = 0;
    IPC2bits.T3IP = TIMING_EPOCH_INTERRUPT_PRIORITY;
    IEC0bits.T3IE = 1;

    T2CONbits.T32 = 1;
    T2CONbits.TCKPS = 0;
    T2CONbits.TCS = 0;
    T2CONbits.TON = 1;
}


void epoch_timer_set_period(uint32_t period_ticks)
{
    __builtin_disi(0x3FFF);
    requested_period = period_ticks;
    requested_period_pending = true;
    __builtin_disi(0);
}


bool epoch_timer_measure_due(void)
{
    return measurement_due;
}


void epoch_timer_sample(EpochTimerSample *sample)
{
    sample->sequence = epoch_sequence;
    sample->running_period = running_period;
    sample->handler_late = handler_late;
    measurement_due = false;
}


// Two equal consecutive reads cannot straddle an update.
uint32_t epoch_timer_ms(void)
{
    uint32_t first;
    uint32_t second;

    do
    {
        first = milliseconds;
        second = milliseconds;
    } while (first != second);

    return first;
}


uint16_t epoch_timer_slot(void)
{
    return current_slot;
}


void epoch_timer_position(uint32_t *sequence, uint16_t *slot)
{
    __builtin_disi(0x3FFF);
    *sequence = epoch_sequence;
    *slot = current_slot;
    __builtin_disi(0);
}


// --------------------------------------------------------------------------
// Interrupt handlers
// --------------------------------------------------------------------------

/*
 * Restarts the scheduler from the epoch and counts the epoch as a millisecond
 * boundary exactly once. Works from where T4 stood at the epoch rather than
 * now, so handler latency cannot change the result.
 */
static void restart_scheduler(void)
{
    static bool scheduler_running;
    uint16_t since_epoch = TMR2; // cycles since the period match
    uint16_t slot_ticks = TMR4;  // T4 count now
    bool boundary_pending = IFS1bits.T4IF;
    bool boundary_after_epoch;
    uint16_t slot_ticks_at_epoch;

    if (since_epoch >= (uint16_t)EPOCH_SLOT_TICKS)
    {
        // Not expected: interrupts are never held off for a millisecond.
        since_epoch = (uint16_t)(EPOCH_SLOT_TICKS - 1UL);
        handler_late++;
    }

    // Preloading the elapsed cycles puts slot boundaries exactly whole ms
    // after the epoch itself.
    TMR4 = since_epoch;
    IFS1bits.T4IF = 0;
    T4CONbits.TON = 1;

    if (scheduler_running)
    {
        // T4 wrapped after the epoch if its count is now below the cycles
        // elapsed since the epoch.
        boundary_after_epoch = slot_ticks < since_epoch;
        slot_ticks_at_epoch =
            boundary_after_epoch ? (uint16_t)(slot_ticks + (uint16_t)EPOCH_SLOT_TICKS - since_epoch)
                                 : (uint16_t)(slot_ticks - since_epoch);

        // A boundary before the epoch whose interrupt was held off is real:
        // count it. One after the epoch is superseded by the restart.
        if (boundary_pending && !boundary_after_epoch)
        {
            milliseconds++;
        }
        // The last boundary was half a slot or more before the epoch, so the
        // epoch starts a new millisecond. Otherwise it merges with it.
        if (slot_ticks_at_epoch >= (uint16_t)SLOT_MIDPOINT_TICKS)
        {
            milliseconds++;
        }
    }
    else
    {
        milliseconds++;
        scheduler_running = true;
    }
    current_slot = 0;
}


/*
 * Epoch. The OC1 edge has already happened in hardware; this handler only
 * prepares the next interval, so its latency never moves the edge.
 */
void __attribute__((interrupt, no_auto_psv)) _T3Interrupt(void)
{
    restart_scheduler();

    if (requested_period_pending)
    {
        load_period(requested_period);
        requested_period_pending = false;
    }

    epoch_sequence++;
    STATUS_LED_SET_HIGH(); // heartbeat and scope marker; main() follows the pulse down

    IFS0bits.T3IF = 0;
}


// One slot boundary: the millisecond, and whatever happens at that slot.
static void advance_slot(void)
{
    milliseconds++;
    if (current_slot < UINT16_MAX)
    {
        current_slot++;
    }

    switch (current_slot)
    {
        case EPOCH_MEASURE_SLOT: measurement_due = true; break;

        case EPOCH_PULSE_PROBE_SLOT: epoch_output_probe(); break;

        case EPOCH_PULSE_END_SLOT: epoch_output_end(); break;

        default: break;
    }
}


// One slot boundary: slot n begins n ms after the epoch.
void __attribute__((interrupt, no_auto_psv)) _T4Interrupt(void)
{
    IFS1bits.T4IF = 0;
    advance_slot();
}


/*
 * After interrupts were held off for longer than a slot (a flash erase):
 * the slot and the milliseconds from where T2/T3 stands, the boundaries
 * missed counted as if they had run, and T4 put back in phase. If the epoch
 * itself came and went meanwhile, its interrupt restarts everything as
 * usual; the milliseconds missed before it are lost, and counted.
 */
void epoch_timer_resume(void)
{
    uint16_t low;
    uint32_t since_epoch;
    uint32_t slot;

    if (!T2CONbits.TON || IFS0bits.T3IF)
    {
        if (IFS0bits.T3IF)
        {
            resume_spans++;
        }
        return;
    }
    low = TMR2; // latches TMR3 into TMR3HLD
    since_epoch = ((uint32_t)TMR3HLD << 16) | low;
    slot = since_epoch / EPOCH_SLOT_TICKS;
    if (slot > current_slot)
    {
        resumed_slots += slot - current_slot;
    }
    while (current_slot < slot && current_slot < UINT16_MAX)
    {
        advance_slot();
    }
    TMR4 = (uint16_t)(since_epoch % EPOCH_SLOT_TICKS);
    IFS1bits.T4IF = 0;
}


void epoch_timer_resume_stats(uint32_t *slots, uint32_t *spans)
{
    *slots = resumed_slots;
    *spans = resume_spans;
}
