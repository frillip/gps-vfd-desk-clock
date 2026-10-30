/*
 * The edge part of a source report, shared by the PPS sources: the watched
 * edge of the source's input this epoch, and, if the input is routed to the
 * IC1/IC2 capture, the captured edge paired with it.
 */
#ifndef SOURCE_EDGE_H
#define SOURCE_EDGE_H

#include "../timing/timing.h"
#include "sync_source.h"

// A capture and a watched edge this close (the watch's interrupt latency,
// with room for a DISI section) are the same edge.
#define SOURCE_EDGE_PAIR_TICKS (TIMING_TICKS_PER_MS)

// Fills edge, edge_sequence, captured and edge_stamp of report for input.
void source_edge_fill(const TimingSnapshot *snapshot, PpsInput input, SyncSourceReport *report);

// The watched edge's lag behind the capture, if both are of input this
// epoch and pair up.
bool source_edge_latency(const TimingSnapshot *snapshot, PpsInput input, uint32_t *latency_ticks);

#endif // SOURCE_EDGE_H
