#include "source_gnss.h"

#include <string.h>

#include "../gnss/gnss.h"
#include "source_edge.h"

void source_gnss_report(const TimingSnapshot *snapshot, uint32_t now_ms, SyncSourceReport *report)
{
    const GnssEpoch *epoch = gnss_fresh_epoch(now_ms);
    uint32_t accuracy_ns;

    memset(report, 0, sizeof(*report));
    report->present = gnss_is_present(now_ms);
    report->time_valid = epoch != NULL && epoch->time_valid;
    if (epoch != NULL)
    {
        report->uncertainty_ns = epoch->time_utc.time_accuracy_ns;
    }

    // The PPS edge seen in this epoch's window, and its UTC label. At the
    // measurement slot the receiver has not yet reported this second, so
    // the label is counted on from the last one it did.
    source_edge_fill(snapshot, PPS_INPUT_GNSS, report);
    report->labelled = report->edge && gnss_utc_at_pps(report->edge_sequence, now_ms,
                                                       &report->utc_seconds, &report->leap_second);
    if (!gnss_leap_event(now_ms, &report->leap))
    {
        report->leap.change = 0;
    }

    report->cross_check = gnss_mark_offset(now_ms, &report->cross_check_ns, &accuracy_ns);
}


void source_gnss_select(void)
{
    pps_capture_route(PPS_INPUT_GNSS);
}
