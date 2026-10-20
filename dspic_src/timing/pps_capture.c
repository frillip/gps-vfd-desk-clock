#include "pps_capture.h"

#include <xc.h>

#include "../platform/system.h"
#include "timing_config.h"

// A reference period outside +/-1000 ppm of nominal is a missed or extra edge.
#define PERIOD_LIMIT_LOWER (TIMING_NOMINAL_TICKS - (TIMING_NOMINAL_TICKS / 1000UL))
#define PERIOD_LIMIT_UPPER (TIMING_NOMINAL_TICKS + (TIMING_NOMINAL_TICKS / 1000UL))

#define SYNCSEL_TIMER1  0b01011 // ICxCON2: synchronise to Timer1
#define ICM_RISING_EDGE 0b011
#define ICTSEL_FP       0b111 // count the peripheral clock

#define SYNC_STROBE_HALF_US 96U // low then high, each this long

static volatile bool timebase_aligned;    // Timer1 sync event has happened
static volatile bool captures_trusted;    // the alignment check passed
static volatile uint32_t ic12_correction; // taken off IC1/IC2 captures (see pps_capture_init)

static volatile uint8_t ic12_input = PPS_INPUT_GNSS;
static volatile uint32_t ic12_stamp;
static volatile uint16_t ic12_low;
static volatile uint16_t ic12_high;
static volatile bool ic12_rollover_corrected;
static volatile bool ic12_stamp_valid;
static volatile uint32_t ic12_period = TIMING_NOMINAL_TICKS;
static volatile uint32_t ic12_period_sequence;
static volatile bool ic12_period_valid;
static volatile uint32_t ic12_edge_sequence;
static volatile uint32_t ic12_previous;
static volatile bool ic12_previous_valid; // ic12_previous may start a period

static volatile uint32_t ic34_stamp;
static volatile uint16_t ic34_low;
static volatile uint16_t ic34_high;
static volatile bool ic34_rollover_corrected;
static volatile bool ic34_stamp_valid;

// --------------------------------------------------------------------------
// Set-up
// --------------------------------------------------------------------------

// Both pairs: 32-bit cascade, synchronised to Timer1 for now so that the
// Timer1 event resets all four timers on the same cycle. Only the odd
// module of each pair interrupts; it reads both halves.
static void configure_captures(void)
{
    IC1CON1 = 0;
    IC1CON2 = 0;
    IC2CON1 = 0;
    IC2CON2 = 0;
    IC3CON1 = 0;
    IC3CON2 = 0;
    IC4CON1 = 0;
    IC4CON2 = 0;

    IC1CON1bits.ICTSEL = ICTSEL_FP;
    IC2CON1bits.ICTSEL = ICTSEL_FP;
    IC3CON1bits.ICTSEL = ICTSEL_FP;
    IC4CON1bits.ICTSEL = ICTSEL_FP;
    IC1CON2bits.IC32 = 1;
    IC2CON2bits.IC32 = 1;
    IC3CON2bits.IC32 = 1;
    IC4CON2bits.IC32 = 1;
    IC1CON2bits.SYNCSEL = SYNCSEL_TIMER1;
    IC2CON2bits.SYNCSEL = SYNCSEL_TIMER1;
    IC3CON2bits.SYNCSEL = SYNCSEL_TIMER1;
    IC4CON2bits.SYNCSEL = SYNCSEL_TIMER1;

    IFS0bits.IC1IF = 0;
    IFS0bits.IC2IF = 0;
    IFS2bits.IC3IF = 0;
    IFS2bits.IC4IF = 0;
    IPC0bits.IC1IP = TIMING_CAPTURE_INTERRUPT_PRIORITY;
    IPC9bits.IC3IP = TIMING_CAPTURE_INTERRUPT_PRIORITY;
    IEC0bits.IC1IE = 1;
    IEC0bits.IC2IE = 0;
    IEC2bits.IC3IE = 1;
    IEC2bits.IC4IE = 0;

    // Even module first: the cascade needs its upper half running.
    IC2CON1bits.ICM = ICM_RISING_EDGE;
    IC1CON1bits.ICM = ICM_RISING_EDGE;
    IC4CON1bits.ICM = ICM_RISING_EDGE;
    IC3CON1bits.ICM = ICM_RISING_EDGE;
}


// Timer1 fires once, 500 us from now; its handler takes the captures off
// Timer1 sync (see _T1Interrupt), after which all four free-run aligned.
static void align_timebases(void)
{
    T1CON = 0;
    PR1 = (uint16_t)((TIMING_TICKS_PER_US * 500UL) - 1UL);
    TMR1 = 0;
    IFS0bits.T1IF = 0;
    IEC0bits.T1IE = 1;
    T1CONbits.TON = 1;

    while (!timebase_aligned)
    {
    }
}


static uint16_t input_pin(PpsInput input)
{
    return input == PPS_INPUT_ESP ? ESP_PPS_RPIN : GNSS_PPS_RPIN;
}


static void route_inputs(uint16_t reference_pin, uint16_t epoch_output_pin)
{
    __builtin_write_OSCCONL(OSCCON & 0xBF); // unlock PPS
    RPINR7bits.IC1R = reference_pin;
    RPINR7bits.IC2R = reference_pin;
    RPINR8bits.IC3R = epoch_output_pin;
    RPINR8bits.IC4R = epoch_output_pin;
    __builtin_write_OSCCONL(OSCCON | 0x40); // lock PPS
}


static void flush_reference(void)
{
    while (IC1CON1bits.ICBNE)
        (void)IC1BUF;
    while (IC2CON1bits.ICBNE)
        (void)IC2BUF;
    IFS0bits.IC1IF = 0;
    IFS0bits.IC2IF = 0;
}


static void flush_captures(void)
{
    flush_reference();
    while (IC3CON1bits.ICBNE)
        (void)IC3BUF;
    while (IC4CON1bits.ICBNE)
        (void)IC4BUF;
    IFS2bits.IC3IF = 0;
    IFS2bits.IC4IF = 0;
}


// One alignment and check: all four modules reset and synchronised to
// Timer1, then one strobe edge captured by both pairs. Returns IC1/IC2
// minus IC3/IC4, and false if either pair missed the edge.
static bool align_and_check(PpsCaptureSync *sync, int32_t *offset)
{
    timebase_aligned = false;
    route_inputs(CAPTURE_SYNC_STROBE_RPIN, CAPTURE_SYNC_STROBE_RPIN);
    configure_captures();
    flush_captures();
    align_timebases();

    // Different values, so a missed edge never passes as agreement.
    ic12_stamp = 0xFFFFFFFFUL;
    ic34_stamp = 0xFFFFFFFEUL;
    CAPTURE_SYNC_STROBE_SET_LOW();
    CAPTURE_SYNC_STROBE_SET_DIGITAL_OUTPUT();
    delay_us(SYNC_STROBE_HALF_US);
    CAPTURE_SYNC_STROBE_SET_HIGH(); // the common edge
    delay_us(SYNC_STROBE_HALF_US);
    CAPTURE_SYNC_STROBE_SET_DIGITAL_INPUT();
    CAPTURE_SYNC_STROBE_SET_LOW();

    sync->ic12_stamp = ic12_stamp;
    sync->ic34_stamp = ic34_stamp;
    *offset = (int32_t)(ic12_stamp - ic34_stamp);
    return ic12_stamp < 0xFFFFFFFEUL && ic34_stamp < 0xFFFFFFFEUL;
}


bool pps_capture_init(PpsCaptureSync *sync)
{
    int32_t offset = 0;
    bool captured = false;
    uint8_t steady = 0; // consecutive attempts with the same offset

    sync->attempts = 0;
    sync->correction = 0;
    sync->valid = false;
    while (sync->attempts < PPS_CAPTURE_ALIGN_ATTEMPTS)
    {
        int32_t previous = offset;
        bool previous_captured = captured;

        captured = align_and_check(sync, &offset);
        sync->offsets[sync->attempts++] = captured ? offset : INT32_MAX;
        if (captured && offset == 0)
        {
            sync->valid = true;
            break;
        }
        steady = (captured && previous_captured && offset == previous) ? steady + 1U : 0U;
    }
    // Never aligned: a steady small offset is corrected, anything else is not
    // to be trusted.
    if (!sync->valid && captured && steady >= 2U && offset <= PPS_CAPTURE_ALIGN_TOLERANCE &&
        offset >= -PPS_CAPTURE_ALIGN_TOLERANCE)
    {
        sync->correction = offset;
        sync->valid = true;
    }

    // Switch to the real signals with nothing able to capture half-way.
    __builtin_disi(0x3FFF);
    route_inputs(input_pin(PPS_INPUT_GNSS), DISPLAY_LATCH_OC_RPIN);
    flush_captures();
    ic12_correction = (uint32_t)sync->correction;
    captures_trusted = sync->valid;
    ic12_input = PPS_INPUT_GNSS;
    ic12_edge_sequence = 0; // the strobe edges do not count
    ic12_previous_valid = false;
    ic12_stamp_valid = false;
    ic34_stamp_valid = false;
    __builtin_disi(0);

    return sync->valid;
}


void pps_capture_sample(PpsCaptureSample *sample)
{
    sample->reference_input = ic12_input;
    sample->reference_capture = ic12_stamp;
    sample->reference_low = ic12_low;
    sample->reference_high = ic12_high;
    sample->reference_rollover_corrected = ic12_rollover_corrected;
    sample->reference_capture_valid = ic12_stamp_valid;
    sample->reference_edge_sequence = ic12_edge_sequence;
    sample->reference_period = ic12_period;
    sample->reference_period_sequence = ic12_period_sequence;
    sample->reference_period_valid = ic12_period_valid;

    sample->epoch_capture = ic34_stamp;
    sample->epoch_low = ic34_low;
    sample->epoch_high = ic34_high;
    sample->epoch_rollover_corrected = ic34_rollover_corrected;
    sample->epoch_capture_valid = ic34_stamp_valid;

    ic12_stamp_valid = false;
    ic34_stamp_valid = false;
}


void pps_capture_route(PpsInput input)
{
    if (input >= PPS_INPUT_COUNT || input == (PpsInput)ic12_input)
    {
        return;
    }
    // If the new pin is high (its pulse lasts 100 ms), switching to it is a
    // rising edge to the capture: let it through the input synchroniser,
    // then flush it.
    __builtin_disi(0x3FFF);
    route_inputs(input_pin(input), DISPLAY_LATCH_OC_RPIN);
    __builtin_nop();
    __builtin_nop();
    __builtin_nop();
    __builtin_nop();
    flush_reference();
    ic12_input = (uint8_t)input;
    ic12_previous_valid = false;
    ic12_period_valid = false;
    ic12_stamp_valid = false;
    __builtin_disi(0);
}


PpsInput pps_capture_input(void)
{
    return (PpsInput)ic12_input;
}


// The upper half is read either side of the lower: if it moved, the lower
// half rolled over in between, and its value says on which side.
uint32_t pps_capture_timebase(void)
{
    uint16_t high = IC4TMR;
    uint16_t low = IC3TMR;
    uint16_t high_again = IC4TMR;

    if (high != high_again && low < 0x8000U)
    {
        high = high_again;
    }
    return ((uint32_t)high << 16) | low;
}


// --------------------------------------------------------------------------
// Interrupt handlers
// --------------------------------------------------------------------------

/*
 * Silicon erratum 31: the upper timer of a cascade increments one cycle
 * after the lower one rolls over, so a capture on exactly that cycle reads
 * the old upper half. Correct it.
 */
static uint32_t join_cascade(uint16_t low, uint16_t high, volatile bool *corrected)
{
    *corrected = (low == 0x0000U);
    if (low == 0x0000U)
    {
        high++;
    }
    return ((uint32_t)high << 16) | low;
}


// Timer1 event: every capture timer was just reset by it. Leave them
// free-running from here, aligned.
void __attribute__((interrupt, no_auto_psv)) _T1Interrupt(void)
{
    IFS0bits.T1IF = 0;
    IEC0bits.T1IE = 0;

    while (TMR1 < 16U)
    {
    }

    IC1CON2bits.SYNCSEL = 0;
    IC2CON2bits.SYNCSEL = 0;
    IC3CON2bits.SYNCSEL = 0;
    IC4CON2bits.SYNCSEL = 0;

    T1CONbits.TON = 0;
    timebase_aligned = true;
    IFS0bits.T1IF = 0;
}


// Reference PPS edge.
void __attribute__((interrupt, no_auto_psv)) _IC1Interrupt(void)
{
    uint16_t low = IC1BUF;
    uint16_t high = IC2BUF;
    uint32_t capture;

    ic12_low = low;
    ic12_high = high;
    capture = join_cascade(low, high, &ic12_rollover_corrected) - ic12_correction;

    if (ic12_previous_valid)
    {
        uint32_t period = capture - ic12_previous;

        if (period >= PERIOD_LIMIT_LOWER && period <= PERIOD_LIMIT_UPPER)
        {
            ic12_period = period;
            ic12_period_sequence++;
            ic12_period_valid = true;
        }
        else
        {
            ic12_period_valid = false;
        }
    }

    ic12_previous = capture;
    ic12_previous_valid = true;
    ic12_stamp = capture;
    ic12_stamp_valid = captures_trusted;
    if (captures_trusted)
    {
        ic12_edge_sequence++;
    }

    IFS0bits.IC1IF = 0;
}


// Epoch output edge, read back from the OC1 pin.
void __attribute__((interrupt, no_auto_psv)) _IC3Interrupt(void)
{
    uint16_t low = IC3BUF;
    uint16_t high = IC4BUF;

    ic34_low = low;
    ic34_high = high;
    ic34_stamp = join_cascade(low, high, &ic34_rollover_corrected);
    ic34_stamp_valid = captures_trusted;

    IFS2bits.IC3IF = 0;
}
