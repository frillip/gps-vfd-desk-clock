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
 * The ESP's PPS is placed by a timer on the ESP and wanders by microseconds;
 * the ESP slews its own clock at up to 100 ppm after each NTP poll. The
 * uncertainty it reports bounds its time, not the jitter of its edge, so
 * the synchroniser adds an allowance for that (SYNC_NTP_EDGE_JITTER_NS).
 * Leap seconds are not followed: a label that jumps at one is judged
 * INCONSISTENT for an epoch.
 */
#ifndef SOURCE_NTP_H
#define SOURCE_NTP_H

#include <stdbool.h>
#include <stdint.h>

#include "../timing/timing.h"
#include "sync_source.h"

// The ESP is answering if a TIME-NTP came within this long.
#define SOURCE_NTP_PRESENT_MS 3000UL
// A TIME-NTP labels the latest ESP edge if that was seen within this long
// before the message was taken.
#define SOURCE_NTP_ASSOCIATION_MS 500UL
// Labels are counted on for this long without a new one.
#define SOURCE_NTP_FRESH_MS 5000UL
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
    int32_t label_phase_ns;
} SourceNtpState;

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
