/*
 * GNSS as a time source: the receiver's PPS on IC1/IC2, labelled with UTC
 * from its navigation epochs (gnss/).
 */
#ifndef SOURCE_GNSS_H
#define SOURCE_GNSS_H

#include <stdint.h>

#include "../timing/timing.h"
#include "sync_source.h"

// Fills report for the epoch in snapshot. now_ms: epoch_timer_ms().
void source_gnss_report(const TimingSnapshot *snapshot, uint32_t now_ms, SyncSourceReport *report);

#endif // SOURCE_GNSS_H
