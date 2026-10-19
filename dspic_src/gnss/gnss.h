/*
 * GNSS service: the single owner of the receiver link and of everything the
 * clock knows about GNSS. Other modules read GnssState through the accessors
 * below instead of sharing globals.
 *
 *   UART2 ring -> UBX parser -> message handler -+-> detection (MON-VER)
 *                                                +-> epoch assembler (NAV-*)
 *                                                +-> leap seconds (NAV-TIMELS)
 *                                                +-> time pulse (TIM-TP)
 *                                                +-> time mark (TIM-TM2)
 *
 * Everything runs in the foreground from gnss_service(), every 10 ms. Nothing
 * here is touched from an interrupt.
 *
 * The receiver is expected to be configured already (UBX output on UART1 at
 * GNSS_OPERATING_BAUD, the messages above enabled). Provisioning a factory
 * receiver will come later; until then use u-center.
 *
 * Time model: a published epoch labels one PPS edge, identified by the PPS
 * capture sequence number (see gnss_epoch.h). The UTC second of any later
 * edge is that label plus the number of edges since, for at most
 * GNSS_EPOCH_FAIL_S - 1 edges. gnss_utc_at_pps() applies all the checks.
 */
#ifndef GNSS_H
#define GNSS_H
#include <stdint.h>
#include <stdbool.h>
#include "gnss_detect.h"
#include "gnss_epoch.h"
#include "ubx_decode.h"
#include "ubx_parser.h"
#include "../platform/utc.h"

#define GNSS_OPERATING_BAUD     115200UL
#define GNSS_INTERRUPT_PRIORITY 1 // UART2, alongside the console

// Freshness policy.
#define GNSS_LINK_SILENCE_MS      3000U // present while frames arrive this often
#define GNSS_LINK_RECOVERY_MS     5000U // silence before the UART is restarted
#define GNSS_EPOCH_MAX_AGE_MS     2500U // by the millisecond tick
#define GNSS_EPOCH_WARN_S         1U    // PPS edges since the labelled one
#define GNSS_EPOCH_FAIL_S         2U
#define GNSS_TIMELS_MAX_AGE_MS    30000U // NAV-TIMELS is sent every 10 s
#define GNSS_TIME_MARK_MAX_AGE_MS 3000U
// A leap second event computed from NAV-TIMELS may sit this far from the
// UTC midnight it describes (whole-second rounding either side).
#define GNSS_LEAP_EVENT_TOLERANCE_S 2U

/*
 * TIM-TM2: the receiver timestamps the epoch output on its EXTINT input.
 *
 * V2: RB10 (LATCH_OC_3V3) -> U12 LVC1G32 OR with LATCH_GPIO_3V3 -> LATCH_3V3
 * -> U4 LV4T125 to 5 V -> LATCH -> U1 AHC1G04 inverter -> LATCH_GPS_3V3 ->
 * EXTINT. The epoch's rising edge is the receiver's FALLING edge, about
 * 10-20 ns of gate delay late; the receiver's rising edge is the end of the
 * 100 ms pulse (software-timed, a few us late, no information). Any ad-hoc
 * LATCH_GPIO pulse (e.g. a menu redraw) also reaches EXTINT and produces a
 * mark that is not the epoch: GNSS_TIME_MARK_WINDOW_NS rejects those.
 *
 * V3: EXTINT is fed from EPOCH_OUT_3V3 through non-inverting LVC buffers,
 * separate from the display latch. Build with this set to 0 there.
 */
#ifndef GNSS_TIME_MARK_EPOCH_EDGE_FALLING
#define GNSS_TIME_MARK_EPOCH_EDGE_FALLING 1
#endif

// An epoch mark further than this from the second is not the epoch edge.
#define GNSS_TIME_MARK_WINDOW_NS 1000000L

/*
 * Time pulse information (TIM-TP). The receiver sends it with each
 * navigation solution, hundreds of ms before the pulse it describes, so it
 * belongs to the edge after the most recent one captured: pps_sequence is
 * that edge's capture sequence number.
 *
 * quantisation_error_ps (qErr) is how far that pulse's edge sits from the
 * receiver's ideal second, because the receiver can only place it on its own
 * clock grid (a sawtooth of a few ns). Not applied to the phase measurement
 * yet: its sign convention is to be confirmed against captures on hardware.
 */
typedef struct
{
    UbxTimTp message;
    uint32_t received_ms;
    uint32_t pps_sequence; // the edge described, valid if associated
    bool associated;
} GnssTimePulse;

typedef struct
{
    uint32_t baud;

    // Link health: any checksum-valid frame counts.
    bool frame_seen;
    uint32_t last_frame_ms;
    uint32_t link_recoveries;
    uint32_t last_recovery_ms;

    GnssDetect detection;      // identity, from MON-VER
    GnssEpochAssembler epochs; // NAV-STATUS/TIMEUTC/CLOCK/POSLLH

    bool has_leap_seconds;
    UbxNavTimels leap_seconds;
    uint32_t leap_seconds_received_ms;
    bool has_leap_event; // leap_seconds announces an event at a UTC midnight
    UtcLeap leap_event;

    bool has_time_pulse;
    GnssTimePulse time_pulse;
    uint32_t time_pulses_unassociated;

    bool has_time_mark; // TIM-TM2: receiver's measurement of our pulse
    UbxTimTm2 time_mark;
    uint32_t time_mark_received_ms;
    uint32_t time_marks_out_of_window; // new epoch edges outside GNSS_TIME_MARK_WINDOW_NS
} GnssState;

// Sets the UART2 interrupt priorities, starts UART2 at GNSS_OPERATING_BAUD
// and polls for the receiver. Pins are mapped by board_init().
void gnss_init(uint32_t instruction_hz, uint32_t now_ms);
// pps: atomic snapshot of the latest GNSS PPS capture.
void gnss_service(uint32_t now_ms, GnssPpsReference pps);

const GnssState *gnss_state(void);
void gnss_parser_stats(UbxParserStats *destination);

// Receiver is answering: a valid frame within GNSS_LINK_SILENCE_MS.
bool gnss_is_present(uint32_t now_ms);
// Latest epoch if it is fresh, else NULL.
const GnssEpoch *gnss_fresh_epoch(uint32_t now_ms);
// Fresh epoch with a usable fix.
bool gnss_has_fix(uint32_t now_ms);
/* UTC (Unix seconds) of the PPS edge with this capture sequence number, and
 * whether it is an inserted leap second (23:59:60, held as :59; see utc.h).
 * False unless the latest epoch is fresh, time-valid, labelled a PPS edge,
 * and that edge is fewer than GNSS_EPOCH_FAIL_S edges before this one.
 * Edges after the labelled one are counted on through any announced leap
 * second. leap_second may be NULL. */
bool gnss_utc_at_pps(uint32_t pps_sequence, uint32_t now_ms, uint32_t *utc_seconds,
                     bool *leap_second);

// Fresh, valid NAV-TIMELS that announces a leap second. change is +1 or -1;
// seconds_to_event counts down to the event.
bool gnss_leap_pending(uint32_t now_ms, int8_t *change, int32_t *seconds_to_event);
// The announced leap second as a UTC event, from a fresh NAV-TIMELS whose
// time to the event lands on a UTC midnight.
bool gnss_leap_event(uint32_t now_ms, UtcLeap *leap);
// GPS - UTC in seconds, from a fresh NAV-TIMELS.
bool gnss_leap_seconds(uint32_t now_ms, int8_t *gps_minus_utc_s);

// The TIM-TP describing the PPS edge with this capture sequence number, if
// one was received and its qErr is valid.
bool gnss_time_pulse(uint32_t pps_sequence, const UbxTimTp **time_pulse);

/* Where the receiver saw our epoch edge (TIM-TM2), relative to the nearest
 * whole second of its time base: positive = our epoch late. An independent
 * check of the phase the clock measures with its own captures, good to the
 * receiver's stated accuracy. False unless a fresh, time-valid mark with a
 * new epoch edge within GNSS_TIME_MARK_WINDOW_NS of the second is held. */
bool gnss_mark_offset(uint32_t now_ms, int32_t *offset_ns, uint32_t *accuracy_ns);

// Full multi-line diagnostics.
void gnss_print(uint32_t now_ms);
// One line for the per-epoch report: the UTC label of this edge, fix, time
// accuracy, leap seconds and qErr.
void gnss_print_edge(uint32_t pps_sequence, uint32_t now_ms);

#endif
