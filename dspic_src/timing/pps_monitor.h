/*
 * Interrupt-on-change watch of both PPS inputs: the GNSS PPS (RA8) and the
 * ESP PPS (RB15).
 *
 * Only one of them can be captured by IC1/IC2 at a time (pps_capture.h);
 * the other would otherwise go unseen. Each rising edge on either pin is
 * timestamped here from the capture timebase, numbered, and given its
 * epoch_timer_ms() time. That is coarse evidence: the timestamp carries the
 * interrupt latency (a microsecond or so, more if a higher-priority
 * interrupt or a DISI section is running), but it shows that a source's
 * pulses are arriving and roughly where, which is enough to qualify it and
 * to label its edges. Steering still needs the capture.
 *
 * The edge numbers here are the ones the sources label with UTC: gnss/
 * associates navigation epochs with GNSS edges, sync/source_ntp.c
 * associates TIME-NTP messages with ESP edges. They count every edge seen,
 * whether or not the pin is also routed to the capture, so routing never
 * disturbs labelling. A captured edge is paired with its number by
 * timestamp (sync/source_edge.c).
 */
#ifndef PPS_MONITOR_H
#define PPS_MONITOR_H

#include <stdbool.h>
#include <stdint.h>

#include "pps_capture.h"

// Interrupt latency allowance subtracted from a watched edge's timestamp to
// estimate the edge itself: a first guess, to be replaced by the measured
// mean of the IOC-minus-capture statistics (sync_tuning.c).
#ifndef PPS_MONITOR_LATENCY_TICKS
#define PPS_MONITOR_LATENCY_TICKS 40L // 1 us
#endif

// One input's latest edge.
typedef struct
{
    uint32_t stamp;       // capture timebase when the interrupt ran, ticks
    uint32_t sequence;    // counts rising edges, from 1
    uint32_t observed_ms; // epoch_timer_ms() when seen
    bool valid;           // new since the last sample
    uint32_t period;      // stamp minus the previous one, when plausible
    bool period_valid;
} PpsMonitorEdge;

typedef struct
{
    PpsMonitorEdge inputs[PPS_INPUT_COUNT];
} PpsMonitorSample;

// The latest edge of one input, for labelling.
typedef struct
{
    uint32_t sequence;    // as PpsMonitorEdge
    uint32_t observed_ms; // epoch_timer_ms() when seen
    bool valid;           // false until the first edge
} PpsEdge;

typedef struct
{
    uint32_t edges[PPS_INPUT_COUNT];
    uint32_t implausible[PPS_INPUT_COUNT]; // periods outside +/-1000 ppm
    uint32_t unseen;                       // changes gone again before the handler read the pins
} PpsMonitorStats;

// After pps_capture_init(): timestamps need the aligned timebase.
void pps_monitor_init(void);

// Copies the latest edges and marks them consumed. Call with interrupts held
// off, with pps_capture_sample() (see timing_snapshot).
void pps_monitor_sample(PpsMonitorSample *sample);

// Atomic copy of one input's latest edge. Foreground, any time.
void pps_monitor_edge(PpsInput input, PpsEdge *edge);

const PpsMonitorStats *pps_monitor_stats(void);

#endif // PPS_MONITOR_H
