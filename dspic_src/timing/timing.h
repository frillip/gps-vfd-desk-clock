/*
 * Timing subsystem entry points: start-up, and the per-epoch snapshot that
 * the control loop works from. See timing_config.h for how the modules fit
 * together.
 */
#ifndef TIMING_H
#define TIMING_H

#include <stdbool.h>
#include <stdint.h>

#include "epoch_output.h"
#include "epoch_timer.h"
#include "pps_capture.h"

// Everything about one epoch, read consistently in one go.
typedef struct
{
    EpochTimerSample timer;
    PpsCaptureSample capture;
    EpochOutputSample output;
} TimingSnapshot;

/*
 * Brings up the timing hardware in the order it needs: captures aligned and
 * checked, then the epoch output armed, then the epoch timer started.
 * Returns whether the capture timebases were proven aligned; if not, the
 * clock still runs but never reports a phase measurement.
 */
bool timing_init(PpsCaptureSync *sync);

/*
 * Once per epoch, EPOCH_MEASURE_SLOT ms after it, by which time both
 * captures belonging to that epoch are in: returns true and fills snapshot.
 * Otherwise returns false at once. Poll from the main loop.
 */
bool timing_snapshot(TimingSnapshot *snapshot);

#endif // TIMING_H
