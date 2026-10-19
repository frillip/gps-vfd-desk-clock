// Human-readable timing status on stdout (the UART1 console).
#ifndef TIMING_REPORT_H
#define TIMING_REPORT_H

#include "epoch_control.h"
#include "timing.h"

// The start-up capture timebase check.
void timing_report_sync(const PpsCaptureSync *sync);

// One epoch: three lines after a measurement, one otherwise.
void timing_report_epoch(const TimingSnapshot *snapshot, const EpochUpdate *update);

#endif // TIMING_REPORT_H
