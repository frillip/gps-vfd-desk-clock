/*
 * Timing configuration shared by the timing modules.
 *
 * Everything in timing/ counts the same clock: T2/T3 (the epoch timer), T4
 * (the millisecond scheduler), IC1-IC4 and OC1 all run from FP = FCY, so one
 * tick is 25 ns at 40 MHz and a nominal second is FCY ticks.
 *
 * The modules and how they fit together:
 *
 *   pps_capture   IC1/IC2 timestamp the reference PPS (GNSS or ESP), IC3/IC4
 *                 timestamp the epoch output; Timer1 aligns their
 *                 timebases at start-up.
 *   pps_monitor   change notification on both PPS pins: every edge of
 *                 either, numbered and coarsely timestamped.
 *   epoch_output  OC1, triggered by the T2/T3 period match: the epoch edge.
 *   epoch_timer   T2/T3 epoch timer (period loaded at each epoch) and the
 *                 T4 scheduler that runs millisecond slots after each epoch.
 *   epoch_control ACQ -> STEP -> TRACK: Kalman frequency estimate, phase
 *                 gate and PI servo, deciding each next period.
 *   timing        start-up sequence and the per-epoch snapshot that ties
 *                 the above together; timing_report prints it.
 *
 * Which source the captured reference edge comes from, and whether the loop
 * may use it in a given epoch, is decided above timing/, in sync/.
 *
 * Interrupt priorities. T3 and T4 share a level so neither can interrupt the
 * other half-way through the T4 restart; T3 still wins a tie by natural
 * order. The capture interrupts only copy registers and sit below both.
 * The PPS watch (change notification) timestamps in software, so it sits
 * above the captures, whose timestamps are hardware, to keep its latency
 * short. The epoch edge itself is hardware (OC1), so no interrupt latency
 * moves it.
 */
#ifndef TIMING_CONFIG_H
#define TIMING_CONFIG_H

#include "../platform/clock.h"

#define TIMING_NOMINAL_TICKS     FCY // per second
#define TIMING_HALF_SECOND_TICKS (TIMING_NOMINAL_TICKS / 2UL)
#define TIMING_TICKS_PER_MS      (TIMING_NOMINAL_TICKS / 1000UL) // 40000
#define TIMING_TICKS_PER_US      (TIMING_NOMINAL_TICKS / 1000000UL)

#define TIMING_EPOCH_INTERRUPT_PRIORITY   4 // T3, T4
#define TIMING_MONITOR_INTERRUPT_PRIORITY 2 // CN
#define TIMING_CAPTURE_INTERRUPT_PRIORITY 1 // IC1, IC3

#endif // TIMING_CONFIG_H
