/*
 * Synchroniser: chooses the time source, decides when the epoch control loop
 * may use it, and keeps the UTC second of our epoch. It sits on top of
 * timing/ and runs once per epoch:
 *
 *   timing_snapshot() -> sync_update() -+-> each source adapter -> judged
 *                                       +-> candidates ranked, state machine
 *                                       +-> epoch_control_update(usable)
 *                                       +-> UTC label of our epoch
 *
 * Every candidate carries a +/- bound on its time error, in ns, and the
 * choice between them is made on those bounds alone (lower is better; the
 * table order in sync.c only breaks ties):
 *
 *   a source       its own uncertainty, made conservative: GNSS tAcc x
 *                  SYNC_GNSS_TACC_SCALE plus the capture resolution, NTP the
 *                  ESP's bound plus an allowance for the jitter of its PPS.
 *                  A candidate once GOOD for SYNC_QUALIFY_EPOCHS.
 *   HOLDOVER       our own oscillator since the loop last steered:
 *                  u0 + f.t + D.t^2/2, where u0 is the bound when it last
 *                  steered, f the frequency error left by that source
 *                  (SYNC_HOLDOVER_FREQUENCY_*) and D the oscillator's drift
 *                  rate (SYNC_HOLDOVER_DRIFT_PPT_PER_S). A candidate once
 *                  the loop has locked.
 *
 * So when GNSS fails, holdover takes over and is kept while it is still
 * better than NTP, which only takes over once its bound is less than half
 * of holdover's.
 *
 * The accuracy limits (SYNC_*_QUALIFY_NS, SYNC_*_HOLD_NS) decide what is a
 * candidate, not whether the clock runs at all. In free run, with nothing
 * else, a source that meets every other requirement for SYNC_QUALIFY_EPOCHS
 * (present, time valid, its edge seen, labelled, consistent) is taken as a
 * fallback however coarse it is: NTP with an ESP that only knows its time
 * to tens of ms, a GNSS receiver just after its first fix. It is kept while
 * it meets those requirements, and any candidate that beats it by
 * SYNC_SWITCH_RATIO displaces it as usual. Even before a source is taken,
 * the time of day is set from the best labelled one, so the clock shows
 * the time from the first epoch it is known. A source displaces the one in use only when its bound is
 * less than 1/SYNC_SWITCH_RATIO of it for SYNC_SWITCH_EPOCHS in a row.
 *
 * States (the source in brackets is the selected one):
 *
 *   FREE_RUN       no source: the epoch runs on the last frequency estimate,
 *                  and the time of day (if ever known) is counted on.
 *   ACQUIRE(s)     s has been chosen: the loop learns its frequency and steps
 *                  onto its edge, or slews onto it if it is already close.
 *   LOCKED(s)      the loop tracks s.
 *   HOLDOVER(s)    s, the source last steered by, has failed or been
 *                  outranked: the loop holds frequency.
 *
 * Synchronised (LOCKED or HOLDOVER) only while the bound in use is within
 * SYNC_SYNCHRONISED_LIMIT_NS: a fallback beyond it steers the clock, but
 * does not count.
 *
 *   FREE_RUN  -> ACQUIRE   a source has qualified, or failing that, a fallback
 *   ACQUIRE   -> LOCKED    the loop measures in TRACK after its step
 *   ACQUIRE   -> HOLDOVER  s fails, and the loop had locked before
 *   ACQUIRE   -> FREE_RUN  s fails, and it had not
 *   LOCKED    -> ACQUIRE   the loop loses phase and re-acquires
 *   LOCKED    -> HOLDOVER  s fails
 *   HOLDOVER  -> LOCKED    s is GOOD again, with its edge captured, and no
 *                          worse than holdover (the loop's phase gate decides
 *                          whether it is still in phase)
 *   HOLDOVER  -> ACQUIRE   s is back but its edge is not routed to the
 *                          capture, or another source beats holdover
 *   HOLDOVER  -> FREE_RUN  the holdover bound passes SYNC_SYNCHRONISED_LIMIT_NS
 *   ACQUIRE/LOCKED(s) -> ACQUIRE(t)
 *                          t beats s (see above)
 *
 * The RTC (source_rtc.c) is a source of the time of day only: on V2 it has
 * no edge the capture can take, so it never steers and is never a
 * candidate (sync_source_steers). It is judged like any other, with a
 * bound of SYNC_RTC_UNCERTAINTY_NS, and at start-up it is usually the first
 * source with a time: the time of day is seeded from it (as above), and
 * while still free running, before anything has steered, the epoch is
 * stepped once onto the RTC's second (epoch_control_free_run_step), so
 * the seconds shown change with the RTC's. That happens the first epoch it
 * is GOOD; and so that it does happen first, in free run nothing is taken
 * to steer by while the RTC answers with a time but its second is still
 * being found, for up to SYNC_ALIGN_WAIT_EPOCHS. GNSS or NTP take over
 * from there.
 *
 * Adding a source takes an id here, an adapter (see source_gnss.c) and a row
 * in the table in sync.c. Every source's edge is watched (pps_monitor), but
 * only one at a time can own the IC1/IC2 capture pair that the loop steers
 * by: a source routes it in its select hook (pps_capture_route), so the
 * routing only ever changes when a source is selected. The loop is then
 * restarted with the source's profile (EpochProfile).
 */
#ifndef SYNC_H
#define SYNC_H

#include <stdbool.h>
#include <stdint.h>

#include "../timing/epoch_control.h"
#include "../timing/timing.h"
#include "sync_source.h"

// In order of preference, when the bounds are equal.
typedef enum
{
    SYNC_SOURCE_GNSS = 0,
    SYNC_SOURCE_NTP,
    SYNC_SOURCE_RTC, // time of day only: never steers
    SYNC_SOURCE_COUNT
} SyncSourceId;

#define SYNC_SOURCE_NONE SYNC_SOURCE_COUNT
// The candidates are the sources and holdover.
#define SYNC_CANDIDATE_HOLDOVER (SYNC_SOURCE_COUNT + 1U)
#define SYNC_BOUND_NONE         UINT32_MAX // not a candidate

// Consecutive GOOD epochs before a source is a candidate.
#define SYNC_QUALIFY_EPOCHS 3U
// A candidate displaces the one in use when its bound is less than
// 1/SYNC_SWITCH_RATIO of it for this many epochs in a row.
#define SYNC_SWITCH_EPOCHS 10U
#define SYNC_SWITCH_RATIO  2U

// GNSS uncertainty (tAcc) limits: to be taken up, and to stay in use.
#ifndef SYNC_GNSS_QUALIFY_NS
#define SYNC_GNSS_QUALIFY_NS 10000UL // 10 us
#endif
#ifndef SYNC_GNSS_HOLD_NS
#define SYNC_GNSS_HOLD_NS 100000UL // 100 us
#endif
// tAcc is an estimate, not a bound: this many of them make one.
#define SYNC_GNSS_TACC_SCALE 3U

// NTP uncertainty limits (the ESP's own bound), and the allowance added for
// the ESP's PPS: placed by a timer on the ESP, measured at 5-7 us of
// latency and jitter. To be refined from the bench statistics. The ESP's
// bound includes the whole round trip, so over Wi-Fi it is ~10 ms even
// when its offset is well under 1 ms: the limits only keep out an ESP that
// has lost track, and the ranking does the rest.
#ifndef SYNC_NTP_QUALIFY_NS
#define SYNC_NTP_QUALIFY_NS 50000000UL // 50 ms
#endif
#ifndef SYNC_NTP_HOLD_NS
#define SYNC_NTP_HOLD_NS 100000000UL // 100 ms
#endif
#ifndef SYNC_NTP_EDGE_JITTER_NS
#define SYNC_NTP_EDGE_JITTER_NS 20000UL // 20 us
#endif

// The RTC's bound: the RTC holds UTC to the second, and how far its second
// is from UTC's is not known for sure until the time of its last aligned
// write can be kept over a power cycle. Its limits only require it to be
// within that.
#ifndef SYNC_RTC_UNCERTAINTY_NS
#define SYNC_RTC_UNCERTAINTY_NS 999000000UL // 999 ms
#endif
#define SYNC_RTC_LIMIT_NS 1000000000UL
// At start-up, free run waits this long at most for a time-of-day source
// that has a time but not yet its second (the RTC being searched).
#define SYNC_ALIGN_WAIT_EPOCHS 6U

// The loop profile for NTP (EpochProfile): Kp 1/64, Ki 1/16384, critically
// damped with a time constant of about two minutes, so the ESP's edge
// jitter averages out; up to 10 ppm of slew; a gate of 300 us. Reference
// periods do not teach the Kalman filter: the ESP slews its own clock.
#ifndef SYNC_NTP_KP_DIVISOR
#define SYNC_NTP_KP_DIVISOR 64L
#endif
#ifndef SYNC_NTP_KI_DIVISOR
#define SYNC_NTP_KI_DIVISOR 16384L
#endif
#ifndef SYNC_NTP_LIMIT_TICKS
#define SYNC_NTP_LIMIT_TICKS 400L // integral and correction: 10 ppm
#endif
#ifndef SYNC_NTP_GATE_TICKS
#define SYNC_NTP_GATE_TICKS 12000L // 300 us
#endif
#ifndef SYNC_NTP_REJECT_LIMIT
#define SYNC_NTP_REJECT_LIMIT 8U
#endif

/*
 * The holdover model, u0 + f.t + D.t^2/2. f is the frequency error left by
 * the source last steered by, in ns/s (= ppb); D the oscillator's frequency
 * drift rate, in ppt/s.
 *
 * GNSS values from the V2 bench (2026-10-20/21, 41 two-hour virtual
 * holdover trials, sync_tuning.c). Error held minus true, RMS / worst:
 * 60 s 0.12 / 0.8 us, 10 min 3.8 / 23 us, 30 min 7 / 33 us, 1 h 15 / 50 us,
 * 2 h 34 / 96 us; the Kalman estimate drifted ~1.8 ppt/s overnight. The
 * bound covers the worst case with margin for temperature: 2.4 us at a
 * minute, 25 us at ten, 176 us at an hour, 420 us at two, ~22 ms at a day.
 * After NTP (NTP-only V2 clock, 11 h): the held frequency against the ESP's
 * PPS was off by RMS / worst 0.16 / 0.47 ms after 10 min and 1.3 / 3.2 ms
 * after an hour, mostly the ESP's own frequency wandering by 2 ppm between
 * its 30-minute polls. 1 ppm covers it; next to the ESP's own ~12 ms bound,
 * which u0 carries, it hardly matters.
 */
#ifndef SYNC_HOLDOVER_FREQUENCY_GNSS
#define SYNC_HOLDOVER_FREQUENCY_GNSS 40UL // ns/s
#endif
#ifndef SYNC_HOLDOVER_FREQUENCY_NTP
#define SYNC_HOLDOVER_FREQUENCY_NTP 1000UL // ns/s
#endif
#ifndef SYNC_HOLDOVER_DRIFT_PPT_PER_S
#define SYNC_HOLDOVER_DRIFT_PPT_PER_S 5UL
#endif
// Synchronised (the colon blinks, the ESP is told LOCKED or HOLDOVER) while
// the bound is within this; beyond it holdover ends in free run. No lower
// than SYNC_NTP_HOLD_NS, so holdover outlasts any NTP worth using.
#ifndef SYNC_SYNCHRONISED_LIMIT_NS
#define SYNC_SYNCHRONISED_LIMIT_NS 100000000UL // 100 ms
#endif

// State changes kept for diagnostics.
#define SYNC_HISTORY_LENGTH 8U
// Capture resolution: one timer tick.
#define SYNC_CAPTURE_RESOLUTION_NS 25UL

typedef enum
{
    SYNC_FREE_RUN = 0,
    SYNC_ACQUIRE,
    SYNC_LOCKED,
    SYNC_HOLDOVER
} SyncState;

typedef enum
{
    SYNC_REASON_QUALIFIED = 0, // a source qualified with none selected
    SYNC_REASON_TRACKING,      // the loop is tracking
    SYNC_REASON_PHASE_LOST,    // the loop lost phase and re-acquires
    SYNC_REASON_SOURCE_LOST,   // the selected source failed
    SYNC_REASON_SOURCE_BACK,   // ... and is good again
    SYNC_REASON_ALTERNATIVE,   // another source took over a failed one
    SYNC_REASON_BETTER_SOURCE, // a better source took over
    SYNC_REASON_EXPIRED        // holdover ran out
} SyncReason;

typedef struct
{
    uint32_t epoch; // our epoch sequence number
    SyncState from;
    SyncState to;
    uint8_t source;  // SyncSourceId selected after it, or SYNC_SOURCE_NONE
    uint8_t reason;  // SyncReason
    uint8_t verdict; // SyncCheck of the source selected before it
} SyncTransition;

// The UTC second our epoch edge marks.
typedef struct
{
    bool known;           // labelled by a source at least once since start
    uint32_t utc_seconds; // Unix seconds; an inserted leap second repeats :59
    bool leap_second;     // ... this one is 23:59:60
    UtcLeap leap;         // announced leap second in use (see sync_set_leap)
    uint32_t corrections; // times a source's label replaced the count
    uint8_t source;       // SyncSourceId that last labelled or seeded it
} SyncTime;

// Holdover as a candidate: how far our oscillator can be trusted since the
// loop last steered.
typedef struct
{
    bool ready;            // the loop has locked: there is something to hold
    uint8_t source;        // the source it last steered by
    uint32_t age;          // epochs since
    uint32_t base_ns;      // the bound at that epoch (u0)
    uint32_t frequency_ns; // the frequency error that source leaves, ns/s
    uint32_t bound_ns;     // the bound now
} SyncHoldover;

typedef struct
{
    SyncState state;
    uint8_t source;        // SyncSourceId, or SYNC_SOURCE_NONE; in HOLDOVER the
                           // source last steered by
    bool fallback;         // source was taken as a fallback: kept while INACCURATE
    uint32_t state_epochs; // epochs in this state
    uint32_t holdover_epochs;
    SyncSourceTracker sources[SYNC_SOURCE_COUNT];
    SyncHoldover holdover;
    SyncTime time;
    UtcLeap link_leap; // announced by the ESP (SML-TIME-LEAP), if any

    // The ranking: the best candidate other than the one in use, and the
    // one, if any, on its way to displacing it.
    uint8_t second; // a SyncSourceId, SYNC_CANDIDATE_HOLDOVER or SYNC_SOURCE_NONE
    uint32_t second_bound_ns;
    uint8_t challenger;
    uint16_t challenge_run;

    // The latest epoch: our epoch minus the selected source's UTC second.
    uint32_t epochs; // processed since sync_init()
    bool phase_valid;
    int32_t phase_ticks;

    uint32_t transitions; // total; the latest are in history
    SyncTransition history[SYNC_HISTORY_LENGTH];

    // Start-up alignment onto a time-of-day source (the RTC), once.
    bool steered;          // a source has been selected to steer by since start
    bool aligned;          // the epoch was stepped onto a time-of-day source
    uint8_t aligned_to;    // ... which one
    int32_t aligned_ticks; // ... by how much (the step requested)
} SyncStatus;

typedef struct
{
    EpochUpdate loop;  // what the control loop did
    bool usable;       // the selected source's edge was used this epoch
    bool transitioned; // the state changed: see sync_last_transition()
} SyncUpdate;

// Also initialises the epoch control loop. Before timing_init().
void sync_init(void);

// Once per epoch, with the snapshot from timing_snapshot(). now_ms:
// epoch_timer_ms().
SyncUpdate sync_update(const TimingSnapshot *snapshot, uint32_t now_ms);

const SyncStatus *sync_status(void);

// An announced leap second from elsewhere (the ESP's choice of GNSS or its
// leap data): used whenever the selected source announces none, including
// in holdover and free run. change 0 withdraws it.
void sync_set_leap(const UtcLeap *leap);

// A +/- bound on the UTC error of our epoch, in ns: the selected source's
// bound and the measured phase, or the holdover bound. False when there is
// none to give: time unknown, or free running on nothing better than a count
// (free running on the RTC's time gives the RTC's bound).
bool sync_uncertainty(uint32_t *uncertainty_ns);
// Locked or in holdover, with a bound within SYNC_SYNCHRONISED_LIMIT_NS.
bool sync_is_synchronised(void);
// False for a source of the time of day only (the RTC), which is judged and
// can seed the time but is never a candidate to steer by.
bool sync_source_steers(uint8_t source);
// A candidate's bound this epoch (a SyncSourceId or SYNC_CANDIDATE_HOLDOVER),
// or SYNC_BOUND_NONE if it is not a candidate.
uint32_t sync_candidate_bound(uint8_t candidate);
// Its place in the ranking, from 1, or 0 if it is not a candidate.
uint8_t sync_candidate_rank(uint8_t candidate);
// The latest state change, or NULL if none yet.
const SyncTransition *sync_last_transition(void);

const char *sync_state_name(SyncState state);
// A SyncSourceId, SYNC_CANDIDATE_HOLDOVER or SYNC_SOURCE_NONE.
const char *sync_source_name(uint8_t source);
const char *sync_reason_name(uint8_t reason);

#endif // SYNC_H
