#include "pps_monitor.h"

#include <xc.h>

#include "../platform/system.h"
#include "epoch_timer.h"
#include "timing_config.h"

// A period outside +/-1000 ppm of nominal is a missed or extra edge.
#define PERIOD_LIMIT_LOWER (TIMING_NOMINAL_TICKS - (TIMING_NOMINAL_TICKS / 1000UL))
#define PERIOD_LIMIT_UPPER (TIMING_NOMINAL_TICKS + (TIMING_NOMINAL_TICKS / 1000UL))

typedef struct
{
    volatile uint32_t stamp;
    volatile uint32_t sequence;
    volatile uint32_t observed_ms;
    volatile bool valid; // new since the last sample
    volatile uint32_t period;
    volatile bool period_valid;
    volatile bool seen; // at least one edge
    bool high;          // pin level at the last interrupt
} Watch;

static Watch watches[PPS_INPUT_COUNT];
static PpsMonitorStats stats;

static bool input_high(PpsInput input)
{
    return input == PPS_INPUT_ESP ? ESP_PPS_GET_VALUE() != 0 : GNSS_PPS_GET_VALUE() != 0;
}


void pps_monitor_init(void)
{
    uint8_t input;

    for (input = 0; input < PPS_INPUT_COUNT; input++)
    {
        watches[input].high = input_high((PpsInput)input);
    }
    GNSS_PPS_CHANGE_ENABLE();
    ESP_PPS_CHANGE_ENABLE();
    IFS1bits.CNIF = 0;
    IPC4bits.CNIP = TIMING_MONITOR_INTERRUPT_PRIORITY;
    IEC1bits.CNIE = 1;
}


void pps_monitor_sample(PpsMonitorSample *sample)
{
    uint8_t input;

    for (input = 0; input < PPS_INPUT_COUNT; input++)
    {
        Watch *watch = &watches[input];
        PpsMonitorEdge *edge = &sample->inputs[input];

        edge->stamp = watch->stamp;
        edge->sequence = watch->sequence;
        edge->observed_ms = watch->observed_ms;
        edge->valid = watch->valid;
        edge->period = watch->period;
        edge->period_valid = watch->period_valid;
        watch->valid = false;
    }
}


void pps_monitor_edge(PpsInput input, PpsEdge *edge)
{
    const Watch *watch = &watches[input < PPS_INPUT_COUNT ? input : PPS_INPUT_GNSS];

    __builtin_disi(0x3FFF);
    edge->sequence = watch->sequence;
    edge->observed_ms = watch->observed_ms;
    edge->valid = watch->seen;
    __builtin_disi(0);
}


const PpsMonitorStats *pps_monitor_stats(void)
{
    return &stats;
}


// --------------------------------------------------------------------------
// Interrupt handler
// --------------------------------------------------------------------------

static void rising_edge(PpsInput input, uint32_t stamp, uint32_t now_ms)
{
    Watch *watch = &watches[input];
    uint32_t period = stamp - watch->stamp;

    watch->period_valid =
        watch->seen && period >= PERIOD_LIMIT_LOWER && period <= PERIOD_LIMIT_UPPER;
    if (watch->seen && !watch->period_valid)
    {
        stats.implausible[input]++;
    }
    watch->period = period;
    watch->stamp = stamp;
    watch->sequence++;
    watch->observed_ms = now_ms;
    watch->valid = true;
    watch->seen = true;
    stats.edges[input]++;
}


// Any change on a watched pin. Timestamped first thing; only rising edges
// count. The pins are read before the flag is cleared: reading the port is
// what ends the mismatch.
void __attribute__((interrupt, no_auto_psv)) _CNInterrupt(void)
{
    uint32_t stamp = pps_capture_timebase();
    bool high[PPS_INPUT_COUNT];
    bool changed = false;
    uint8_t input;
    uint32_t now_ms;

    for (input = 0; input < PPS_INPUT_COUNT; input++)
    {
        high[input] = input_high((PpsInput)input);
    }
    IFS1bits.CNIF = 0;

    now_ms = epoch_timer_ms(); // T4 may preempt: it re-reads
    for (input = 0; input < PPS_INPUT_COUNT; input++)
    {
        if (high[input] != watches[input].high)
        {
            changed = true;
            watches[input].high = high[input];
            if (high[input])
            {
                rising_edge((PpsInput)input, stamp, now_ms);
            }
        }
    }
    if (!changed)
    {
        stats.unseen++;
    }
}
