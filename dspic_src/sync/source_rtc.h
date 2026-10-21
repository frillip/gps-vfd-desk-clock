/*
 * The RTC (PCF8563, drivers/pcf8563.h) as a time source, and keeping it set.
 *
 * Its second. The RTC holds UTC to the second and has no edge on V2, so its
 * second is found by polling: back-to-back reads (a few hundred
 * microseconds each), each stamped on the capture timebase when its I2C
 * start completed, until the seconds count goes up; the increment lies
 * between the last read before it and the first after (a search). That
 * places the RTC's second to well within a millisecond; from there it is
 * counted on at the nominal second, and searched for again every
 * SOURCE_RTC_CHECK_EPOCHS.
 *
 * As a source (source_rtc_report) it reports that second as a watched edge
 * labelled with the RTC's time, never captured, with a bound of
 * SYNC_RTC_UNCERTAINTY_NS: the time of day at start-up, and the epoch's
 * first placing (sync.h).
 *
 * Keeping it set. Once the clock is synchronised, after SOURCE_RTC_SETTLE_
 * EPOCHS, and every SOURCE_RTC_CHECK_EPOCHS from then (and soon after any
 * correction of the clock's time), the RTC's second is searched for and
 * compared with ours. If it is out by more than SOURCE_RTC_RESYNC_MS, or
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

// A search places the increment between two reads at most this far apart;
// wider (the main loop was busy), it waits for the next increment.
#define SOURCE_RTC_SEARCH_RESOLUTION_TICKS (2000UL * SOURCE_RTC_TICKS_PER_US) // 2 ms
// ... and gives up after this many epochs.
#define SOURCE_RTC_SEARCH_EPOCHS 4U
// The registers are taken as the chip is addressed: about one byte after
// the start condition at 400 kHz.
#define SOURCE_RTC_READ_LATCH_TICKS (25UL * SOURCE_RTC_TICKS_PER_US)

// Searched for, and checked against our time, this often.
#ifndef SOURCE_RTC_CHECK_EPOCHS
#define SOURCE_RTC_CHECK_EPOCHS 21600UL // 6 h
#endif
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

    // Against our clock, at the last search made with our time known.
    bool offset_valid;
    int32_t offset_ms; // RTC minus our clock, + = RTC ahead (saturated at +/-23 days)
    uint32_t offset_epoch;

    // Keeping it set.
    uint32_t next_check_epoch;
    uint8_t attempts;                 // of the aligned write in progress
    int32_t release_correction_ticks; // learnt, added to the start delay
    bool last_write_valid;
    int32_t last_write_error_ticks; // where the increment landed, minus our epoch
    uint32_t last_write_utc;        // the second it was aligned to

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

// Once per epoch, after sync_update(): checks and aligned writes, with our
// epoch's time from the synchroniser.
void source_rtc_epoch(const TimingSnapshot *snapshot);

// Every pass of the main loop, after pcf8563_service(): searches, and the
// release. Returns at once when there is nothing to do.
void source_rtc_poll(void);

// A setting by command (SML SET_RTC): written as given, to the nearest
// second, then the RTC's second is found again. False if the RTC is absent
// or busy, or the time is before 2000.
bool source_rtc_set(uint32_t utc_seconds);

const SourceRtcState *source_rtc_state(void);
const char *source_rtc_activity_name(SourceRtcActivity activity);
// The RTC's second and how it is kept: one or two lines.
void source_rtc_print(void);

#endif // SOURCE_RTC_H
