/*
 * Navigation epoch assembly.
 *
 * A u-blox receiver emits one NAV-STATUS, NAV-TIMEUTC, NAV-CLOCK and
 * NAV-POSLLH per navigation epoch, all carrying the same time of week. Each
 * message arrives separately, so judging "is GNSS time usable" from the
 * latest of each can mix a fix from one second with a time from another.
 * The assembler groups messages by time of week and publishes a snapshot
 * only once NAV-STATUS and NAV-TIMEUTC for the same epoch are both present.
 * NAV-CLOCK and NAV-POSLLH join the snapshot when they match its epoch.
 * NAV-EOE closes the epoch: the published snapshot is then final, and one
 * still missing NAV-STATUS or NAV-TIMEUTC is abandoned at once rather than
 * when the next epoch starts.
 *
 * PPS association: the epoch at time T is reported shortly after the PPS
 * edge at T. When an epoch completes, the most recent PPS edge is taken as
 * the one it labels, provided that edge is recent and has not already been
 * labelled by an earlier epoch. Without that guard a missing pulse (no PPS is
 * output while the receiver is unlocked) would label the previous edge and
 * put the clock one second out.
 *
 * Pure logic: no UART, no globals. Suitable for host tests.
 */
#ifndef GNSS_EPOCH_H
#define GNSS_EPOCH_H
#include <stdint.h>
#include <stdbool.h>
#include "ubx_decode.h"

// An epoch time further than this from a whole second means the navigation
// rate is not aligned to the second, so it cannot label a PPS edge. The
// receiver's own clock bias is normally well under 1 ms.
#define GNSS_EPOCH_ALIGNMENT_LIMIT_NS 100000000L
// The labelling message must arrive within this long of its PPS edge.
#define GNSS_PPS_ASSOCIATION_WINDOW_MS 950U

// Snapshot of the most recent PPS edge, taken atomically by the caller.
typedef struct
{
    uint32_t sequence;    // increments once per captured edge
    uint32_t observed_ms; // millisecond tick when the edge was captured
    bool valid;           // false until the first edge
} GnssPpsReference;

typedef struct
{
    uint32_t time_of_week_ms;
    uint32_t completed_ms; // millisecond tick when the epoch completed

    // Derived from NAV-STATUS and NAV-TIMEUTC.
    bool time_valid; // fix OK, UTC valid, aligned to the second
    bool fix_ok;
    uint32_t utc_seconds;          // Unix seconds of the nearest whole second
    bool leap_second;              // the epoch is 23:59:60; utc_seconds holds :59
    int32_t offset_from_second_ns; // epoch time minus utc_seconds

    // PPS association, see above.
    bool pps_associated;
    uint32_t pps_sequence;

    // The decoded messages the snapshot was built from.
    UbxNavStatus status;
    UbxNavTimeutc time_utc;
    bool has_clock;
    UbxNavClock clock;
    bool has_position;
    UbxNavPosllh position;

    bool ended; // NAV-EOE seen: no more messages for this epoch
} GnssEpoch;

typedef struct
{
    uint32_t published;
    uint32_t abandoned;    // superseded before NAV-STATUS + NAV-TIMEUTC
    uint32_t unassociated; // published without a PPS edge to label
    uint32_t invalid_time; // published with time_valid false
} GnssEpochStats;

typedef struct
{
    GnssEpoch building;
    uint8_t building_parts; // EPOCH_PART_* bits
    bool building_active;

    GnssEpoch published;
    bool has_published;

    uint32_t last_labelled_pps_sequence;
    bool has_labelled_pps;

    GnssEpochStats stats;
} GnssEpochAssembler;

void gnss_epoch_init(GnssEpochAssembler *assembler);
// Each returns true if the call published a new epoch.
bool gnss_epoch_add_status(GnssEpochAssembler *assembler, const UbxNavStatus *status,
                           uint32_t now_ms, GnssPpsReference pps);
bool gnss_epoch_add_time_utc(GnssEpochAssembler *assembler, const UbxNavTimeutc *time_utc,
                             uint32_t now_ms, GnssPpsReference pps);
void gnss_epoch_add_clock(GnssEpochAssembler *assembler, const UbxNavClock *clock);
void gnss_epoch_add_position(GnssEpochAssembler *assembler, const UbxNavPosllh *position);
// NAV-EOE for this time of week.
void gnss_epoch_end(GnssEpochAssembler *assembler, const UbxNavEoe *end);

// Most recent published epoch, or NULL if none yet.
const GnssEpoch *gnss_epoch_latest(const GnssEpochAssembler *assembler);

#endif
