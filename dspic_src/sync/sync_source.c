#include "sync_source.h"

#include <stddef.h>
#include <string.h>

// Most fundamental failure first: the first that applies is the verdict.
static SyncCheck classify(const SyncSourceReport *report, uint32_t max_uncertainty_ns)
{
    if (!report->present)
    {
        return SYNC_CHECK_ABSENT;
    }
    if (!report->time_valid)
    {
        return SYNC_CHECK_NO_TIME;
    }
    if (!report->edge)
    {
        return SYNC_CHECK_NO_EDGE;
    }
    if (!report->labelled)
    {
        return SYNC_CHECK_UNLABELLED;
    }
    if (max_uncertainty_ns != 0U && report->uncertainty_ns > max_uncertainty_ns)
    {
        return SYNC_CHECK_INACCURATE;
    }
    return SYNC_CHECK_GOOD;
}


/* A label must follow on from the source's last one by the number of its
 * edges in between, counted through any announced leap second. If it does
 * not, the source's time has jumped: this epoch is not trusted, and the new
 * label becomes the one to follow, so a source that has really changed its
 * time is believed again from the next epoch. Only edges can be counted:
 * after an epoch without one, the next label starts afresh. */
static bool label_follows_on(SyncSourceTracker *tracker, const SyncSourceReport *report)
{
    bool follows = true;

    if (tracker->has_label)
    {
        bool expected_leap;
        uint32_t edges = report->edge_sequence - tracker->last_label_edge;
        uint32_t expected =
            utc_advance(tracker->last_label, tracker->last_label_leap, edges,
                        report->leap.change != 0 ? &report->leap : NULL, &expected_leap);
        follows = expected == report->utc_seconds && expected_leap == report->leap_second;
    }
    if (!follows)
    {
        tracker->label_jumps++;
    }
    tracker->has_label = true;
    tracker->last_label = report->utc_seconds;
    tracker->last_label_leap = report->leap_second;
    tracker->last_label_edge = report->edge_sequence;
    return follows;
}


static void record_cross_check(SyncSourceTracker *tracker, const SyncSourceReport *report)
{
    if (!report->cross_check)
    {
        return;
    }
    if (tracker->cross_checks == 0U || report->cross_check_ns < tracker->cross_check_min_ns)
    {
        tracker->cross_check_min_ns = report->cross_check_ns;
    }
    if (tracker->cross_checks == 0U || report->cross_check_ns > tracker->cross_check_max_ns)
    {
        tracker->cross_check_max_ns = report->cross_check_ns;
    }
    tracker->cross_checks++;
}


void sync_source_reset(SyncSourceTracker *tracker)
{
    if (tracker != NULL)
    {
        memset(tracker, 0, sizeof(*tracker));
        tracker->check = SYNC_CHECK_ABSENT;
    }
}


SyncCheck sync_source_judge(SyncSourceTracker *tracker, const SyncSourceReport *report,
                            uint32_t max_uncertainty_ns, uint32_t epoch)
{
    SyncCheck check = classify(report, max_uncertainty_ns);

    if (!report->present || !report->edge)
    {
        tracker->has_label = false;
    }

    // Continuity is followed for every labelled edge, so a label is judged
    // against the last one even across epochs that failed for other reasons.
    if (report->present && report->time_valid && report->edge && report->labelled &&
        !label_follows_on(tracker, report) && check == SYNC_CHECK_GOOD)
    {
        check = SYNC_CHECK_INCONSISTENT;
    }

    tracker->report = *report;
    tracker->check = check;
    tracker->verdicts[check]++;
    if (check == SYNC_CHECK_GOOD)
    {
        if (tracker->good_run < UINT16_MAX)
        {
            tracker->good_run++;
        }
        tracker->last_good_epoch = epoch;
    }
    else
    {
        tracker->good_run = 0;
    }
    if (check == SYNC_CHECK_GOOD || check == SYNC_CHECK_INACCURATE)
    {
        if (tracker->usable_run < UINT16_MAX)
        {
            tracker->usable_run++;
        }
    }
    else
    {
        tracker->usable_run = 0;
    }
    record_cross_check(tracker, report);
    return check;
}


const char *sync_source_check_name(SyncCheck check)
{
    switch (check)
    {
        case SYNC_CHECK_ABSENT: return "absent";
        case SYNC_CHECK_NO_TIME: return "no time";
        case SYNC_CHECK_NO_EDGE: return "no edge";
        case SYNC_CHECK_UNLABELLED: return "unlabelled";
        case SYNC_CHECK_INACCURATE: return "inaccurate";
        case SYNC_CHECK_INCONSISTENT: return "inconsistent";
        case SYNC_CHECK_GOOD: return "good";
        default: return "?";
    }
}
