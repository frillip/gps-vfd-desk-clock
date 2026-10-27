/*
 * NTP as a time source, through the ESP: the ESP's PPS (RB15), watched
 * always and captured by IC1/IC2 while NTP is selected, labelled by the
 * SML-TIME-NTP the ESP sends after each of its edges (esp/esp_link.h).
 *
 * A TIME-NTP arrives a few milliseconds after the edge it labels, so it is
 * paired with the latest ESP edge seen by then (source_ntp_service). At the
 * measurement slot the label for this epoch's edge may not be in yet: it is
 * counted on from the last edge that was labelled, as for GNSS.
 *
 * The ESP's PPS is placed by a timer on the ESP and wanders by microseconds.
 * The ESP slews each routine NTP correction in at no more than
 * SML_NTP_GENTLE_SLEW_PPM, which the loop simply follows. The
 * uncertainty it reports bounds its time, not the jitter of its edge, so
 * the synchroniser adds an allowance for that (SYNC_NTP_EDGE_JITTER_NS).
 * Leap seconds are not followed: a label that jumps at one is judged
 * INCONSISTENT for an epoch.
 *
 * The ESP's own estimate of each edge (utc_phase_ns) arrives with that
 * edge's TIME-NTP, after the measurement slot, so it is only ever known for
 * the edge before. It wanders by a few microseconds from edge to edge, and
 * by tens on the edge after an NTP correction: the edge is corrected by its
 * running average instead (SOURCE_NTP_PHASE_*), outliers left out.
 *
 * While the ESP flags a step or a fast slew (TIME-NTP CORRECTING), the
 * source is reported as stepping its time (SyncSourceReport.stepped): the
 * edge is moving faster than the loop should believe, and the phase jump
 * is the ESP's, not our oscillator's.
 */
#ifndef SOURCE_NTP_H
#define SOURCE_NTP_H

#include <stdbool.h>
#include <stdint.h>

#include "../../common/sml_proto.h"
#include "../timing/timing.h"
#include "sync_source.h"

// The ESP is answering if a TIME-NTP came within this long.
#define SOURCE_NTP_PRESENT_MS 3000UL
// A TIME-NTP labels the latest ESP edge if that was seen within this long
// before the message was taken.
#define SOURCE_NTP_ASSOCIATION_MS 500UL
// Labels are counted on for this long without a new one.
#define SOURCE_NTP_FRESH_MS 5000UL
// The running average of utc_phase_ns: 1/SOURCE_NTP_PHASE_WEIGHT of each
// new value, those further than SOURCE_NTP_PHASE_OUTLIER_NS out left out.
#define SOURCE_NTP_PHASE_WEIGHT     16L
#define SOURCE_NTP_PHASE_OUTLIER_NS 20000L

// Fixed delay from the ESP's timestamp of its edge to the edge on RB15, if
// one is measured (from the GNSS clock, where both are seen).
#ifndef SOURCE_NTP_PPS_DELAY_NS
#define SOURCE_NTP_PPS_DELAY_NS 0L
#endif

typedef struct
{
    uint32_t messages;       // TIME-NTP taken
    uint32_t labelled;       // ... paired with an ESP edge
    uint32_t unassociated;   // ... with no recent ESP edge to label
    uint32_t sequence_slips; // ESP and PIC edge counts moved apart
    uint32_t received_ms;    // when the latest was taken
    bool has_label;
    uint32_t label_edge;         // PIC edge number (pps_monitor) of the latest label
    uint32_t label_esp_sequence; // ... and the ESP's own
    uint32_t label_utc;
    int32_t label_phase_ns; // the ESP's estimate for that edge
    bool has_phase;
    int32_t phase_average_ns; // ... averaged: what the next edge is corrected by
    uint32_t phase_outliers;  // estimates left out of the average
    uint32_t updates;         // NTP polls seen (last_update_utc changed)
    uint32_t corrections;     // ... steps or fast slews flagged CORRECTING
    uint32_t last_update_utc;
} SourceNtpState;

// A TIME-NTP, as the ESP link receives it (esp/esp_link.c): kept for
// source_ntp_service(). A later one before then replaces it.
void source_ntp_received(const SML_TIME_NTP_PAYLOAD *ntp);

// Every 10 ms, after esp_link_service(): pairs a new TIME-NTP with its edge.
void source_ntp_service(uint32_t now_ms);

// Fills report for the epoch in snapshot. now_ms: epoch_timer_ms().
void source_ntp_report(const TimingSnapshot *snapshot, uint32_t now_ms, SyncSourceReport *report);
// Selected: routes the ESP PPS to the capture.
void source_ntp_select(void);

const SourceNtpState *source_ntp_state(void);
// One console line per epoch: the latest TIME-NTP and its pairing.
void source_ntp_print_epoch(const TimingSnapshot *snapshot, uint32_t now_ms);

#endif // SOURCE_NTP_H
