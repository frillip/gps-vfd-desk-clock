/*
 * The epoch output: OC1 on DISPLAY_LATCH_OC (RB10, see platform/board.h).
 *
 * OC1 is triggered in hardware by the T2/T3 period match, so its rising edge
 * is the epoch, unaffected by any interrupt latency. It is authoritative for
 * everything connected to it. The pin then stays high until the scheduler
 * ends the pulse 100 ms later; the falling edge carries no timing.
 */
#ifndef EPOCH_OUTPUT_H
#define EPOCH_OUTPUT_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    uint32_t pulses_seen;   // epochs at which OC1 fired
    uint32_t pulses_missed; // epochs at which it did not
    // OC1 state 50 ms into the last pulse, for diagnosing a dead output.
    // Healthy: CON1 1C09, CON2 000D (TRIGSTAT clear again), pin high.
    uint16_t probe_con1;
    uint16_t probe_con2;
    bool probe_pin;
} EpochOutputSample;

// Configures and arms OC1. Call before the epoch timer starts, so the first
// trigger is not missed.
void epoch_output_init(void);

// Scheduler hooks (interrupt context, from the T4 handler).
void epoch_output_probe(void); // mid-pulse diagnostic sample
void epoch_output_end(void);   // drive low and re-arm for the next epoch

// Call with interrupts held off (see timing_snapshot).
void epoch_output_sample(EpochOutputSample *sample);

#endif // EPOCH_OUTPUT_H
