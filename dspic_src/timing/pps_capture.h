/*
 * Input capture of the reference PPS and of the epoch output.
 *
 * IC1/IC2 form one free-running 32-bit capture of the reference PPS: the
 * GNSS PPS (RA8) or the ESP's NTP-aligned PPS (RB15), switched by
 * pps_capture_route() when the synchroniser selects a source that needs it.
 * IC3/IC4 form another, of the epoch output OC1 read back from RB10. All
 * four count FCY. The phase of the epoch against the reference is the
 * difference of the two captures, so they must share a timebase:
 * pps_capture_init() aligns the two cascades with a Timer1 sync event and
 * then proves the alignment by capturing one common edge on both.
 *
 * Whichever input is not routed here is still watched, by interrupt-on-change
 * (pps_monitor.h), timestamped on the same timebase.
 */
#ifndef PPS_CAPTURE_H
#define PPS_CAPTURE_H

#include <stdbool.h>
#include <stdint.h>

// The PPS inputs the reference capture can be routed to.
typedef enum
{
    PPS_INPUT_GNSS = 0, // RA8
    PPS_INPUT_ESP,      // RB15
    PPS_INPUT_COUNT
} PpsInput;

// Alignment attempts before settling for a small offset, and the largest
// offset (ticks) that is then corrected rather than refused.
#define PPS_CAPTURE_ALIGN_ATTEMPTS  8U
#define PPS_CAPTURE_ALIGN_TOLERANCE 4L

typedef struct
{
    uint32_t ic12_stamp; // capture of the common edge by IC1/IC2, last attempt
    uint32_t ic34_stamp; // ... and by IC3/IC4; equal when aligned
    int32_t offsets[PPS_CAPTURE_ALIGN_ATTEMPTS]; // IC1/IC2 minus IC3/IC4, each attempt
    uint8_t attempts;
    int32_t correction; // ticks taken off every IC1/IC2 capture: 0 if aligned
    bool valid;         // the captures can be trusted (aligned, or corrected)
} PpsCaptureSync;

typedef struct
{
    uint8_t reference_input;    // PpsInput routed to IC1/IC2
    uint32_t reference_capture; // latest reference edge, ticks
    uint16_t reference_low;     // raw IC1BUF / IC2BUF of that edge
    uint16_t reference_high;
    bool reference_rollover_corrected; // cascade erratum fix applied
    bool reference_capture_valid;      // new since the last sample
    uint32_t reference_edge_sequence;  // counts captured edges, any input

    uint32_t reference_period;          // latest plausible edge-to-edge interval
    uint32_t reference_period_sequence; // counts accepted periods
    bool reference_period_valid;        // the latest interval was plausible

    uint32_t epoch_capture; // latest epoch output edge, ticks
    uint16_t epoch_low;     // raw IC3BUF / IC4BUF
    uint16_t epoch_high;
    bool epoch_rollover_corrected;
    bool epoch_capture_valid; // new since the last sample
} PpsCaptureSample;

/*
 * Configures IC1-IC4, aligns their timebases and routes their inputs to the
 * GNSS PPS and the epoch output. The alignment is checked by capturing one
 * strobe edge on both pairs, and retried from scratch (modules reset,
 * Timer1 sync again) up to PPS_CAPTURE_ALIGN_ATTEMPTS times until the two
 * agree. If they never do, but the last attempts agree on an offset within
 * PPS_CAPTURE_ALIGN_TOLERANCE, the offset is corrected on every IC1/IC2
 * capture: the timebases still count the same clock, only from different
 * cycles. Otherwise captures are never reported valid. Blocks for a few
 * milliseconds at most.
 */
bool pps_capture_init(PpsCaptureSync *sync);

// Copies the latest captures and marks them consumed. Call with interrupts
// held off (see timing_snapshot) so the sample is consistent.
void pps_capture_sample(PpsCaptureSample *sample);

/*
 * Routes IC1/IC2 to input from the next edge on. The capture and period in
 * flight are dropped: the first period after a change would span two
 * references. Foreground; takes a few microseconds with interrupts held off.
 */
void pps_capture_route(PpsInput input);
PpsInput pps_capture_input(void);

// The capture timebase now: what IC3/IC4 would capture for an edge at this
// instant. Timestamps the edges that are watched, not captured
// (pps_monitor.c). Any context.
uint32_t pps_capture_timebase(void);

#endif // PPS_CAPTURE_H
