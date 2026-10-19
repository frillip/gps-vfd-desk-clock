/*
 * Synchroniser: chooses the time source, decides when the epoch control loop
 * may use it, and keeps the UTC second of our epoch. It sits on top of
 * timing/ and runs once per epoch:
 *
 *   timing_snapshot() -> sync_update() -+-> each source adapter -> judged
 *                                       +-> state machine
 *                                       +-> epoch_control_update(usable)
 *                                       +-> UTC label of our epoch
 *
 * States (the source in brackets is the selected one):
 *
 *   FREE_RUN       no source: the epoch runs on the last frequency estimate,
 *                  and the time of day (if ever known) is counted on.
 *   ACQUIRE(s)     s has qualified: the loop learns its frequency and steps
 *                  onto its edge.
 *   LOCKED(s)      the loop tracks s.
 *   HOLDOVER(s)    s has failed: the loop holds frequency. Still counted as
 *                  synchronised until SYNC_HOLDOVER_LIMIT_EPOCHS.
 *
 *   FREE_RUN  -> ACQUIRE   a source has been GOOD for SYNC_QUALIFY_EPOCHS
 *   ACQUIRE   -> LOCKED    the loop measures in TRACK after its step
 *   ACQUIRE   -> FREE_RUN  s fails before the loop locks
 *   LOCKED    -> ACQUIRE   the loop loses phase and re-acquires
 *   LOCKED    -> HOLDOVER  s fails
 *   HOLDOVER  -> LOCKED    s is GOOD again (the loop's phase gate decides
 *                          whether it is still in phase)
 *   HOLDOVER  -> ACQUIRE   another source has qualified
 *   HOLDOVER  -> FREE_RUN  SYNC_HOLDOVER_LIMIT_EPOCHS have passed
 *   ACQUIRE/LOCKED(s) -> ACQUIRE(t)
 *                          t ranks above s and has been GOOD for
 *                          SYNC_SWITCH_EPOCHS
 *
 * Sources rank by their order in SyncSourceId: earlier is preferred. Adding
 * one takes an id here, an adapter (see source_gnss.c) and a row in the
 * table in sync.c. Only one source at a time can own the IC1/IC2 capture
 * pair that the loop steers by; a source that needs it re-routes the pair in
 * its select hook (pps_capture), and the loop is restarted on every change.
 */
#ifndef SYNC_H
#define SYNC_H

#include <stdbool.h>
#include <stdint.h>

#include "../timing/epoch_control.h"
#include "../timing/timing.h"
#include "sync_source.h"

// In order of preference.
typedef enum
{
    SYNC_SOURCE_GNSS = 0,
    SYNC_SOURCE_COUNT
} SyncSourceId;

#define SYNC_SOURCE_NONE SYNC_SOURCE_COUNT

// Consecutive GOOD epochs before a source is selected.
#define SYNC_QUALIFY_EPOCHS 3U
// ... before a better-ranked source displaces a working one.
#define SYNC_SWITCH_EPOCHS 10U
// Holdover still counts as synchronised for this long (1 h). Set by how far
// the oscillator wanders unsteered; the loop re-steps afterwards if needed.
#ifndef SYNC_HOLDOVER_LIMIT_EPOCHS
#define SYNC_HOLDOVER_LIMIT_EPOCHS 3600UL
#endif
// State changes kept for diagnostics.
#define SYNC_HISTORY_LENGTH 8U
// How fast the time error may grow in holdover, for the uncertainty bound
// reported to the ESP: the Kalman estimate holds the oscillator to well
// within 0.1 ppm over an hour, unless the temperature changes a lot.
#ifndef SYNC_HOLDOVER_DRIFT_NS_PER_S
#define SYNC_HOLDOVER_DRIFT_NS_PER_S 100UL
#endif
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
    SYNC_REASON_BETTER_SOURCE, // a better-ranked source took over
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
} SyncTime;

typedef struct
{
    SyncState state;
    uint8_t source;        // SyncSourceId, or SYNC_SOURCE_NONE
    uint32_t state_epochs; // epochs in this state
    uint32_t holdover_epochs;
    SyncSourceTracker sources[SYNC_SOURCE_COUNT];
    SyncTime time;
    UtcLeap link_leap; // announced by the ESP (SML-TIME-LEAP), if any

    // The latest epoch: our epoch minus the selected source's edge.
    uint32_t epochs; // processed since sync_init()
    bool phase_valid;
    int32_t phase_ticks;

    uint32_t transitions; // total; the latest are in history
    SyncTransition history[SYNC_HISTORY_LENGTH];
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

// A +/- bound on the UTC error of our epoch, in ns: the selected source's own
// uncertainty, the capture resolution and the measured phase, growing by
// SYNC_HOLDOVER_DRIFT_NS_PER_S in holdover. False when there is none to give
// (time unknown, or free running).
bool sync_uncertainty(uint32_t *uncertainty_ns);
// Locked, or in holdover within its limit.
bool sync_is_synchronised(void);
// The latest state change, or NULL if none yet.
const SyncTransition *sync_last_transition(void);

const char *sync_state_name(SyncState state);
const char *sync_source_name(uint8_t source);
const char *sync_reason_name(uint8_t reason);

#endif // SYNC_H
