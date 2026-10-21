/*
 * The RTC as a time source, and keeping it set. See source_rtc.h.
 *
 * Who runs what: source_rtc_report() from sync_update() each epoch (counts
 * the RTC's second on); source_rtc_epoch() after it (decides searches,
 * checks and writes, acts on their results, with our epoch's time to hand);
 * source_rtc_poll() every pass of the main loop (keeps a search's reads
 * going, and times the release). Everything is foreground.
 */
#include "source_rtc.h"

#include <stdio.h>
#include <string.h>

#include "../drivers/pcf8563.h"
#include "../timing/timing_config.h"
#include "sync.h"

#define SECOND_TICKS ((uint32_t)TIMING_NOMINAL_TICKS)
// ns to ticks: 25 ns a tick.
#define START_DELAY_TICKS ((int32_t)(PCF8563_START_DELAY_NS / (1000UL / SOURCE_RTC_TICKS_PER_US)))
#define TICKS_PER_MS      ((int32_t)TIMING_TICKS_PER_MS)

typedef enum
{
    PURPOSE_MEASURE = 0, // find the RTC's second: at start-up, after a setting, when stale
    PURPOSE_CHECK,       // ... and compare it with ours, perhaps to set it
    PURPOSE_CONFIRM      // ... where an aligned write landed
} SearchPurpose;

static SourceRtcState state;

// Our latest epoch (source_rtc_epoch).
static uint32_t epoch_sequence;
static bool epoch_valid; // its capture
static uint32_t epoch_stamp;

// A search.
static SearchPurpose purpose;
static uint32_t search_deadline;
static bool sample_valid;
static uint32_t sample_utc;
static uint32_t sample_stamp;
static uint32_t reads_seen;
static bool search_done; // ... ended: found (second_valid) or not, acted on at the epoch
static bool search_found;
static uint32_t measure_due_epoch; // the next start-up or stale-second search

// An aligned write.
static bool write_wanted; // begin one (or the next attempt) at the next epoch
static bool plain_wanted; // give up: write plainly at the next epoch
static uint32_t release_target;
static uint32_t expect_stamp; // the epoch its first increment should land on
static uint32_t expect_label; // ... showing this second
static uint32_t writes_seen;
static uint32_t releases_seen;
static uint32_t activity_epoch; // the epoch the current step began

// An RTC found stopped: released, then to be read back.
static bool stopped_released;

// Checks.
static bool was_synchronised;
static uint32_t corrections_seen;

// --------------------------------------------------------------------------
// Helpers
// --------------------------------------------------------------------------

static bool epoch_reached(uint32_t epoch)
{
    return (int32_t)(epoch_sequence - epoch) >= 0;
}


// The next check no later than epoch.
static void check_by(uint32_t epoch)
{
    if ((int32_t)(epoch - state.next_check_epoch) < 0)
    {
        state.next_check_epoch = epoch;
    }
}


static bool writing(void)
{
    return state.activity == SOURCE_RTC_LOADING || state.activity == SOURCE_RTC_WAITING ||
           state.activity == SOURCE_RTC_RELEASING || state.activity == SOURCE_RTC_CONFIRMING;
}


static void set_activity(SourceRtcActivity activity)
{
    state.activity = activity;
    activity_epoch = epoch_sequence;
    // The periodic reads would only get in the way.
    pcf8563_set_auto_read(activity == SOURCE_RTC_IDLE);
}


static int32_t clamp_correction(int32_t ticks)
{
    if (ticks > SOURCE_RTC_CORRECTION_LIMIT_TICKS)
    {
        return SOURCE_RTC_CORRECTION_LIMIT_TICKS;
    }
    if (ticks < -SOURCE_RTC_CORRECTION_LIMIT_TICKS)
    {
        return -SOURCE_RTC_CORRECTION_LIMIT_TICKS;
    }
    return ticks;
}


static void print_us(int32_t ticks)
{
    int32_t us = ticks / (int32_t)SOURCE_RTC_TICKS_PER_US;
    uint32_t size = (uint32_t)(us < 0 ? -us : us);

    printf("%c%lu.%03lu ms", us < 0 ? '-' : '+', (unsigned long)(size / 1000U),
           (unsigned long)(size % 1000U));
}


// --------------------------------------------------------------------------
// Searching for the RTC's second
// --------------------------------------------------------------------------

static void start_search(SearchPurpose why, uint32_t epochs)
{
    purpose = why;
    search_deadline = epoch_sequence + epochs;
    sample_valid = false;
    reads_seen = pcf8563_status()->reads;
    search_done = false;
    state.searches++;
    set_activity(why == PURPOSE_CONFIRM ? SOURCE_RTC_CONFIRMING : SOURCE_RTC_SEARCHING);
    (void)pcf8563_read();
}


static void end_search(bool found)
{
    search_done = true;
    search_found = found;
    if (!found)
    {
        state.searches_failed++;
    }
    set_activity(SOURCE_RTC_IDLE);
}


// The increment lies between the last read before it and the first after.
static void found(uint32_t stamp, uint32_t label, uint32_t resolution)
{
    if (state.second_valid)
    {
        // Counted from the second it replaces, so the edges run on.
        int32_t apart = (int32_t)(stamp - state.second_stamp);
        int32_t seconds = (apart + (int32_t)(SECOND_TICKS / 2U)) / (int32_t)SECOND_TICKS;
        state.second_edges += (uint32_t)(seconds > 0 ? seconds : 0);
    }
    else
    {
        state.second_edges++;
    }
    state.second_valid = true;
    state.second_stamp = stamp;
    state.second_label = label;
    state.measured_epoch = epoch_sequence;
    state.resolution_ticks = resolution;
    end_search(true);
}


static void take_sample(const Pcf8563Status *rtc)
{
    if (!rtc->calendar_valid || rtc->stopped)
    {
        sample_valid = false;
        return;
    }
    if (sample_valid && rtc->utc_seconds == sample_utc + 1U)
    {
        uint32_t gap = rtc->read_stamp - sample_stamp;

        if (gap <= SOURCE_RTC_SEARCH_RESOLUTION_TICKS)
        {
            found(sample_stamp + gap / 2U + SOURCE_RTC_READ_LATCH_TICKS, rtc->utc_seconds, gap);
            return;
        }
    }
    sample_valid = true;
    sample_utc = rtc->utc_seconds;
    sample_stamp = rtc->read_stamp;
}


static void keep_searching(void)
{
    const Pcf8563Status *rtc = pcf8563_status();

    if (rtc->reads != reads_seen)
    {
        reads_seen = rtc->reads;
        take_sample(rtc);
    }
    if (state.activity != SOURCE_RTC_SEARCHING && state.activity != SOURCE_RTC_CONFIRMING)
    {
        return; // found
    }
    if (!pcf8563_busy())
    {
        if (!rtc->present)
        {
            end_search(false);
            return;
        }
        (void)pcf8563_read();
    }
}


// --------------------------------------------------------------------------
// Aligned writing
// --------------------------------------------------------------------------

static void abort_write(void)
{
    state.writes_failed++;
    state.attempts++;
    set_activity(SOURCE_RTC_IDLE);
    if (state.attempts < SOURCE_RTC_SYNC_ATTEMPTS)
    {
        write_wanted = true;
    }
    else
    {
        plain_wanted = true; // never leave it stopped
    }
}


// Loads the time that the RTC should show until its first increment, which
// is to land on our epoch n seconds on: released n seconds less the start
// delay after this epoch, at least SOURCE_RTC_RELEASE_EARLIEST_TICKS in.
static void begin_write(const SyncStatus *sync)
{
    int32_t delay =
        START_DELAY_TICKS + state.release_correction_ticks + SOURCE_RTC_RELEASE_LATENCY_TICKS;
    uint32_t n = 1;
    uint32_t label = sync->time.utc_seconds;

    write_wanted = false;
    while ((int32_t)(n * SECOND_TICKS) - delay < (int32_t)SOURCE_RTC_RELEASE_EARLIEST_TICKS)
    {
        n++;
    }
    // The RTC cannot hold a leap second: not across one.
    if (sync->time.leap.change != 0 && sync->time.leap.event_utc > label &&
        sync->time.leap.event_utc - label <= n + 1U)
    {
        write_wanted = true; // after it
        return;
    }
    release_target = epoch_stamp + n * SECOND_TICKS - (uint32_t)delay;
    expect_stamp = epoch_stamp + n * SECOND_TICKS;
    expect_label = label + n;
    writes_seen = pcf8563_status()->writes;
    set_activity(SOURCE_RTC_LOADING);
    if (!pcf8563_write_stopped(expect_label - 1U))
    {
        abort_write();
    }
}


// Spins for the last SOURCE_RTC_SPIN_TICKS before the release, so it goes
// out within microseconds of its moment; too late, it waits for another
// attempt.
static void wait_release(void)
{
    int32_t remaining = (int32_t)(release_target - pps_capture_timebase());

    if (remaining > SOURCE_RTC_SPIN_TICKS)
    {
        return;
    }
    if (remaining < -SOURCE_RTC_LATE_TICKS)
    {
        state.late_releases++;
        abort_write();
        return;
    }
    while ((int32_t)(release_target - pps_capture_timebase()) > 0)
    {
    }
    releases_seen = pcf8563_status()->releases;
    if (pcf8563_release())
    {
        set_activity(SOURCE_RTC_RELEASING);
    }
    else
    {
        abort_write();
    }
}


// --------------------------------------------------------------------------
// Results, at the epoch
// --------------------------------------------------------------------------

// RTC minus our clock at the RTC's latest increment, from our epoch now:
// whole seconds apart, less where its increment falls after our epoch.
// Saturated well before the arithmetic would overflow (24 days).
#define OFFSET_LIMIT_S 2000000L

static void measure_offset(const SyncStatus *sync)
{
    int32_t seconds;
    int32_t delta_ms;

    state.offset_valid = false;
    if (!sync->time.known || !epoch_valid || !state.second_valid)
    {
        return;
    }
    seconds = (int32_t)(state.second_label - sync->time.utc_seconds);
    delta_ms = (int32_t)(state.second_stamp - epoch_stamp) / TICKS_PER_MS;
    if (seconds > OFFSET_LIMIT_S)
    {
        state.offset_ms = OFFSET_LIMIT_S * 1000L;
    }
    else if (seconds < -OFFSET_LIMIT_S)
    {
        state.offset_ms = -OFFSET_LIMIT_S * 1000L;
    }
    else
    {
        state.offset_ms = seconds * 1000L - delta_ms;
    }
    state.offset_valid = true;
    state.offset_epoch = epoch_sequence;
}


static void confirmed(void)
{
    int32_t error = (int32_t)(state.second_stamp - expect_stamp);
    bool label_ok = state.second_label == expect_label;

    bool within =
        error <= SOURCE_RTC_ALIGN_TOLERANCE_TICKS && error >= -SOURCE_RTC_ALIGN_TOLERANCE_TICKS;

    if (label_ok)
    {
        // Learnt either way, so the next release allows for it: all of a
        // miss, half of what is left within tolerance, where the search's
        // own resolution (a read's length) is most of it.
        state.release_correction_ticks =
            clamp_correction(state.release_correction_ticks + (within ? error / 2 : error));
        state.last_write_error_ticks = error;
    }
    if (label_ok && within)
    {
        state.writes++;
        state.attempts = 0;
        state.last_write_valid = true;
        state.last_write_utc = expect_label;
        state.next_check_epoch = epoch_sequence + SOURCE_RTC_CHECK_EPOCHS;
        printf("RTC: set, its second ");
        print_us(error);
        printf(" from the epoch (start delay correction ");
        print_us(state.release_correction_ticks);
        printf(")\n");
        return;
    }
    printf("RTC: aligned write landed ");
    if (label_ok)
    {
        print_us(error);
        printf(" from the epoch");
    }
    else
    {
        printf("on the wrong second (%lu, not %lu)", (unsigned long)state.second_label,
               (unsigned long)expect_label);
    }
    printf(", attempt %u\n", (unsigned)(state.attempts + 1U));
    state.writes_failed++;
    state.attempts++;
    if (state.attempts < SOURCE_RTC_SYNC_ATTEMPTS)
    {
        write_wanted = true;
    }
    else if (!label_ok)
    {
        plain_wanted = true;
    }
    else
    {
        state.attempts = 0;
        state.next_check_epoch = epoch_sequence + SOURCE_RTC_CHECK_EPOCHS;
    }
}


static void search_result(const SyncStatus *sync, bool synchronised)
{
    const Pcf8563Status *rtc = pcf8563_status();

    search_done = false;
    if (!search_found)
    {
        printf("RTC: second not found\n");
        if (purpose == PURPOSE_CONFIRM)
        {
            abort_write();
        }
        else
        {
            measure_due_epoch = epoch_sequence + SOURCE_RTC_RETRY_EPOCHS;
            if (purpose == PURPOSE_CHECK)
            {
                state.next_check_epoch = epoch_sequence + SOURCE_RTC_RETRY_EPOCHS;
            }
        }
        return;
    }
    measure_offset(sync);
    if (purpose == PURPOSE_CONFIRM)
    {
        confirmed();
        return;
    }
    printf("RTC: second found to %lu us",
           (unsigned long)(state.resolution_ticks / SOURCE_RTC_TICKS_PER_US));
    if (state.offset_valid)
    {
        printf(", %+ld ms against the clock", (long)state.offset_ms);
    }
    printf("\n");
    if (purpose != PURPOSE_CHECK)
    {
        return;
    }
    state.next_check_epoch = epoch_sequence + SOURCE_RTC_CHECK_EPOCHS;
    if (synchronised && state.offset_valid &&
        (!rtc->time_valid || state.offset_ms > SOURCE_RTC_RESYNC_MS ||
         state.offset_ms < -SOURCE_RTC_RESYNC_MS))
    {
        state.attempts = 0;
        write_wanted = true;
    }
}


// --------------------------------------------------------------------------
// Interface
// --------------------------------------------------------------------------

void source_rtc_init(void)
{
    memset(&state, 0, sizeof(state));
    state.next_check_epoch = SOURCE_RTC_CHECK_EPOCHS;
    epoch_sequence = 0;
    epoch_valid = false;
    search_done = false;
    write_wanted = false;
    plain_wanted = false;
    measure_due_epoch = 0;
    was_synchronised = false;
    corrections_seen = 0;
    stopped_released = false;
    set_activity(SOURCE_RTC_IDLE);
}


void source_rtc_report(const TimingSnapshot *snapshot, uint32_t now_ms, SyncSourceReport *report)
{
    const Pcf8563Status *rtc = pcf8563_status();
    const PpsCaptureSample *capture = &snapshot->capture;
    uint8_t steps = 0;

    (void)now_ms;
    memset(report, 0, sizeof(*report));
    report->present = rtc->present;
    report->time_valid = rtc->time_valid && !writing();
    report->uncertainty_ns = SYNC_RTC_UNCERTAINTY_NS;
    if (!state.second_valid || !capture->epoch_capture_valid)
    {
        return;
    }
    // Counted on to the latest increment at or before our epoch.
    while ((int32_t)(capture->epoch_capture - (state.second_stamp + SECOND_TICKS)) >= 0)
    {
        state.second_stamp += SECOND_TICKS;
        state.second_label++;
        state.second_edges++;
        if (++steps > 3U)
        {
            state.second_valid = false; // lost track: find it again
            return;
        }
    }
    if (writing())
    {
        return;
    }
    report->edge = true;
    report->edge_sequence = state.second_edges;
    report->edge_stamp = state.second_stamp;
    report->labelled = true;
    report->utc_seconds = state.second_label;
}


void source_rtc_epoch(const TimingSnapshot *snapshot)
{
    const SyncStatus *sync = sync_status();
    const Pcf8563Status *rtc = pcf8563_status();
    bool synchronised = sync_is_synchronised() && sync->time.known && !sync->time.leap_second;

    epoch_sequence = snapshot->timer.sequence;
    epoch_valid = snapshot->capture.epoch_capture_valid;
    epoch_stamp = snapshot->capture.epoch_capture;

    if (state.second_valid &&
        epoch_sequence - state.measured_epoch > SOURCE_RTC_SECOND_MAX_AGE_EPOCHS)
    {
        state.second_valid = false;
    }

    // A search that ran out of time; a write step that never finished.
    if ((state.activity == SOURCE_RTC_SEARCHING || state.activity == SOURCE_RTC_CONFIRMING) &&
        epoch_reached(search_deadline))
    {
        end_search(false);
    }
    else if ((state.activity == SOURCE_RTC_LOADING || state.activity == SOURCE_RTC_WAITING ||
              state.activity == SOURCE_RTC_RELEASING) &&
             epoch_reached(activity_epoch + 4U))
    {
        abort_write();
    }
    if (search_done)
    {
        search_result(sync, synchronised);
    }

    // Check soon after the clock is synchronised, or its time corrected.
    if (synchronised && (!was_synchronised || sync->time.corrections != corrections_seen))
    {
        check_by(epoch_sequence + SOURCE_RTC_SETTLE_EPOCHS);
    }
    was_synchronised = synchronised;
    corrections_seen = sync->time.corrections;

    if (state.activity != SOURCE_RTC_IDLE || !rtc->present || pcf8563_busy())
    {
        return;
    }
    if (plain_wanted)
    {
        plain_wanted = false;
        state.attempts = 0;
        state.next_check_epoch = epoch_sequence + SOURCE_RTC_CHECK_EPOCHS;
        if (synchronised && pcf8563_write(sync->time.utc_seconds))
        {
            state.plain_writes++;
            state.second_valid = false;
            printf("RTC: aligned write failed: set to the nearest second\n");
        }
        return;
    }
    if (write_wanted)
    {
        if (synchronised && epoch_valid)
        {
            begin_write(sync);
        }
        else if (!synchronised)
        {
            write_wanted = false; // no time to give it: the next check decides
            state.attempts = 0;
        }
        return;
    }
    // Stopped, with no write of ours to follow: an aligned write cut short
    // (by a reset, or by losing synchronisation between attempts).
    // Let it run; its time is then stale by how long it was stopped, and
    // it is set at the next check once there is a time to give it.
    if (rtc->calendar_valid && rtc->stopped)
    {
        if (!stopped_released)
        {
            stopped_released = pcf8563_release();
            state.second_valid = false;
            if (stopped_released)
            {
                printf("RTC: found stopped: released\n");
            }
        }
        else
        {
            stopped_released = !pcf8563_read(); // see whether it runs now
        }
        return;
    }
    if (synchronised && epoch_reached(state.next_check_epoch))
    {
        state.checks++;
        if (!rtc->time_valid)
        {
            state.attempts = 0;
            write_wanted = true; // nothing to measure: set it
            return;
        }
        start_search(PURPOSE_CHECK, SOURCE_RTC_SEARCH_EPOCHS);
        return;
    }
    if (!state.second_valid && rtc->time_valid && epoch_reached(measure_due_epoch))
    {
        start_search(PURPOSE_MEASURE, SOURCE_RTC_SEARCH_EPOCHS);
    }
}


void source_rtc_poll(void)
{
    const Pcf8563Status *rtc;

    switch (state.activity)
    {
        case SOURCE_RTC_SEARCHING:
        case SOURCE_RTC_CONFIRMING: keep_searching(); break;
        case SOURCE_RTC_LOADING:
            if (!pcf8563_busy())
            {
                rtc = pcf8563_status();
                if (rtc->writes != writes_seen)
                {
                    set_activity(SOURCE_RTC_WAITING);
                }
                else
                {
                    abort_write();
                }
            }
            break;
        case SOURCE_RTC_WAITING: wait_release(); break;
        case SOURCE_RTC_RELEASING:
            if (!pcf8563_busy())
            {
                rtc = pcf8563_status();
                if (rtc->releases != releases_seen)
                {
                    start_search(PURPOSE_CONFIRM, SOURCE_RTC_SEARCH_EPOCHS);
                }
                else
                {
                    abort_write();
                }
            }
            break;
        default: break;
    }
}


bool source_rtc_set(uint32_t utc_seconds)
{
    if (state.activity != SOURCE_RTC_IDLE || !pcf8563_write(utc_seconds))
    {
        return false;
    }
    state.plain_writes++;
    state.second_valid = false; // found again once it is read back
    measure_due_epoch = epoch_sequence;
    write_wanted = false;
    plain_wanted = false;
    return true;
}


const SourceRtcState *source_rtc_state(void)
{
    return &state;
}


const char *source_rtc_activity_name(SourceRtcActivity activity)
{
    switch (activity)
    {
        case SOURCE_RTC_IDLE: return "idle";
        case SOURCE_RTC_SEARCHING: return "searching";
        case SOURCE_RTC_LOADING: return "loading";
        case SOURCE_RTC_WAITING: return "waiting to release";
        case SOURCE_RTC_RELEASING: return "releasing";
        case SOURCE_RTC_CONFIRMING: return "confirming";
        default: return "?";
    }
}


void source_rtc_print(void)
{
    printf("RTC sync: %s", source_rtc_activity_name(state.activity));
    if (state.second_valid)
    {
        printf(", second found %lu s ago to %lu us",
               (unsigned long)(epoch_sequence - state.measured_epoch),
               (unsigned long)(state.resolution_ticks / SOURCE_RTC_TICKS_PER_US));
    }
    else
    {
        printf(", second not found");
    }
    if (state.offset_valid)
    {
        printf(", %+ld ms against the clock %lu s ago", (long)state.offset_ms,
               (unsigned long)(epoch_sequence - state.offset_epoch));
    }
    printf("\n  ");
    if (state.last_write_valid)
    {
        printf("last set to the epoch of %lu, ", (unsigned long)state.last_write_utc);
        print_us(state.last_write_error_ticks);
        printf(" off; ");
    }
    printf("start delay correction ");
    print_us(state.release_correction_ticks);
    printf("; next check in %ld s; %lu searches (%lu failed), %lu checks, %lu set (%lu failed, "
           "%lu late), %lu plain\n",
           (long)(int32_t)(state.next_check_epoch - epoch_sequence), (unsigned long)state.searches,
           (unsigned long)state.searches_failed, (unsigned long)state.checks,
           (unsigned long)state.writes, (unsigned long)state.writes_failed,
           (unsigned long)state.late_releases, (unsigned long)state.plain_writes);
}
