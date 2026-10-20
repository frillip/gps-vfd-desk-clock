/*
 * Bench instrumentation for tuning the loop profiles and the holdover model.
 * Diagnostic only: nothing here feeds back into the clock.
 *
 * Each minute (sync_tuning_print, then reset):
 *
 *   - the reference the loop steers by: its period against nominal and the
 *     loop's measured phase (mean, sd, extremes), gate and Kalman rejections;
 *   - each PPS input as watched: its period, and its latency behind the
 *     capture while it is the one routed there (for PPS_MONITOR_LATENCY_TICKS);
 *   - each source's phase against our epoch, from whichever edge it has: for
 *     the source not in use, that is its jitter as the PIC sees it (the ESP
 *     PPS against a GNSS-locked epoch, say), and its mean the offset to
 *     calibrate (SOURCE_NTP_PPS_DELAY_NS).
 *
 * And, cumulatively, virtual holdover trials, for the holdover model in
 * sync.h. While the loop is locked, a trial starts every
 * TUNING_TRIAL_INTERVAL epochs (up to TUNING_TRIALS at once): it takes the
 * period the loop would hold if the source failed now (Kalman estimate less
 * the servo integral) and adds up, for each reference period after, how far
 * that held period would have drifted from the reference. The sum
 * telescopes, so it is exact to a tick however long it runs. Its error at
 * 10 s, 1, 5, 10, 30, 60 and 120 minutes is collected per source: the RMS
 * against t gives f (the linear term) and D (the quadratic term) of
 * u0 + f.t + D.t^2/2 without ever leaving lock. A trial ends early on a gap
 * in the reference periods, a loss of lock or a change of source.
 */
#ifndef SYNC_TUNING_H
#define SYNC_TUNING_H

#include "sync.h"

#define TUNING_TRIALS         12U
#define TUNING_TRIAL_INTERVAL 600UL // epochs between trial starts
#define TUNING_CHECKPOINTS    7U    // 10 s .. 2 h

void sync_tuning_init(void);
// Once per epoch, after sync_update(), with its snapshot and result.
void sync_tuning_epoch(const TimingSnapshot *snapshot, const SyncUpdate *update);
// The minute's statistics and the trials so far; starts the next minute.
void sync_tuning_print(void);

#endif // SYNC_TUNING_H
