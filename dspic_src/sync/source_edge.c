#include "source_edge.h"

bool source_edge_latency(const TimingSnapshot *snapshot, PpsInput input, uint32_t *latency_ticks)
{
    const PpsMonitorEdge *watched = &snapshot->monitor.inputs[input];
    const PpsCaptureSample *capture = &snapshot->capture;
    uint32_t latency;

    if (!watched->valid || !capture->reference_capture_valid ||
        capture->reference_input != (uint8_t)input)
    {
        return false;
    }
    latency = watched->stamp - capture->reference_capture;
    if (latency > SOURCE_EDGE_PAIR_TICKS)
    {
        return false;
    }
    *latency_ticks = latency;
    return true;
}


void source_edge_fill(const TimingSnapshot *snapshot, PpsInput input, SyncSourceReport *report)
{
    const PpsMonitorEdge *watched = &snapshot->monitor.inputs[input];
    uint32_t latency;

    report->edge = watched->valid;
    report->edge_sequence = watched->sequence;
    report->edge_stamp = watched->stamp - (uint32_t)PPS_MONITOR_LATENCY_TICKS;
    report->captured = source_edge_latency(snapshot, input, &latency);
    if (report->captured)
    {
        report->edge_stamp = snapshot->capture.reference_capture;
    }
}
