#include "timing.h"

#include <xc.h>

bool timing_init(PpsCaptureSync *sync)
{
    bool aligned = pps_capture_init(sync);

    // OC1 before T2/T3, so the first epoch trigger is not missed.
    epoch_output_init();
    epoch_timer_init();
    return aligned;
}


bool timing_snapshot(TimingSnapshot *snapshot)
{
    if (!epoch_timer_measure_due())
    {
        return false;
    }

    // Held off together so the three samples describe the same epoch.
    __builtin_disi(0x3FFF);
    epoch_timer_sample(&snapshot->timer);
    pps_capture_sample(&snapshot->capture);
    epoch_output_sample(&snapshot->output);
    __builtin_disi(0);
    return true;
}
