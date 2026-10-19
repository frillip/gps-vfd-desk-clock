/*
 * Input capture of the GNSS PPS and of the epoch output.
 *
 * IC1/IC2 form one free-running 32-bit capture of the GNSS PPS (RA8);
 * IC3/IC4 form another, of the epoch output OC1 read back from RB10. Both
 * count FCY. The phase of the epoch against UTC is the difference of the two
 * captures, so they must share a timebase: pps_capture_init() aligns the
 * two cascades with a Timer1 sync event and then proves the alignment by
 * capturing one common edge on both.
 *
 * Planned (with the ESP port, not yet implemented): the IC1/IC2 input will be
 * switchable between the GNSS PPS (RA8) and the ESP's NTP-aligned PPS (RB15),
 * so the same capture and control path disciplines to either reference.
 * While GNSS is the reference, the ESP pulse is watched by GPIO
 * interrupt-on-change, giving a second opinion on GNSS and a ready reference
 * to switch to if GNSS disappears.
 */
#ifndef PPS_CAPTURE_H
#define PPS_CAPTURE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    uint32_t ic12_stamp; // capture of the common edge by IC1/IC2
    uint32_t ic34_stamp; // ... and by IC3/IC4; equal when aligned
    bool valid;          // the two timebases agree
} PpsCaptureSync;

typedef struct
{
    uint32_t gnss_capture; // latest GNSS PPS edge, ticks
    uint16_t gnss_low;     // raw IC1BUF / IC2BUF of that edge
    uint16_t gnss_high;
    bool gnss_rollover_corrected; // cascade erratum fix applied
    bool gnss_capture_valid;      // new since the last sample
    uint32_t gnss_edge_sequence;  // counts edges; identifies this one to gnss/

    uint32_t gnss_period;          // latest plausible PPS-to-PPS interval
    uint32_t gnss_period_sequence; // counts accepted periods
    bool gnss_period_valid;        // the latest interval was plausible

    uint32_t epoch_capture; // latest epoch output edge, ticks
    uint16_t epoch_low;     // raw IC3BUF / IC4BUF
    uint16_t epoch_high;
    bool epoch_rollover_corrected;
    bool epoch_capture_valid; // new since the last sample
} PpsCaptureSample;

// The latest GNSS PPS edge, as gnss/ needs it to label edges with UTC.
typedef struct
{
    uint32_t sequence;    // increments once per captured edge
    uint32_t observed_ms; // epoch_timer_ms() when it was captured
    bool valid;           // false until the first trusted edge
} PpsEdge;

/*
 * Configures IC1-IC4, aligns their timebases and routes their inputs to the
 * GNSS PPS and the epoch output. Blocks for well under a millisecond.
 * Captures are only reported valid if the alignment check passed.
 */
bool pps_capture_init(PpsCaptureSync *sync);

// Copies the latest captures and marks them consumed. Call with interrupts
// held off (see timing_snapshot) so the sample is consistent.
void pps_capture_sample(PpsCaptureSample *sample);

// Atomic copy of the latest GNSS PPS edge. Foreground, any time.
void pps_capture_gnss_edge(PpsEdge *edge);

#endif // PPS_CAPTURE_H
