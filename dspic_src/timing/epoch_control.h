/*
 * The epoch control loop: decides the length of each next second.
 *
 *   ACQUIRE  wait for a few reference periods, then step the epoch onto the
 *            reference edge in one interval;
 *   STEP     the step is loaded; follow it at the estimated frequency;
 *   TRACK    steer the phase with the PI servo on top of the Kalman
 *            frequency estimate, gating outliers; fall back to ACQUIRE if
 *            the reference has really moved.
 *
 * The reference is the edge captured by IC1/IC2. Whether it may be used is
 * decided above this loop (sync/): when it may not, the loop neither learns
 * from it nor steers to it, and holds the frequency it has (holdover once
 * tracking, free run before). It still reports the measured phase.
 *
 * How hard the loop follows its reference depends on the reference: a
 * profile (EpochProfile) sets the servo gains and limits, the phase gate,
 * and whether reference periods may teach the Kalman filter the oscillator's
 * frequency. GNSS edges are good to a few ns and steer tightly; the ESP's
 * NTP-aligned edges carry microseconds of jitter and are slewed by the ESP
 * itself, so they steer gently and teach the filter nothing.
 *
 * A period requested after measuring epoch N runs from epoch N+1, so the
 * loop acts on the phase predicted for N+1 (servo_phase_predict_q16). A
 * period is requested every epoch, so nothing ever runs twice by default.
 */
#ifndef EPOCH_CONTROL_H
#define EPOCH_CONTROL_H

#include <stdbool.h>
#include <stdint.h>

#include "servo.h"
#include "kalman.h"
#include "timing.h"

// How the loop treats its reference (see above).
typedef struct
{
    const char *name;
    int32_t kp_q16; // PI gains, Q16
    int32_t ki_q16;
    int32_t integral_limit_ticks;   // the integral: frequency the servo may add
    int32_t correction_limit_ticks; // the whole correction: the fastest slew
    int32_t gate_ticks;             // phase gate (servo.h)
    uint8_t reject_limit;           // ... rejections in a row before re-acquiring
    bool learn_frequency;           // reference periods feed the Kalman filter
} EpochProfile;

// GNSS-grade: the loop as tuned against a NEO-M9N.
extern const EpochProfile epoch_profile_precise;

typedef enum
{
    EPOCH_STATE_ACQUIRE = 0,
    EPOCH_STATE_STEP_ACTIVE,
    EPOCH_STATE_TRACK
} EpochSyncState;

typedef enum
{
    EPOCH_UPDATE_MEASURED,  // phase measured against the reference and acted on
    EPOCH_UPDATE_HOLDOVER,  // tracking without a usable edge: frequency held
    EPOCH_UPDATE_FREE_RUN,  // not tracking, no usable reference: frequency held
    EPOCH_UPDATE_INCOMPLETE // reference usable, but a capture is missing
} EpochUpdateKind;

typedef struct
{
    EpochUpdateKind kind;
    bool phase_valid;         // both captures in, whatever the kind
    int32_t utc_offset_ticks; // epoch - reference edge, when phase_valid
} EpochUpdate;

// The loop's whole state, readable for reporting and tests.
typedef struct
{
    EpochSyncState state;
    uint32_t last_resync;       // epoch sequence when tracking began
    uint32_t last_pps_sequence; // last reference period fed to the filter
    uint32_t holdover_epochs;   // consecutive epochs without a usable edge
    uint32_t reference_epochs;  // usable epochs since the last restart
    bool discard_period;        // the next period spans a reference change
    const EpochProfile *profile;

    KalmanFilter pps_kf;
    EpochPiServo pi;
    EpochPhaseGate gate;
    EpochFractionalAccumulator fractional;

    uint32_t filtered_pps_period; // Kalman estimate, whole ticks
    int32_t predicted_phase_q16;  // phase acted on, Q16 ticks
    int32_t commanded_offset_q16; // period - nominal, Q16 ticks
    uint32_t commanded_period;    // whole ticks, as requested
} EpochControl;

void epoch_control_init(void);

/*
 * One epoch: feeds the filter, runs the state machine and requests the next
 * period from the epoch timer. reference_usable: the captured reference edge
 * may be learnt from and steered to this epoch. reference_offset_ticks: the
 * reference edge minus the UTC second it marks, as far as its source knows
 * (0 for GNSS; the ESP's own estimate of its edge for NTP). The loop steers
 * the epoch to the edge minus this offset.
 */
EpochUpdate epoch_control_update(const TimingSnapshot *snapshot, bool reference_usable,
                                 int32_t reference_offset_ticks);

/*
 * A different reference from now on, followed with profile. The Kalman
 * frequency estimate is kept either way: it describes the oscillator, not
 * the reference.
 *
 * slew false: drops the phase servo's state, including its integral, and
 * acquires afresh, stepping onto the new edge.
 * slew true, and only if the loop is tracking: carries on tracking the new
 * reference, keeping the integral, so the servo slews the epoch onto it at
 * no more than the profile's correction limit. For a reference already
 * within the profile's gate of the epoch: a handover with no step.
 */
void epoch_control_restart(const EpochProfile *profile, bool slew);

// The offset from nominal the loop would hold in holdover now: the Kalman
// estimate less the servo integral, Q16 ticks.
int32_t epoch_control_holdover_offset_q16(void);

const EpochControl *epoch_control_state(void);
const char *epoch_control_state_name(EpochSyncState state);

#endif // EPOCH_CONTROL_H
