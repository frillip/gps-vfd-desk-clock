#include "sync.h"

#include <stddef.h>
#include <string.h>

#include "../timing/timing_config.h"
#include "source_gnss.h"
#include "source_ntp.h"
#include "source_rtc.h"

// How a source is consulted and how far it is trusted.
typedef struct
{
    const char *name;
    // Fills the report for this epoch.
    void (*report)(const TimingSnapshot *snapshot, uint32_t now_ms, SyncSourceReport *report);
    // Called when the source is selected: route its edge to the capture
    // pair, if it is not there already. NULL if nothing to do.
    void (*select)(void);
    // Its self-reported uncertainty must be within qualify_ns to be taken
    // up, and stay within hold_ns while it is the selected source. The gap
    // keeps a source near the limit from flapping in and out.
    uint32_t qualify_ns;
    uint32_t hold_ns;
    // Its bound: uncertainty x bound_scale + bound_floor_ns.
    uint32_t bound_scale;
    uint32_t bound_floor_ns;
    // How the loop follows it, and the frequency error, ns/s, that it
    // leaves for holdover.
    const EpochProfile *profile;
    uint32_t holdover_frequency_ns;
    // False for a source of the time of day only, never to steer by.
    bool steers;
} SyncSourceDefinition;

static const EpochProfile ntp_profile = {
    "gentle",
    EPOCH_Q16_GAIN(1, SYNC_NTP_KP_DIVISOR),
    EPOCH_Q16_GAIN(1, SYNC_NTP_KI_DIVISOR),
    SYNC_NTP_LIMIT_TICKS,
    SYNC_NTP_LIMIT_TICKS,
    SYNC_NTP_GATE_TICKS,
    SYNC_NTP_REJECT_LIMIT,
    false,
};

// One row per SyncSourceId, in the same order.
static const SyncSourceDefinition sources[SYNC_SOURCE_COUNT] = {
    // GNSS: tAcc is tens of ns when healthy, ~1 us just after a fix or with
    // few satellites, while its PPS is still steady to a few hundred ns.
    // Even 100 us is ten times better than NTP over Wi-Fi (~1-10 ms).
    {"GNSS", source_gnss_report, source_gnss_select, SYNC_GNSS_QUALIFY_NS, SYNC_GNSS_HOLD_NS,
     SYNC_GNSS_TACC_SCALE, SYNC_CAPTURE_RESOLUTION_NS, &epoch_profile_precise,
     SYNC_HOLDOVER_FREQUENCY_GNSS, true},
    // NTP through the ESP: its bound, plus the jitter of its PPS.
    {"NTP", source_ntp_report, source_ntp_select, SYNC_NTP_QUALIFY_NS, SYNC_NTP_HOLD_NS, 1U,
     SYNC_NTP_EDGE_JITTER_NS, &ntp_profile, SYNC_HOLDOVER_FREQUENCY_NTP, true},
    // The RTC: the time of day, and its second placed by polling. Never
    // captured, so never steers; its bound is SOURCE_RTC_UNCERTAINTY_NS.
    {"RTC", source_rtc_report, NULL, SYNC_RTC_LIMIT_NS, SYNC_RTC_LIMIT_NS, 1U, 0U, NULL, 0U, false},
};

static SyncStatus status;

// A time set by hand, applied at the next epoch (sync_set_time).
static struct
{
    bool pending;
    uint32_t utc_seconds;
    uint32_t epoch;
    uint16_t slot_ms;
} manual_time;

// --------------------------------------------------------------------------
// Helpers
// --------------------------------------------------------------------------

static uint32_t saturate(uint64_t value)
{
    return value >= SYNC_BOUND_NONE ? SYNC_BOUND_NONE - 1U : (uint32_t)value;
}


// Nearest whole tick.
static int32_t ns_to_ticks(int32_t ns)
{
    int32_t half = (int32_t)(SYNC_CAPTURE_RESOLUTION_NS / 2U);
    return (ns >= 0 ? ns + half : ns - half) / (int32_t)SYNC_CAPTURE_RESOLUTION_NS;
}


static uint32_t ticks_to_ns(int32_t ticks)
{
    return (uint32_t)(ticks < 0 ? -ticks : ticks) * SYNC_CAPTURE_RESOLUTION_NS;
}


// Folded into [-half a second, +half a second).
static int32_t wrap_phase(int32_t phase)
{
    const int32_t half = (int32_t)(TIMING_NOMINAL_TICKS / 2U);

    if (phase >= half)
    {
        phase -= (int32_t)TIMING_NOMINAL_TICKS;
    }
    else if (phase < -half)
    {
        phase += (int32_t)TIMING_NOMINAL_TICKS;
    }
    return phase;
}


static bool is_tracking(void)
{
    return status.state == SYNC_ACQUIRE || status.state == SYNC_LOCKED;
}


static bool source_good(uint8_t source)
{
    return source < SYNC_SOURCE_COUNT && status.sources[source].check == SYNC_CHECK_GOOD;
}


// Fit to steer by: GOOD, or only INACCURATE if it was taken as a fallback.
static bool source_usable(uint8_t source)
{
    return source_good(source) ||
           (source < SYNC_SOURCE_COUNT && source == status.source && status.fallback &&
            status.sources[source].check == SYNC_CHECK_INACCURATE);
}


// Everything but the accuracy limit met, for SYNC_QUALIFY_EPOCHS.
static bool source_fallback(uint8_t source)
{
    return status.sources[source].usable_run >= SYNC_QUALIFY_EPOCHS;
}


// --------------------------------------------------------------------------
// Source evaluation
// --------------------------------------------------------------------------

static void judge_sources(const TimingSnapshot *snapshot, uint32_t now_ms)
{
    uint8_t index;
    SyncSourceReport report;

    for (index = 0; index < SYNC_SOURCE_COUNT; index++)
    {
        const SyncSourceDefinition *source = &sources[index];
        SyncSourceTracker *tracker = &status.sources[index];
        uint32_t limit = index == status.source ? source->hold_ns : source->qualify_ns;

        source->report(snapshot, now_ms, &report);
        (void)sync_source_judge(tracker, &report, limit, snapshot->timer.sequence);
        tracker->bound_ns =
            tracker->check == SYNC_CHECK_GOOD || tracker->check == SYNC_CHECK_INACCURATE
                ? saturate((uint64_t)report.uncertainty_ns * source->bound_scale +
                           source->bound_floor_ns)
                : 0U;
        // Where our epoch is against this source, from whichever edge it has.
        tracker->phase_valid = report.edge && snapshot->capture.epoch_capture_valid;
        tracker->phase_ticks =
            tracker->phase_valid
                ? wrap_phase((int32_t)(snapshot->capture.epoch_capture - report.edge_stamp) +
                             ns_to_ticks(report.edge_offset_ns))
                : 0;
    }
}


// u0 + f.t + D.t^2/2, D in ppt/s: D.t^2/2 ns = D x t^2 / 2000.
static void update_holdover_bound(void)
{
    SyncHoldover *holdover = &status.holdover;
    uint64_t age = holdover->age;

    if (!holdover->ready)
    {
        holdover->bound_ns = SYNC_BOUND_NONE;
        return;
    }
    holdover->bound_ns = saturate((uint64_t)holdover->base_ns + holdover->frequency_ns * age +
                                  (SYNC_HOLDOVER_DRIFT_PPT_PER_S * age * age) / 2000U);
}


bool sync_source_steers(uint8_t source)
{
    return source < SYNC_SOURCE_COUNT && sources[source].steers;
}


uint32_t sync_candidate_bound(uint8_t candidate)
{
    if (candidate == SYNC_CANDIDATE_HOLDOVER)
    {
        // Only while it would still count as synchronised.
        return status.holdover.ready && status.holdover.bound_ns <= SYNC_SYNCHRONISED_LIMIT_NS
                   ? status.holdover.bound_ns
                   : SYNC_BOUND_NONE;
    }
    if (candidate >= SYNC_SOURCE_COUNT || !sources[candidate].steers)
    {
        return SYNC_BOUND_NONE;
    }
    // The one in use, while fit to steer by.
    if (candidate == status.source && is_tracking())
    {
        return source_usable(candidate) ? status.sources[candidate].bound_ns : SYNC_BOUND_NONE;
    }
    // Any other qualifies by staying GOOD.
    if (!source_good(candidate) || status.sources[candidate].good_run < SYNC_QUALIFY_EPOCHS)
    {
        return SYNC_BOUND_NONE;
    }
    return status.sources[candidate].bound_ns;
}


// The candidate source with the lowest bound, other than except, or
// SYNC_SOURCE_NONE. Ties go to the earlier source.
static uint8_t best_source(uint8_t except)
{
    uint8_t best = SYNC_SOURCE_NONE;
    uint32_t best_bound = SYNC_BOUND_NONE;
    uint8_t index;

    for (index = 0; index < SYNC_SOURCE_COUNT; index++)
    {
        uint32_t bound = sync_candidate_bound(index);
        if (index != except && bound < best_bound)
        {
            best = index;
            best_bound = bound;
        }
    }
    return best;
}


// The fallback with the lowest bound, or SYNC_SOURCE_NONE.
static uint8_t best_fallback(void)
{
    uint8_t best = SYNC_SOURCE_NONE;
    uint32_t best_bound = SYNC_BOUND_NONE;
    uint8_t index;

    for (index = 0; index < SYNC_SOURCE_COUNT; index++)
    {
        if (sources[index].steers && source_fallback(index) &&
            status.sources[index].bound_ns < best_bound)
        {
            best = index;
            best_bound = status.sources[index].bound_ns;
        }
    }
    return best;
}


// The bound of whatever is in use now.
static uint32_t current_bound(void)
{
    switch (status.state)
    {
        case SYNC_ACQUIRE:
        case SYNC_LOCKED: return sync_candidate_bound(status.source);
        case SYNC_HOLDOVER: return status.holdover.bound_ns;
        default: return SYNC_BOUND_NONE;
    }
}


// True once challenger has beaten the incumbent's bound by SYNC_SWITCH_RATIO
// for SYNC_SWITCH_EPOCHS in a row.
static bool challenge(uint8_t challenger, uint32_t incumbent_bound)
{
    uint32_t bound = sync_candidate_bound(challenger);

    if (challenger == SYNC_SOURCE_NONE || bound == SYNC_BOUND_NONE ||
        (uint64_t)bound * SYNC_SWITCH_RATIO >= incumbent_bound)
    {
        status.challenger = SYNC_SOURCE_NONE;
        status.challenge_run = 0;
        return false;
    }
    if (challenger != status.challenger)
    {
        status.challenger = challenger;
        status.challenge_run = 0;
    }
    if (status.challenge_run < UINT16_MAX)
    {
        status.challenge_run++;
    }
    return status.challenge_run >= SYNC_SWITCH_EPOCHS;
}


// The best candidate other than the one in use, for the reports.
static void rank_second(void)
{
    uint8_t current = SYNC_SOURCE_NONE;
    uint8_t candidate;

    if (status.state == SYNC_HOLDOVER)
    {
        current = SYNC_CANDIDATE_HOLDOVER;
    }
    else if (is_tracking())
    {
        current = status.source;
    }
    status.second = SYNC_SOURCE_NONE;
    status.second_bound_ns = SYNC_BOUND_NONE;
    for (candidate = 0; candidate <= SYNC_CANDIDATE_HOLDOVER; candidate++)
    {
        uint32_t bound = sync_candidate_bound(candidate);
        if (candidate != current && candidate != SYNC_SOURCE_NONE && bound < status.second_bound_ns)
        {
            status.second = candidate;
            status.second_bound_ns = bound;
        }
    }
}


uint8_t sync_candidate_rank(uint8_t candidate)
{
    uint32_t bound = sync_candidate_bound(candidate);
    uint8_t rank = 1;
    uint8_t other;

    if (bound == SYNC_BOUND_NONE || candidate == SYNC_SOURCE_NONE)
    {
        return 0;
    }
    for (other = 0; other <= SYNC_CANDIDATE_HOLDOVER; other++)
    {
        uint32_t other_bound = sync_candidate_bound(other);
        if (other != SYNC_SOURCE_NONE && other != candidate &&
            (other_bound < bound || (other_bound == bound && other < candidate)))
        {
            rank++;
        }
    }
    return rank;
}


// --------------------------------------------------------------------------
// State machine
// --------------------------------------------------------------------------

static void enter(SyncState state, uint8_t source, SyncReason reason, uint32_t epoch)
{
    SyncTransition *entry = &status.history[status.transitions % SYNC_HISTORY_LENGTH];

    entry->epoch = epoch;
    entry->from = status.state;
    entry->to = state;
    entry->source = source;
    entry->reason = (uint8_t)reason;
    entry->verdict =
        (uint8_t)(status.source < SYNC_SOURCE_COUNT ? status.sources[status.source].check
                                                    : SYNC_CHECK_ABSENT);
    status.transitions++;

    if (source != status.source)
    {
        status.fallback = false;
    }
    status.state = state;
    status.source = source;
    status.state_epochs = 0;
    status.challenger = SYNC_SOURCE_NONE;
    status.challenge_run = 0;
    if (state != SYNC_HOLDOVER)
    {
        status.holdover_epochs = 0;
    }
}


/* Starts on source with its loop profile. If the loop is tracking and our
 * epoch is already within half the profile's gate of the source (judged
 * from its watched edge, as it is not captured yet), it slews onto it with
 * no step; otherwise it acquires from scratch, as the epoch may be anywhere
 * relative to the edge. */
static void acquire(uint8_t source, SyncReason reason, uint32_t epoch, bool fallback)
{
    const SyncSourceDefinition *definition = &sources[source];
    const SyncSourceTracker *tracker = &status.sources[source];
    bool slew = false;

    if ((status.state == SYNC_LOCKED || status.state == SYNC_HOLDOVER) && tracker->phase_valid &&
        epoch_control_state()->state == EPOCH_STATE_TRACK)
    {
        int32_t limit = definition->profile->gate_ticks / 2;
        slew = tracker->phase_ticks <= limit && tracker->phase_ticks >= -limit;
    }
    if (definition->select != NULL)
    {
        definition->select();
    }
    epoch_control_restart(definition->profile, slew);
    enter(SYNC_ACQUIRE, source, reason, epoch);
    status.fallback = fallback;
    status.steered = true;
}


/* At start-up, still free running and with nothing yet to steer by: puts
 * the epoch on the second of a time-of-day source (the RTC) the first
 * epoch it is GOOD, in one interval, so the seconds shown change with its
 * seconds. Once only: from then on the epoch is left to the sources that
 * steer. */
static void align_to_time_of_day(void)
{
    uint8_t index;

    if (status.state != SYNC_FREE_RUN || status.aligned || status.steered)
    {
        return;
    }
    for (index = 0; index < SYNC_SOURCE_COUNT; index++)
    {
        const SyncSourceTracker *tracker = &status.sources[index];

        if (!sources[index].steers && tracker->check == SYNC_CHECK_GOOD && tracker->phase_valid)
        {
            status.aligned = true;
            status.aligned_to = index;
            status.aligned_ticks = -tracker->phase_ticks;
            epoch_control_free_run_step(status.aligned_ticks);
            return;
        }
    }
}


// Still to be aligned onto a time-of-day source that has a time but not
// yet its second (the RTC being searched for), and not for long yet.
static bool time_of_day_pending(void)
{
    uint8_t index;

    if (status.aligned || status.steered || status.epochs >= SYNC_ALIGN_WAIT_EPOCHS)
    {
        return false;
    }
    for (index = 0; index < SYNC_SOURCE_COUNT; index++)
    {
        const SyncSourceReport *report = &status.sources[index].report;

        if (!sources[index].steers && report->present && report->time_valid && !report->edge)
        {
            return true;
        }
    }
    return false;
}


// Transitions driven by the sources, before the loop runs.
static void follow_sources(uint32_t epoch)
{
    uint8_t selected = status.source;
    uint8_t best;

    switch (status.state)
    {
        case SYNC_FREE_RUN:
            if (time_of_day_pending())
            {
                break; // the RTC first
            }
            best = best_source(SYNC_SOURCE_NONE);
            if (best != SYNC_SOURCE_NONE)
            {
                acquire(best, SYNC_REASON_QUALIFIED, epoch, false);
            }
            else
            {
                // Nothing accurate enough: still a clock. Take what there is.
                best = best_fallback();
                if (best != SYNC_SOURCE_NONE)
                {
                    acquire(best, SYNC_REASON_QUALIFIED, epoch, !source_good(best));
                }
            }
            break;

        case SYNC_ACQUIRE:
        case SYNC_LOCKED:
            if (source_usable(selected))
            {
                best = best_source(selected);
                if (challenge(best, current_bound()))
                {
                    acquire(best, SYNC_REASON_BETTER_SOURCE, epoch, false);
                }
            }
            else if (sync_candidate_bound(SYNC_CANDIDATE_HOLDOVER) != SYNC_BOUND_NONE)
            {
                // Holdover is at its best now: anything else must beat it
                // from there.
                enter(SYNC_HOLDOVER, status.holdover.source, SYNC_REASON_SOURCE_LOST, epoch);
            }
            else
            {
                // Not yet in phase with anything: nothing to hold over.
                best = best_source(selected);
                if (best != SYNC_SOURCE_NONE)
                {
                    acquire(best, SYNC_REASON_ALTERNATIVE, epoch, false);
                }
                else
                {
                    enter(SYNC_FREE_RUN, SYNC_SOURCE_NONE, SYNC_REASON_SOURCE_LOST, epoch);
                }
            }
            break;

        case SYNC_HOLDOVER:
            best = best_source(SYNC_SOURCE_NONE);
            if (source_usable(selected) && status.sources[selected].report.captured &&
                status.sources[selected].bound_ns <= status.holdover.bound_ns)
            {
                enter(SYNC_LOCKED, selected, SYNC_REASON_SOURCE_BACK, epoch);
            }
            else if (best == selected && !status.sources[selected].report.captured)
            {
                // Back, but its edge is no longer routed to the capture.
                acquire(selected, SYNC_REASON_SOURCE_BACK, epoch, false);
            }
            else if (challenge(best, status.holdover.bound_ns))
            {
                acquire(best, SYNC_REASON_ALTERNATIVE, epoch, false);
            }
            else if (status.holdover.bound_ns > SYNC_SYNCHRONISED_LIMIT_NS)
            {
                enter(SYNC_FREE_RUN, SYNC_SOURCE_NONE, SYNC_REASON_EXPIRED, epoch);
            }
            break;

        default: enter(SYNC_FREE_RUN, SYNC_SOURCE_NONE, SYNC_REASON_SOURCE_LOST, epoch); break;
    }
}


// Transitions driven by the loop, after it has run.
static void follow_loop(const TimingSnapshot *snapshot, const EpochUpdate *update)
{
    const EpochControl *loop = epoch_control_state();
    uint32_t epoch = snapshot->timer.sequence;

    // Locked once the loop has measured in TRACK after its step: the epoch
    // it entered TRACK still measured the edge before the step.
    if (status.state == SYNC_ACQUIRE && loop->state == EPOCH_STATE_TRACK &&
        loop->last_resync != epoch && update->kind == EPOCH_UPDATE_MEASURED)
    {
        enter(SYNC_LOCKED, status.source, SYNC_REASON_TRACKING, epoch);
    }
    else if (status.state == SYNC_LOCKED && loop->state != EPOCH_STATE_TRACK)
    {
        enter(SYNC_ACQUIRE, status.source, SYNC_REASON_PHASE_LOST, epoch);
    }
}


// Each epoch steered while locked restarts holdover from that epoch's bound;
// any other epoch ages it.
static void follow_holdover(const EpochUpdate *update)
{
    SyncHoldover *holdover = &status.holdover;

    if (status.state == SYNC_LOCKED && update->kind == EPOCH_UPDATE_MEASURED)
    {
        holdover->ready = true;
        holdover->source = status.source;
        holdover->age = 0;
        holdover->base_ns = saturate((uint64_t)status.sources[status.source].bound_ns +
                                     ticks_to_ns(update->utc_offset_ticks));
        holdover->frequency_ns = sources[status.source].holdover_frequency_ns;
    }
    else if (holdover->ready && holdover->age < UINT32_MAX)
    {
        holdover->age++;
    }
}


// --------------------------------------------------------------------------
// Time of day
// --------------------------------------------------------------------------

// The label of our epoch from a report: its edge's label, or the next
// second if our epoch is nearer that (before the loop has stepped onto it).
static uint32_t label_epoch(const TimingSnapshot *snapshot, const SyncSourceReport *report,
                            const UtcLeap *leap, bool *label_leap)
{
    int32_t raw = (int32_t)(snapshot->capture.epoch_capture - report->edge_stamp) +
                  ns_to_ticks(report->edge_offset_ns);
    uint32_t label = report->utc_seconds;

    *label_leap = report->leap_second;
    if (snapshot->capture.epoch_capture_valid && raw >= (int32_t)(TIMING_NOMINAL_TICKS / 2U))
    {
        label = utc_advance(label, *label_leap, 1U, leap->change != 0 ? leap : NULL, label_leap);
    }
    return label;
}


// The time of day before any source is in use: from the best labelled
// source that meets everything but its accuracy limit. A clock should show
// the time as soon as it has one; steering waits for the source to qualify.
static void seed_time(const TimingSnapshot *snapshot)
{
    uint8_t best = SYNC_SOURCE_NONE;
    uint32_t best_bound = SYNC_BOUND_NONE;
    uint8_t index;
    SyncTime *time = &status.time;

    for (index = 0; index < SYNC_SOURCE_COUNT; index++)
    {
        const SyncSourceTracker *tracker = &status.sources[index];
        if ((tracker->check == SYNC_CHECK_GOOD || tracker->check == SYNC_CHECK_INACCURATE) &&
            tracker->report.labelled && tracker->bound_ns < best_bound)
        {
            best = index;
            best_bound = tracker->bound_ns;
        }
    }
    if (best == SYNC_SOURCE_NONE)
    {
        return;
    }
    time->leap = status.sources[best].report.leap.change != 0 ? status.sources[best].report.leap
                                                              : status.link_leap;
    time->utc_seconds =
        label_epoch(snapshot, &status.sources[best].report, &time->leap, &time->leap_second);
    time->known = true;
    time->source = best;
}


/* The UTC second of our epoch. While a source is in use, its label for the
 * edge paired with our epoch; that edge is up to a second before it, so
 * until the loop has stepped onto it, our epoch may be nearer the next one.
 * Otherwise the last label counted on, through any announced leap second,
 * or until there is one, a label from any source fit to give it
 * (seed_time). A label that disagrees with the count once the loop was
 * already locked is a correction: the source's time moved. */
static void keep_time(const TimingSnapshot *snapshot, bool usable, bool was_locked)
{
    SyncTime *time = &status.time;
    bool counted_leap;
    uint32_t counted = utc_advance(time->utc_seconds, time->leap_second, 1U,
                                   time->leap.change != 0 ? &time->leap : NULL, &counted_leap);

    if (usable)
    {
        const SyncSourceReport *report = &status.sources[status.source].report;
        bool label_leap;
        uint32_t label;

        time->leap = report->leap.change != 0 ? report->leap : status.link_leap;
        label = label_epoch(snapshot, report, &time->leap, &label_leap);
        if (was_locked && time->known && (label != counted || label_leap != counted_leap))
        {
            time->corrections++;
        }
        time->utc_seconds = label;
        time->leap_second = label_leap;
        time->known = true;
        time->source = status.source;
    }
    else if (time->known)
    {
        time->utc_seconds = counted;
        time->leap_second = counted_leap;
        if (status.link_leap.change != 0)
        {
            time->leap = status.link_leap;
        }
    }
    else
    {
        seed_time(snapshot);
    }
}


// --------------------------------------------------------------------------
// Interface
// --------------------------------------------------------------------------

void sync_init(void)
{
    uint8_t index;

    memset(&status, 0, sizeof(status));
    manual_time.pending = false;
    status.state = SYNC_FREE_RUN;
    status.source = SYNC_SOURCE_NONE;
    status.second = SYNC_SOURCE_NONE;
    status.second_bound_ns = SYNC_BOUND_NONE;
    status.challenger = SYNC_SOURCE_NONE;
    status.holdover.bound_ns = SYNC_BOUND_NONE;
    status.time.source = SYNC_SOURCE_NONE;
    status.aligned_to = SYNC_SOURCE_NONE;
    for (index = 0; index < SYNC_SOURCE_COUNT; index++)
    {
        sync_source_reset(&status.sources[index]);
    }
    epoch_control_init();
}


/* The time set by hand, n epochs after the call (normally 1): this epoch,
 * n s less slot_ms after the moment set, is labelled the nearest second,
 * and the interval after the next is stretched (by slot_ms, at most half a
 * second) or shortened (by the rest of the second) so that the epochs fall
 * on the seconds from it. keep_time then counts on to the label. */
static void apply_manual_time(uint32_t epoch)
{
    uint32_t elapsed = epoch - manual_time.epoch;
    bool later_half = manual_time.slot_ms > 500U;
    SyncTime *time = &status.time;

    if (!manual_time.pending)
    {
        return;
    }
    manual_time.pending = false;
    if (status.state != SYNC_FREE_RUN || status.steered)
    {
        return;
    }
    epoch_control_free_run_step(
        later_half ? -(int32_t)((1000U - manual_time.slot_ms) * TIMING_TICKS_PER_MS)
                   : (int32_t)(manual_time.slot_ms * TIMING_TICKS_PER_MS));
    if (time->known)
    {
        time->corrections++;
    }
    time->known = true;
    time->leap_second = false;
    time->utc_seconds = manual_time.utc_seconds + elapsed - (later_half ? 1U : 0U) - 1U;
    time->source = SYNC_SOURCE_NONE;
    status.aligned = true; // the RTC is not aligned to any more
}


SyncUpdate sync_update(const TimingSnapshot *snapshot, uint32_t now_ms)
{
    SyncUpdate update;
    uint32_t transitions = status.transitions;
    bool was_locked = status.state == SYNC_LOCKED || status.state == SYNC_HOLDOVER;
    int32_t offset_ticks = 0;

    judge_sources(snapshot, now_ms);
    apply_manual_time(snapshot->timer.sequence);
    // A fallback that has met its accuracy limit is a source like any other.
    if (status.fallback && source_good(status.source))
    {
        status.fallback = false;
    }
    update_holdover_bound();
    // Before a source is taken up in the same epoch: the RTC first.
    align_to_time_of_day();
    follow_sources(snapshot->timer.sequence);

    update.usable = is_tracking() && source_usable(status.source) &&
                    status.sources[status.source].report.captured;
    if (update.usable)
    {
        offset_ticks = ns_to_ticks(status.sources[status.source].report.edge_offset_ns);
    }
    if (is_tracking() && status.source < SYNC_SOURCE_COUNT &&
        status.sources[status.source].report.stepped)
    {
        epoch_control_source_stepped();
    }
    update.loop = epoch_control_update(snapshot, update.usable, offset_ticks);
    follow_loop(snapshot, &update.loop);
    follow_holdover(&update.loop);
    // Again, for the reports and TIME-AUTH: as of this epoch, after any
    // steering in it has set a new base or ageing has moved it on.
    update_holdover_bound();
    keep_time(snapshot, update.usable, was_locked);
    rank_second();

    status.epochs++;
    status.phase_valid = update.loop.phase_valid;
    status.phase_ticks = update.loop.utc_offset_ticks;
    status.state_epochs++;
    if (status.state == SYNC_HOLDOVER)
    {
        status.holdover_epochs++;
    }
    update.transitioned = status.transitions != transitions;
    return update;
}


bool sync_set_time(uint32_t utc_seconds, uint32_t epoch_sequence, uint16_t slot_ms)
{
    if (status.state != SYNC_FREE_RUN || status.steered || slot_ms >= 1000U)
    {
        return false;
    }
    manual_time.pending = true;
    manual_time.utc_seconds = utc_seconds;
    manual_time.epoch = epoch_sequence;
    manual_time.slot_ms = slot_ms;
    return true;
}


const SyncStatus *sync_status(void)
{
    return &status;
}


void sync_set_leap(const UtcLeap *leap)
{
    status.link_leap = *leap;
    if (leap->change == 0)
    {
        status.link_leap.event_utc = 0;
    }
}


bool sync_uncertainty(uint32_t *uncertainty_ns)
{
    if (!status.time.known)
    {
        return false;
    }
    if (status.state == SYNC_HOLDOVER && status.holdover.ready)
    {
        *uncertainty_ns = status.holdover.bound_ns;
        return true;
    }
    if (is_tracking() && source_usable(status.source) && status.phase_valid)
    {
        *uncertainty_ns = saturate((uint64_t)status.sources[status.source].bound_ns +
                                   ticks_to_ns(status.phase_ticks));
        return true;
    }
    // Free running on the time of a time-of-day source that is still GOOD.
    if (status.state == SYNC_FREE_RUN && status.time.source < SYNC_SOURCE_COUNT &&
        !sources[status.time.source].steers && source_good(status.time.source))
    {
        *uncertainty_ns = status.sources[status.time.source].bound_ns;
        return true;
    }
    return false;
}


bool sync_is_synchronised(void)
{
    return (status.state == SYNC_LOCKED || status.state == SYNC_HOLDOVER) &&
           current_bound() <= SYNC_SYNCHRONISED_LIMIT_NS;
}


const SyncTransition *sync_last_transition(void)
{
    if (status.transitions == 0U)
    {
        return NULL;
    }
    return &status.history[(status.transitions - 1U) % SYNC_HISTORY_LENGTH];
}


const char *sync_state_name(SyncState state)
{
    switch (state)
    {
        case SYNC_FREE_RUN: return "FREE_RUN";
        case SYNC_ACQUIRE: return "ACQUIRE";
        case SYNC_LOCKED: return "LOCKED";
        case SYNC_HOLDOVER: return "HOLDOVER";
        default: return "?";
    }
}


const char *sync_source_name(uint8_t source)
{
    if (source == SYNC_CANDIDATE_HOLDOVER)
    {
        return "HOLD";
    }
    return source < SYNC_SOURCE_COUNT ? sources[source].name : "none";
}


const char *sync_reason_name(uint8_t reason)
{
    switch (reason)
    {
        case SYNC_REASON_QUALIFIED: return "qualified";
        case SYNC_REASON_TRACKING: return "tracking";
        case SYNC_REASON_PHASE_LOST: return "phase lost";
        case SYNC_REASON_SOURCE_LOST: return "source lost";
        case SYNC_REASON_SOURCE_BACK: return "source back";
        case SYNC_REASON_ALTERNATIVE: return "alternative";
        case SYNC_REASON_BETTER_SOURCE: return "better source";
        case SYNC_REASON_EXPIRED: return "holdover expired";
        default: return "?";
    }
}
