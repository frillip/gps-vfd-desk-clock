#include "source_ntp.h"

#include <string.h>

#include "../console/text.h"
#include "../platform/time_ms.h"
#include "source_edge.h"

static SourceNtpState state;
static SML_TIME_NTP_PAYLOAD latest;
static bool has_latest;
static bool pending; // a TIME-NTP received, not yet paired
static SML_TIME_NTP_PAYLOAD received;
static bool correcting; // the latest TIME-NTP flags CORRECTING

static void average_phase(int32_t phase_ns)
{
    int32_t difference;

    if (!state.has_phase)
    {
        state.has_phase = true;
        state.phase_average_ns = phase_ns;
        return;
    }
    difference = phase_ns - state.phase_average_ns;
    if (difference > SOURCE_NTP_PHASE_OUTLIER_NS || difference < -SOURCE_NTP_PHASE_OUTLIER_NS)
    {
        state.phase_outliers++;
        return;
    }
    state.phase_average_ns += difference / SOURCE_NTP_PHASE_WEIGHT;
}


void source_ntp_received(const SML_TIME_NTP_PAYLOAD *ntp)
{
    received = *ntp;
    pending = true;
}


void source_ntp_service(uint32_t now_ms)
{
    PpsEdge edge;

    if (!pending)
    {
        return;
    }
    pending = false;
    latest = received;
    has_latest = true;
    state.messages++;
    state.received_ms = now_ms;
    if ((latest.flags & SML_NTP_UPDATE_VALID) != 0U &&
        latest.last_update_utc != state.last_update_utc)
    {
        state.updates++;
        state.last_update_utc = latest.last_update_utc;
    }
    if ((latest.flags & SML_NTP_CORRECTING) != 0U && !correcting)
    {
        state.corrections++;
    }
    correcting = (latest.flags & SML_NTP_CORRECTING) != 0U;

    pps_monitor_edge(PPS_INPUT_ESP, &edge);
    if (!edge.valid || time_ms_elapsed(now_ms, edge.observed_ms) > SOURCE_NTP_ASSOCIATION_MS)
    {
        state.unassociated++;
        return;
    }
    // The two edge counts move together, unless an edge was missed on one
    // side: diagnostic only, the PIC's count is the one labelled.
    if (state.has_label &&
        (edge.sequence - state.label_edge) != (latest.pps_sequence - state.label_esp_sequence))
    {
        state.sequence_slips++;
    }
    state.has_label = true;
    state.label_edge = edge.sequence;
    state.label_esp_sequence = latest.pps_sequence;
    state.label_utc = latest.utc_seconds;
    state.label_phase_ns = latest.utc_phase_ns;
    average_phase(latest.utc_phase_ns);
    state.labelled++;
}


void source_ntp_report(const TimingSnapshot *snapshot, uint32_t now_ms, SyncSourceReport *report)
{
    memset(report, 0, sizeof(*report));
    report->present =
        has_latest && time_ms_elapsed(now_ms, state.received_ms) < SOURCE_NTP_PRESENT_MS;
    report->time_valid = report->present && (latest.flags & SML_NTP_TIME_VALID) != 0U &&
                         (latest.flags & SML_NTP_UNCERTAINTY_VALID) != 0U;
    if (report->time_valid)
    {
        report->uncertainty_ns = latest.uncertainty_ns;
    }

    source_edge_fill(snapshot, PPS_INPUT_ESP, report);
    // Counted on from the last labelled edge (never backwards).
    if (report->edge && state.has_label &&
        time_ms_elapsed(now_ms, state.received_ms) < SOURCE_NTP_FRESH_MS &&
        (int32_t)(report->edge_sequence - state.label_edge) >= 0)
    {
        bool leap;
        report->labelled = true;
        report->utc_seconds = utc_advance(state.label_utc, false,
                                          report->edge_sequence - state.label_edge, NULL, &leap);
        report->edge_offset_ns = state.phase_average_ns + SOURCE_NTP_PPS_DELAY_NS;
    }
    report->stepped = report->present && correcting;
}


void source_ntp_select(void)
{
    pps_capture_route(PPS_INPUT_ESP);
}


const SourceNtpState *source_ntp_state(void)
{
    return &state;
}


void source_ntp_print_epoch(const TimingSnapshot *snapshot, uint32_t now_ms)
{
    const PpsMonitorEdge *edge = &snapshot->monitor.inputs[PPS_INPUT_ESP];
    TextLine line;

    text_start(&line);
    text_str(&line, "       NTP: edge ");
    text_u32(&line, edge->sequence, 0, ' ');
    text_str(&line, edge->valid ? "" : " (none)");
    if (!has_latest)
    {
        text_str(&line, ", no TIME-NTP\r\n");
        text_send(&line);
        return;
    }
    text_str(&line, " label ");
    text_u32(&line, state.label_edge, 0, ' ');
    text_char(&line, '=');
    text_u32(&line, state.label_utc, 0, ' ');
    text_str(&line, " esp-ph ");
    text_i32(&line, state.label_phase_ns, 0, true);
    text_str(&line, "ns (avg ");
    text_i32(&line, state.phase_average_ns, 0, true);
    text_str(&line, ") unc ");
    text_u32(&line, latest.uncertainty_ns, 0, ' ');
    text_str(&line, "ns flags ");
    text_hex16(&line, latest.flags);
    text_str(&line, " src ");
    text_u32(&line, latest.source, 0, ' ');
    text_str(&line, " lock ");
    text_u32(&line, latest.lock_state, 0, ' ');
    text_str(&line, " rtt ");
    text_u32(&line, latest.round_trip_us, 0, ' ');
    text_str(&line, "us off ");
    text_i32(&line, latest.last_offset_us, 0, true);
    text_str(&line, "us age ");
    text_u32(&line, time_ms_elapsed(now_ms, state.received_ms), 0, ' ');
    text_str(&line, "ms freq ");
    text_i32(&line, latest.frequency_estimate_ppt, 0, true);
    text_str(&line, "ppt\r\n");
    text_send(&line);
}
