/*
 * The RTC (PCF8563, drivers/pcf8563.h) as a time source, and keeping it set.
 *
 * Its second. The RTC holds UTC to the second and has no edge on V2, so its
 * second is found by polling: back-to-back reads (a few hundred
 * microseconds each), each stamped on the capture timebase when its I2C
 * start completed, until the seconds count goes up; the increment lies
 * between the last read before it and the first after (a search). That
 * places the RTC's second to well within a millisecond; from there it is
 * counted on, and searched for again at every check. The first search
 * starts as soon as the chip has been read, so the RTC is the first source
 * at start-up.
 *
 * Its rate. Each search after the first, with no setting in between and at
 * least SOURCE_RTC_RATE_MIN_SECONDS on, shows how far its second has moved
 * against where it was counted on to: that corrects its rate, and its
 * second is counted on at that length rather than the nominal second. A
 * search SOURCE_RTC_RATE_EPOCHS after each setting gives the first. (V2's
 * PCF8563s were measured at about 150 ppm fast: 13 s a day.)
 *
 * As a source (source_rtc_report) it reports that second as a watched edge
 * labelled with the RTC's time, never captured: the time of day at
 * start-up, and the epoch's first placing (sync.h).
 *
 * Its time against UTC. Each time its second is found while the clock is
 * synchronised, where it started against our epoch is measured: an
 * anchor, with a bound of the clock's own plus half the search's
 * resolution. (An aligned write's confirmation is one; a plain write, by
 * command or after failed aligned writes, makes one of SOURCE_RTC_
 * UNCERTAINTY_NS with nothing measured; any write begun voids it until
 * then.) From the anchor and the rate, its second now is predicted: where
 * it starts against UTC's, the rate times the seconds since added to the
 * anchor's (the rate as against true seconds: the rate learnt is on the
 * capture timebase, the PIC's own crystal, so the control loop's estimate
 * of that is taken off it, source_rtc_set_oscillator), which is reported as the edge's offset (edge_offset_ns), whole
 * seconds of it moving the label; so at start-up the epoch is placed on
 * UTC's second as predicted, not on the RTC's. The bound grows from the
 * anchor's by SOURCE_RTC_ALLOWANCE_PPM of the seconds since with a rate,
 * SOURCE_RTC_RATE_LIMIT_PPM without. With no anchor it is SOURCE_RTC_
 * UNCERTAINTY_NS. The anchor, the rate and the last setting are kept over a
 * reset by the settings (source_rtc_kept, source_rtc_restore): what makes
 * all this worth having is the time since the clock was last powered.
 *
 * Keeping it set. Once the clock is synchronised, after SOURCE_RTC_SETTLE_
 * EPOCHS, then often enough that at its rate it drifts no further than
 * SOURCE_RTC_DRIFT_LIMIT_MS from ours (between SOURCE_RTC_CHECK_MIN_EPOCHS
 * and SOURCE_RTC_CHECK_EPOCHS apart), and soon after any correction of the
 * clock's time, the RTC's second is searched for and compared with ours. If it is out by more than SOURCE_RTC_RESYNC_MS, or
 * its time cannot be trusted (VL, STOP), it is set, aligned:
 *
 *   1. load a time with STOP set, which holds the prescaler in reset;
 *   2. release STOP so that the chip's first increment, which follows the
 *      release by PCF8563_START_DELAY_NS, lands on one of our epochs: the
 *      release is timed against the capture timebase, the main loop
 *      spinning for its last SOURCE_RTC_SPIN_TICKS;
 *   3. search for the RTC's second, which confirms its label and measures
 *      how far from our epoch it landed. That error corrects the next
 *      release (release_correction_ticks), so a start delay that differs
 *      from the data sheet's is learnt.
 *
 * An attempt that misses its moment (a busy main loop, the bus in use) is
 * tried again the next epoch, up to SOURCE_RTC_SYNC_ATTEMPTS; after that
 * the time is written plainly, to the nearest second, so the RTC is never
 * left stopped.
 *
 * Nothing is written to the RTC unless the clock is synchronised, and never
 * near a leap second, which the RTC cannot hold: after one it is a second
 * ahead until the next check sets it.
 */
#ifndef SOURCE_RTC_H
#define SOURCE_RTC_H

#include <stdbool.h>
#include <stdint.h>

#include "../timing/timing.h"
#include "sync_source.h"

#define SOURCE_RTC_TICKS_PER_US TIMING_TICKS_PER_US

// Its bound with no anchor (nothing measured against the clock): it holds
// UTC to the second.
#ifndef SOURCE_RTC_UNCERTAINTY_NS
#define SOURCE_RTC_UNCERTAINTY_NS 999000000UL // 999 ms
#endif
// Drift beyond the rate learnt, for the bound: its crystal at another
// temperature while the clock is off (a tuning fork, -0.035 ppm/C^2), and
// ageing.
#define SOURCE_RTC_ALLOWANCE_PPM 10L
// The largest bound reported: one more is SYNC_BOUND_NONE.
#define SOURCE_RTC_BOUND_MAX_NS 0xFFFFFFFEUL

// A search places the increment between two reads at most this far apart;
// wider (the main loop was busy), it waits for the next increment.
#define SOURCE_RTC_SEARCH_RESOLUTION_TICKS (2000UL * SOURCE_RTC_TICKS_PER_US) // 2 ms
// ... and gives up after this many epochs.
#define SOURCE_RTC_SEARCH_EPOCHS 4U
// The registers are taken as the chip is addressed: about one byte after
// the start condition at 400 kHz.
#define SOURCE_RTC_READ_LATCH_TICKS (25UL * SOURCE_RTC_TICKS_PER_US)

// Searched for, and checked against our time, at most this far apart...
#ifndef SOURCE_RTC_CHECK_EPOCHS
#define SOURCE_RTC_CHECK_EPOCHS 21600UL // 6 h
#endif
// ... and at least this far, sooner the faster it drifts, so it stays
// within SOURCE_RTC_DRIFT_LIMIT_MS: half its bound.
#define SOURCE_RTC_CHECK_MIN_EPOCHS 600UL // 10 min
#define SOURCE_RTC_DRIFT_LIMIT_MS   500UL

// Its rate: learnt from searches at least this far apart (by its own
// count), the first this long after each setting; a rate beyond the limit
// is not believed.
#define SOURCE_RTC_RATE_MIN_SECONDS 300UL
#ifndef SOURCE_RTC_RATE_EPOCHS
#define SOURCE_RTC_RATE_EPOCHS 600UL
#endif
#define SOURCE_RTC_RATE_LIMIT_PPM 500L
// A second measured longer ago than this is no longer reported.
#define SOURCE_RTC_SECOND_MAX_AGE_EPOCHS (SOURCE_RTC_CHECK_EPOCHS + 3600UL)
// After the clock synchronises, or its time is corrected, before checking.
#define SOURCE_RTC_SETTLE_EPOCHS 60U
// After a failed search or write, before trying again.
#define SOURCE_RTC_RETRY_EPOCHS 60U
// Set the RTC when its second is further than this from ours.
#ifndef SOURCE_RTC_RESYNC_MS
#define SOURCE_RTC_RESYNC_MS 10L
#endif

// Aligned writing.
#define SOURCE_RTC_SYNC_ATTEMPTS 3U
// An aligned write that lands within this of our epoch is done.
#define SOURCE_RTC_ALIGN_TOLERANCE_TICKS (2000L * (int32_t)SOURCE_RTC_TICKS_PER_US) // 2 ms
// From submitting the release to STOP clearing: start, address and
// register bytes, and the data byte that clears it, at 400 kHz.
#define SOURCE_RTC_RELEASE_LATENCY_TICKS (70L * (int32_t)SOURCE_RTC_TICKS_PER_US)
// The release is never planned closer than this after an epoch (the load
// goes first), spun for over its last SOURCE_RTC_SPIN_TICKS, and given up
// if the spin starts more than SOURCE_RTC_LATE_TICKS late.
#define SOURCE_RTC_RELEASE_EARLIEST_TICKS (50UL * 1000UL * SOURCE_RTC_TICKS_PER_US)  // 50 ms
#define SOURCE_RTC_SPIN_TICKS             (2000L * (int32_t)SOURCE_RTC_TICKS_PER_US) // 2 ms
#define SOURCE_RTC_LATE_TICKS             (100L * (int32_t)SOURCE_RTC_TICKS_PER_US)  // 100 us
// The learnt correction to the start delay is kept within this.
#define SOURCE_RTC_CORRECTION_LIMIT_TICKS (600L * 1000L * (int32_t)SOURCE_RTC_TICKS_PER_US)

typedef enum
{
    SOURCE_RTC_IDLE = 0,
    SOURCE_RTC_SEARCHING,  // for the RTC's second
    SOURCE_RTC_LOADING,    // aligned write: the time, stopped
    SOURCE_RTC_WAITING,    // ... for the moment to release
    SOURCE_RTC_RELEASING,  // ... STOP being cleared
    SOURCE_RTC_CONFIRMING, // ... searching for where its second landed
} SourceRtcActivity;

// Its second against UTC, measured (or set), from which its time is
// predicted (see above).
typedef struct
{
    bool valid;
    bool aligned;         // measured against the synchronised clock; else a plain write
    uint32_t utc_seconds; // the RTC's label of that second
    int64_t offset_ns;    // where it started minus UTC's second, + = the RTC late
    uint32_t bound_ns;    // +/- on offset_ns
} SourceRtcAnchor;

// What is kept over a reset (settings.c).
typedef struct
{
    SourceRtcAnchor anchor;
    bool rate_valid;
    int32_t rate_q8;
    bool last_write_valid;
    bool last_write_aligned;
    uint32_t last_write_utc;
} SourceRtcKept;

typedef struct
{
    SourceRtcActivity activity;

    // The RTC's second, as last found and counted on since.
    bool second_valid;
    uint32_t second_stamp;     // capture timebase of its latest increment
    uint32_t second_label;     // the UTC second it shows from then
    uint32_t second_edges;     // increments counted (SyncSourceReport.edge_sequence)
    uint32_t measured_epoch;   // our epoch when it was last found
    uint32_t resolution_ticks; // ... between the reads either side

    // Its rate: its second on the capture timebase less the nominal second,
    // Q8 ticks (+ = slow), from the searches since a baseline.
    bool rate_valid;
    int32_t rate_q8;
    uint32_t rate_updates;
    bool baseline_valid;     // no setting since the last search
    uint32_t baseline_stamp; // ... where it found the second
    uint32_t baseline_label;

    // Against our clock, at the last search made with our time known.
    bool offset_valid;
    int32_t offset_ms; // RTC minus our clock, + = RTC ahead (saturated at +/-23 days)
    uint32_t offset_epoch;

    // Its second against UTC.
    SourceRtcAnchor anchor;
    uint32_t kept_changes; // anchor, rate or last write: what is kept
    uint32_t rtc_changes;  // writes of the RTC begun: the anchor no longer holds
    bool restored;         // what is kept came from the store at start-up

    // When it was last read, by our clock.
    bool last_read_valid;
    uint32_t last_read_utc;

    // Keeping it set.
    uint32_t next_check_epoch;
    uint8_t attempts;                 // of the aligned write in progress
    int32_t release_correction_ticks; // learnt, added to the start delay
    bool last_write_valid;
    bool last_write_aligned;        // ... else written plainly
    int32_t last_write_error_ticks; // where the increment landed, minus our epoch (aligned)
    uint32_t last_write_utc;        // the second it was set to
    uint32_t last_write_epoch;
    bool rate_due; // a search for the rate, SOURCE_RTC_RATE_EPOCHS after a setting
    uint32_t rate_due_epoch;

    uint32_t searches;
    uint32_t searches_failed;
    uint32_t checks;
    uint32_t writes;        // aligned writes confirmed
    uint32_t writes_failed; // attempts that missed or landed out
    uint32_t plain_writes;  // after SOURCE_RTC_SYNC_ATTEMPTS, or by command
    uint32_t late_releases; // the spin started too late
} SourceRtcState;

void source_rtc_init(void);

// The adapter for sync.c: fills report for the epoch in snapshot.
void source_rtc_report(const TimingSnapshot *snapshot, uint32_t now_ms, SyncSourceReport *report);

// Once per epoch, after sync_update(): checks and aligned writes. time: our
// epoch's time, from the synchroniser (its SyncStatus time);
// synchronised: sync_is_synchronised(); bound_ns: sync_uncertainty(), or
// SYNC_BOUND_NONE without one.
void source_rtc_epoch(const TimingSnapshot *snapshot, const SyncTime *time, bool synchronised,
                      uint32_t bound_ns);

// Every pass of the main loop, after pcf8563_service(): searches, and the
// release. Returns at once when there is nothing to do.
void source_rtc_poll(void);

// A setting by command (SML SET_RTC): written as given, to the nearest
// second, then the RTC's second is found again. False if the RTC is absent
// or busy, or the time is before 2000.
bool source_rtc_set(uint32_t utc_seconds);
// True once after a source_rtc_set(): its read-back (the next
// pcf8563_report_due()) answers a command, so is printed whatever the debug.
bool source_rtc_take_set(void);

// The PIC's oscillator, as the control loop has it (kalman_phase_q16: a
// true second on the capture timebase less the nominal, Q16 ticks), each
// epoch before sync_update(): taken off the RTC's rate in predicting it.
void source_rtc_set_oscillator(int32_t offset_q16);

// What is kept over a reset, and putting it back at start-up (after
// source_rtc_init, before the first epoch).
void source_rtc_kept(SourceRtcKept *kept);
void source_rtc_restore(const SourceRtcKept *kept);

// The RTC's time, as it counts it: its latest second (found and counted
// on), else as last read. False if it has no valid time.
bool source_rtc_reckoning(uint32_t *utc_seconds);
// Its bound now (see above). False if it has no valid time.
bool source_rtc_bound(uint32_t *bound_ns);

const SourceRtcState *source_rtc_state(void);
// Its rate in tenths of a ppm, + = fast; 0 until one has been learnt.
int32_t source_rtc_rate_ppm10(void);
// Epochs between checks at the rate learnt.
uint32_t source_rtc_check_interval(void);
const char *source_rtc_activity_name(SourceRtcActivity activity);
// The RTC's second and its rate, when the clock last set it and the next
// check, its bound and drift: three lines. verbose: the learnt start delay
// and the counts.
void source_rtc_print(bool verbose);

#endif // SOURCE_RTC_H
