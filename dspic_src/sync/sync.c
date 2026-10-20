#include "sync.h"

#include <stddef.h>
#include <string.h>

#include "../timing/timing_config.h"
#include "source_gnss.h"

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
} SyncSourceDefinition;

// One row per SyncSourceId, in the same order.
static const SyncSourceDefinition sources[SYNC_SOURCE_COUNT] = {
    // GNSS: tAcc is tens of ns when healthy, ~1 us just after a fix or with
    // few satellites, while its PPS is still steady to a few hundred ns.
    // Even 100 us is ten times better than NTP over Wi-Fi (~1-10 ms), the
    // next source down. Beyond that the receiver is coasting, and holdover
    // on our own oscillator does as well.
    {"GNSS", source_gnss_report, NULL, SYNC_GNSS_QUALIFY_NS, SYNC_GNSS_HOLD_NS},
};

static SyncStatus status;

// --------------------------------------------------------------------------
// Source evaluation
// --------------------------------------------------------------------------

static void judge_sources(const TimingSnapshot *snapshot, uint32_t now_ms)
{
    uint8_t index;
    SyncSourceReport report;

    for (index = 0; index < SYNC_SOURCE_COUNT; index++)
    {
        sources[index].report(snapshot, now_ms, &report);
        uint32_t limit =
            index == status.source ? sources[index].hold_ns : sources[index].qualify_ns;
        (void)sync_source_judge(&status.sources[index], &report, limit, snapshot->timer.sequence);
    }
}


static bool source_good(uint8_t source)
{
    return source < SYNC_SOURCE_COUNT && status.sources[source].check == SYNC_CHECK_GOOD;
}


// The best-ranked source GOOD for at least run epochs, ranked above limit
// (SYNC_SOURCE_NONE: any), or SYNC_SOURCE_NONE.
static uint8_t best_source(uint16_t run, uint8_t limit)
{
    uint8_t index;

    for (index = 0; index < limit && index < SYNC_SOURCE_COUNT; index++)
    {
        if (status.sources[index].check == SYNC_CHECK_GOOD && status.sources[index].good_run >= run)
        {
            return index;
        }
    }
    return SYNC_SOURCE_NONE;
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

    status.state = state;
    status.source = source;
    status.state_epochs = 0;
    if (state != SYNC_HOLDOVER)
    {
        status.holdover_epochs = 0;
    }
}


// Starts acquiring source: always from scratch, as the epoch may be anywhere
// relative to its edge.
static void acquire(uint8_t source, SyncReason reason, uint32_t epoch)
{
    if (sources[source].select != NULL)
    {
        sources[source].select();
    }
    epoch_control_restart();
    enter(SYNC_ACQUIRE, source, reason, epoch);
}


// Transitions driven by the sources, before the loop runs.
static void follow_sources(uint32_t epoch)
{
    bool selected_good = source_good(status.source);
    uint8_t qualified = best_source(SYNC_QUALIFY_EPOCHS, SYNC_SOURCE_NONE);
    uint8_t better = best_source(SYNC_SWITCH_EPOCHS, status.source);

    switch (status.state)
    {
        case SYNC_FREE_RUN:
            if (qualified != SYNC_SOURCE_NONE)
            {
                acquire(qualified, SYNC_REASON_QUALIFIED, epoch);
            }
            break;

        case SYNC_ACQUIRE:
        case SYNC_LOCKED:
            if (better != SYNC_SOURCE_NONE)
            {
                acquire(better, SYNC_REASON_BETTER_SOURCE, epoch);
            }
            else if (selected_good)
            {
                // Carry on.
            }
            else if (status.state == SYNC_LOCKED)
            {
                enter(SYNC_HOLDOVER, status.source, SYNC_REASON_SOURCE_LOST, epoch);
            }
            else if (qualified != SYNC_SOURCE_NONE && qualified != status.source)
            {
                acquire(qualified, SYNC_REASON_ALTERNATIVE, epoch);
            }
            else
            {
                // Not yet in phase with it: nothing to hold over.
                enter(SYNC_FREE_RUN, SYNC_SOURCE_NONE, SYNC_REASON_SOURCE_LOST, epoch);
            }
            break;

        case SYNC_HOLDOVER:
            if (selected_good)
            {
                enter(SYNC_LOCKED, status.source, SYNC_REASON_SOURCE_BACK, epoch);
            }
            else if (qualified != SYNC_SOURCE_NONE)
            {
                acquire(qualified, SYNC_REASON_ALTERNATIVE, epoch);
            }
            else if (status.holdover_epochs >= SYNC_HOLDOVER_LIMIT_EPOCHS)
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


// --------------------------------------------------------------------------
// Time of day
// --------------------------------------------------------------------------

/* The UTC second of our epoch. While a source is in use, its label for the
 * edge paired with our epoch; that edge is up to a second before it, so
 * until the loop has stepped onto it, our epoch may be nearer the next one.
 * Otherwise the last label counted on, through any announced leap second.
 * A label that disagrees with the count once the loop was already locked is
 * a correction: the source's time moved. */
static void keep_time(const TimingSnapshot *snapshot, bool usable, bool was_locked)
{
    SyncTime *time = &status.time;
    bool counted_leap;
    uint32_t counted = utc_advance(time->utc_seconds, time->leap_second, 1U,
                                   time->leap.change != 0 ? &time->leap : NULL, &counted_leap);

    if (usable)
    {
        const SyncSourceReport *report = &status.sources[status.source].report;
        int32_t raw = (int32_t)(snapshot->capture.epoch_capture - snapshot->capture.gnss_capture);
        uint32_t label = report->utc_seconds;
        bool label_leap = report->leap_second;

        time->leap = report->leap.change != 0 ? report->leap : status.link_leap;
        if (snapshot->capture.epoch_capture_valid && raw >= (int32_t)(TIMING_NOMINAL_TICKS / 2U))
        {
            label = utc_advance(label, label_leap, 1U, time->leap.change != 0 ? &time->leap : NULL,
                                &label_leap);
        }
        if (was_locked && time->known && (label != counted || label_leap != counted_leap))
        {
            time->corrections++;
        }
        time->utc_seconds = label;
        time->leap_second = label_leap;
        time->known = true;
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
}


// --------------------------------------------------------------------------
// Interface
// --------------------------------------------------------------------------

void sync_init(void)
{
    uint8_t index;

    memset(&status, 0, sizeof(status));
    status.state = SYNC_FREE_RUN;
    status.source = SYNC_SOURCE_NONE;
    for (index = 0; index < SYNC_SOURCE_COUNT; index++)
    {
        sync_source_reset(&status.sources[index]);
    }
    epoch_control_init();
}


SyncUpdate sync_update(const TimingSnapshot *snapshot, uint32_t now_ms)
{
    SyncUpdate update;
    uint32_t transitions = status.transitions;
    bool was_locked = status.state == SYNC_LOCKED || status.state == SYNC_HOLDOVER;

    judge_sources(snapshot, now_ms);
    follow_sources(snapshot->timer.sequence);

    update.usable =
        (status.state == SYNC_ACQUIRE || status.state == SYNC_LOCKED) && source_good(status.source);
    update.loop = epoch_control_update(snapshot, update.usable);
    follow_loop(snapshot, &update.loop);
    keep_time(snapshot, update.usable, was_locked);

    status.epochs++;
    status.phase_valid = update.loop.phase_valid;
    status.phase_ticks = update.loop.utc_offset_ticks;
    if (update.usable)
    {
        status.steered_uncertainty_ns = status.sources[status.source].report.uncertainty_ns;
    }
    status.state_epochs++;
    if (status.state == SYNC_HOLDOVER)
    {
        status.holdover_epochs++;
    }
    update.transitioned = status.transitions != transitions;
    return update;
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
    uint32_t bound;

    if (!status.time.known || status.source >= SYNC_SOURCE_COUNT || status.state == SYNC_FREE_RUN)
    {
        return false;
    }
    bound = SYNC_CAPTURE_RESOLUTION_NS;
    if (status.state == SYNC_HOLDOVER)
    {
        // The source's word no longer counts: what it was when last used.
        bound +=
            status.steered_uncertainty_ns + status.holdover_epochs * SYNC_HOLDOVER_DRIFT_NS_PER_S;
    }
    else if (status.phase_valid)
    {
        int32_t phase = status.phase_ticks;
        uint32_t magnitude = (uint32_t)(phase < 0 ? -phase : phase);
        bound += status.sources[status.source].report.uncertainty_ns +
                 magnitude * SYNC_CAPTURE_RESOLUTION_NS;
    }
    else
    {
        return false;
    }
    *uncertainty_ns = bound;
    return true;
}


bool sync_is_synchronised(void)
{
    return status.state == SYNC_LOCKED || status.state == SYNC_HOLDOVER;
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
