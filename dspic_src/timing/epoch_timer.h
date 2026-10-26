/*
 * The epoch timer (T2/T3) and the epoch-locked scheduler (T4).
 *
 * T2/T3 is a 32-bit timer whose period match is the epoch: it triggers OC1
 * (the epoch output) in hardware and raises the T3 interrupt. The period for
 * each interval is loaded by that interrupt, so a period requested now runs
 * from the next epoch: the interval already running cannot change.
 *
 * T4 counts 1 ms slots. The T3 interrupt restarts it at every epoch,
 * preloaded with the cycles already elapsed since the epoch, so slot n always
 * begins n ms after the epoch however late the interrupt runs. No alignment
 * state is needed: it cannot drift for longer than one second. Slot 0 is the
 * epoch itself.
 */
#ifndef EPOCH_TIMER_H
#define EPOCH_TIMER_H

#include <stdbool.h>
#include <stdint.h>

#include "timing_config.h"

#define EPOCH_SLOT_TICKS       TIMING_TICKS_PER_MS
#define EPOCH_MEASURE_SLOT     1U   // both captures of the epoch are in
#define EPOCH_PULSE_PROBE_SLOT 50U  // OC1 state sampled mid-pulse
#define EPOCH_PULSE_END_SLOT   100U // OC1 pulse ends: 100 ms high

typedef struct
{
    uint32_t sequence;       // epochs since start
    uint32_t running_period; // ticks, of the interval after this epoch
    uint32_t handler_late;   // T3 handler ran > 1 ms after its epoch
} EpochTimerSample;

// Configures T4 and starts T2/T3 at the nominal period. Start the epoch
// output first (epoch_output_init) so it sees the first trigger.
void epoch_timer_init(void);

// Period of the interval that starts at the next epoch, in ticks. A newer
// request before that epoch replaces this one.
void epoch_timer_set_period(uint32_t period_ticks);

// Set at EPOCH_MEASURE_SLOT, cleared by epoch_timer_sample().
bool epoch_timer_measure_due(void);

// Call with interrupts held off (see timing_snapshot). Clears the
// measurement-due flag.
void epoch_timer_sample(EpochTimerSample *sample);

/* Milliseconds on the epoch timeline: exactly 1000 per epoch while
 * tracking, so it follows UTC rather than the raw oscillator; an
 * acquisition step adds its length rounded to the nearest ms. Monotonic,
 * wraps after 49.7 days; compare with unsigned subtraction. */
uint32_t epoch_timer_ms(void);

// Milliseconds since the latest epoch (the current slot).
uint16_t epoch_timer_slot(void);

// The epoch sequence number and the slot within it, read together: a slot
// is never paired with the wrong epoch across the epoch interrupt.
void epoch_timer_position(uint32_t *sequence, uint16_t *slot);

// With interrupts held off, after they have been for longer than a slot
// (a flash erase stops the CPU for ~25 ms): brings the slot and the
// milliseconds up to where T2/T3 stands, running the slot work missed, and
// puts T4 back in phase. Nothing to do if the epoch passed meanwhile: its
// interrupt restarts the slots, and the stall is counted.
void epoch_timer_resume(void);
// Slot boundaries made up by epoch_timer_resume(), and stalls that spanned
// an epoch.
void epoch_timer_resume_stats(uint32_t *slots, uint32_t *spans);

#endif // EPOCH_TIMER_H
