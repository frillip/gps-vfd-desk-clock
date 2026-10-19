#include "epoch_output.h"

#include <xc.h>

#include "../platform/board.h"

/*
 * OC1 runs from its own timer at FCY, held in reset until triggered by the
 * T2/T3 period match (Timer3 sync), in single-shot mode: armed low, driven
 * high on the OC1R match one cycle after the trigger.
 *
 * The OC1 timer is a one-shot (TRIGMODE = 1): the Timer3 trigger sets
 * TRIGSTAT and starts OC1TMR, the OC1R match drives the pin high, and the
 * OC1RS match clears TRIGSTAT in hardware, holding OC1TMR in reset for the
 * rest of the second. Two rules from DS70358C matter here:
 *   - Table 13-1: if OCxR is beyond the timer's period no compare happens,
 *     so OC1R must be below OC1RS. OC1RS = 0 left OC1R = 1 unreachable and
 *     the pin never rose.
 *   - 13.3.3.13 note 1: TRIGSTAT cannot be written by software in one-shot
 *     mode, so re-arming is a mode change only; the hardware has already
 *     cleared TRIGSTAT 41 cycles after the trigger.
 *
 * Re-arming (OCM = 000) hands the pin back to LATB/TRISB, so the pin must be an
 * output with its latch low (board_init); between pulses the latch holds
 * it low rather than letting it float into IC3/IC4.
 *
 * No OC1 interrupt: the epoch is the T3 interrupt, when the pin actually rose
 * is the IC3/IC4 capture, and whether it rose at all is OC1IF, checked when
 * the pulse is ended.
 */
#define OC1CON1_DISABLED   0x1C08U // OCTSEL = FP, TRIGMODE = 1, OCM = off
#define OC1CON1_ARMED      0x1C09U // ... OCM = 001: init low, high on OC1R
#define OC1CON2_TRIGGERED  0x008DU // OCTRIG = 1, SYNCSEL = 01101 (Timer3)
#define OC1_RISE_TICKS     1U      // OC1R: rise one cycle after the trigger
#define OC1_ONE_SHOT_TICKS 41U     // OC1RS: end of the one-shot timer run

static volatile uint32_t pulses_seen;
static volatile uint32_t pulses_missed;
static volatile uint16_t probe_con1;
static volatile uint16_t probe_con2;
static volatile bool probe_pin;

static void arm(void)
{
    OC1CON1 = OC1CON1_DISABLED; // mode change: output low
    OC1CON1 = OC1CON1_ARMED;    // wait for the next trigger
}


void epoch_output_init(void)
{
    OC1CON1 = OC1CON1_DISABLED;
    OC1CON2 = OC1CON2_TRIGGERED;
    OC1R = OC1_RISE_TICKS;
    OC1RS = OC1_ONE_SHOT_TICKS;

    IEC0bits.OC1IE = 0;
    IFS0bits.OC1IF = 0;
    arm();
}


void epoch_output_probe(void)
{
    probe_con1 = OC1CON1;
    probe_con2 = OC1CON2;
    probe_pin = DISPLAY_LATCH_OC_GET_VALUE();
}


// OC1IF is set by the compare match whether or not its interrupt is
// enabled, so it tells us whether the pulse fired.
void epoch_output_end(void)
{
    if (IFS0bits.OC1IF)
    {
        pulses_seen++;
    }
    else
    {
        pulses_missed++;
    }
    IFS0bits.OC1IF = 0;
    arm();
}


void epoch_output_sample(EpochOutputSample *sample)
{
    sample->pulses_seen = pulses_seen;
    sample->pulses_missed = pulses_missed;
    sample->probe_con1 = probe_con1;
    sample->probe_con2 = probe_con2;
    sample->probe_pin = probe_pin;
}
