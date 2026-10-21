/*
 * One time source, as the synchroniser sees it.
 *
 * Every source (GNSS today; NTP via the ESP, an RTC, ... later) is reduced
 * to the same per-epoch report by a small adapter (source_gnss.c, ...). The
 * code here judges a report the same way whatever the source, and keeps the
 * running record that the state machine and the diagnostics read. Adding a
 * source never touches this file.
 *
 * A report describes the source's edge that belongs to our epoch: whether
 * there is one, the UTC second it marks, and how far the source itself
 * trusts its time. A source without an edge of its own (a time-of-day
 * message, say) leaves edge false and cannot discipline the phase.
 *
 * Edges come at two grades. Every PPS edge is watched by interrupt-on-change
 * (timing/pps_monitor.h): enough to show the pulses are arriving, to number
 * them for labelling and to place them within a microsecond or so. That is
 * evidence enough to judge a source GOOD. Only the source routed to the
 * IC1/IC2 capture also has its edge captured to the tick (captured), and
 * only a captured edge can steer the loop.
 */
#ifndef SYNC_SOURCE_H
#define SYNC_SOURCE_H

#include <stdbool.h>
#include <stdint.h>

#include "../platform/utc.h"

typedef struct
{
    bool present;            // answering at all
    bool time_valid;         // claims to know UTC
    bool edge;               // its edge was seen this epoch
    uint32_t edge_sequence;  // counts the source's edges; consecutive edges differ by 1
    bool captured;           // ... and captured by IC1/IC2: the loop can steer by it
    uint32_t edge_stamp;     // the edge on the capture timebase, ticks: the capture, or
                             // the watched timestamp less its latency
    int32_t edge_offset_ns;  // the edge minus the UTC second it marks, as far as the
                             // source knows (GNSS 0, NTP the ESP's own estimate)
    bool stepped;            // the source has just corrected its own time: a phase
                             // jump from here is the source's, not our frequency
    bool labelled;           // the UTC second of that edge is known
    uint32_t utc_seconds;    // ... as Unix seconds (an inserted leap second repeats :59)
    bool leap_second;        // ... and it is an inserted 23:59:60
    UtcLeap leap;            // announced leap second; change 0 if none
    uint32_t uncertainty_ns; // the source's own estimate of its time error; 0 unknown

    // An independent check of our epoch against this source, if it has one
    // (GNSS: TIM-TM2, the receiver timing our epoch output). Diagnostic only.
    bool cross_check;
    int32_t cross_check_ns; // our epoch minus the source's second
} SyncSourceReport;

// The verdict on one epoch's report: GOOD, or the first reason it is not.
typedef enum
{
    SYNC_CHECK_ABSENT = 0,   // not answering
    SYNC_CHECK_NO_TIME,      // does not know UTC (no fix, unlocked)
    SYNC_CHECK_NO_EDGE,      // no edge seen this epoch
    SYNC_CHECK_UNLABELLED,   // an edge, but its UTC second is unknown
    SYNC_CHECK_INACCURATE,   // its own uncertainty is above the limit
    SYNC_CHECK_INCONSISTENT, // label does not follow on from its last one
    SYNC_CHECK_GOOD,
    SYNC_CHECK_COUNT
} SyncCheck;

typedef struct
{
    SyncCheck check;         // this epoch's verdict
    SyncSourceReport report; // this epoch's report
    uint16_t good_run;       // consecutive GOOD epochs, saturating
    uint16_t usable_run;     // consecutive GOOD or INACCURATE epochs: everything but
                             // the accuracy limit met, enough to leave free run
    uint32_t last_good_epoch;

    // Set by the synchroniser each epoch (sync.c).
    uint32_t bound_ns;   // +/- bound on the source's time while GOOD or INACCURATE, else 0
    bool phase_valid;    // our epoch could be placed against its edge
    int32_t phase_ticks; // ... our epoch minus its UTC second, ticks

    // Label continuity, by the source's own edge count.
    bool has_label;
    uint32_t last_label;
    bool last_label_leap;
    uint32_t last_label_edge;
    uint32_t label_jumps; // labels that did not follow on

    // Diagnostics.
    uint32_t verdicts[SYNC_CHECK_COUNT]; // epochs per verdict
    uint32_t cross_checks;
    int32_t cross_check_min_ns;
    int32_t cross_check_max_ns;
} SyncSourceTracker;

void sync_source_reset(SyncSourceTracker *tracker);

// Judges one epoch's report and updates the record. max_uncertainty_ns 0
// accepts any uncertainty. epoch: our epoch sequence number.
SyncCheck sync_source_judge(SyncSourceTracker *tracker, const SyncSourceReport *report,
                            uint32_t max_uncertainty_ns, uint32_t epoch);

const char *sync_source_check_name(SyncCheck check);

#endif // SYNC_SOURCE_H
