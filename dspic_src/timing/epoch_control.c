#include "epoch_control.h"

#include <stddef.h>

#include "epoch_timer.h"
#include "timing_config.h"

/*
 * The precise profile. Kp 1/8 and Ki 1/256 (Ki = Kp^2/4) make a critically
 * damped loop with a time constant of about 8 s.
 *
 * Phase gate (see EpochPhaseGate). In track the predicted phase sits within
 * a tick or two of zero: PPS jitter is well under a tick RMS and the servo
 * follows temperature to a fraction of a tick. 16 ticks (400 ns) is beyond
 * any plausible receiver jitter, yet an outlier just inside it moves the
 * output by at most Kp x 16 = 2 ticks (50 ns). Four rejections in a row
 * re-acquire.
 */
const EpochProfile epoch_profile_precise = {
    "precise", EPOCH_Q16_GAIN(1, 8), EPOCH_Q16_GAIN(1, 256), 8, 16, 16, 4U, true,
};

// Usable reference epochs before the first step, so the Kalman filter has a
// few measurements.
#define EPOCH_ACQUIRE_DELAY 3UL

// Consecutive reference periods averaged to seed the Kalman filter (see
// epoch_control.h). The ESP PPS period jitters by ~60 ticks RMS: over 16 s,
// the average is good to a few ticks, ~0.1 ppm.
#define EPOCH_SEED_PERIODS 16UL

static EpochControl loop;

// --------------------------------------------------------------------------
// Helpers
// --------------------------------------------------------------------------

// Phase folded into [-period/2, +period/2).
static int32_t wrap_phase(int32_t phase, uint32_t nominal_period)
{
    int32_t period = (int32_t)nominal_period;
    int32_t half_period = period / 2;

    if (phase >= half_period)
    {
        phase -= period;
    }
    else if (phase < -half_period)
    {
        phase += period;
    }
    return phase;
}


// One interval that moves the epoch by -offset_ticks, if it is a sane
// length (half a second to one and a half).
static bool compute_step(uint32_t base_period, int32_t offset_ticks, uint32_t *step_period)
{
    uint32_t period;

    if (offset_ticks > 0)
    {
        period = base_period - (uint32_t)offset_ticks;
    }
    else
    {
        period = base_period + (uint32_t)(-offset_ticks);
    }

    if (period < TIMING_HALF_SECOND_TICKS ||
        period > TIMING_HALF_SECOND_TICKS + TIMING_NOMINAL_TICKS)
    {
        return false;
    }
    *step_period = period;
    return true;
}


// learn: the reference is usable. Either way the period is consumed, so a
// period measured while it was not is never learnt later.
static void update_kalman(const PpsCaptureSample *capture, bool learn)
{
    if (!capture->reference_period_valid ||
        capture->reference_period_sequence == loop.last_pps_sequence)
    {
        return;
    }
    loop.last_pps_sequence = capture->reference_period_sequence;
    if (!learn || loop.discard_period || !loop.profile->learn_frequency)
    {
        loop.discard_period = false;
        return;
    }
    // Rejected outliers leave the estimate unchanged.
    (void)kalman_update(&loop.pps_kf, capture->reference_period);
    loop.filtered_pps_period = kalman_period(&loop.pps_kf);
}


static void request_offset(int32_t offset_q16)
{
    loop.commanded_offset_q16 = offset_q16;
    loop.commanded_period = servo_fraction_next(&loop.fractional, TIMING_NOMINAL_TICKS, offset_q16);
    epoch_timer_set_period(loop.commanded_period);
}


// The best frequency estimate, with no phase correction.
static void request_frequency(void)
{
    request_offset(servo_holdover_offset(&loop.pi, kalman_phase_q16(&loop.pps_kf)));
}


static void request_holdover(void)
{
    loop.holdover_epochs++;
    request_frequency();
}


static void enter_track(uint32_t sequence)
{
    // Keep the integral: it is the servo's estimate of the Kalman filter's
    // frequency error, and a phase step does not change it.
    servo_pi_reset_phase(&loop.pi);
    servo_fraction_reset(&loop.fractional);
    servo_pi_enable(&loop.pi, true);
    servo_gate_reset(&loop.gate);
    loop.state = EPOCH_STATE_TRACK;
    loop.last_resync = sequence;
}


static void enter_acquire(void)
{
    // A phase excursion does not invalidate the integral (see
    // enter_track); use servo_pi_reset() when the reference changes.
    servo_pi_enable(&loop.pi, false);
    servo_pi_reset_phase(&loop.pi);
    servo_fraction_reset(&loop.fractional);
    loop.state = EPOCH_STATE_ACQUIRE;
}


// --------------------------------------------------------------------------
// States
// --------------------------------------------------------------------------

// Seeds the Kalman filter if the profile does not teach it and nothing has
// yet, from the capture-to-capture baseline over EPOCH_SEED_PERIODS
// consecutive periods (exact: the captures telescope). True once there is
// an estimate to step with.
static bool seed_frequency(const PpsCaptureSample *capture)
{
    uint32_t span;
    int64_t excess_q16;

    if (loop.profile->learn_frequency || loop.pps_kf.initialized)
    {
        return true;
    }
    if (!capture->reference_capture_valid)
    {
        return false;
    }
    if (!loop.seed_started || !capture->reference_period_valid ||
        capture->reference_period_sequence != loop.seed_sequence + 1U)
    {
        loop.seed_started = true;
        loop.seed_first = capture->reference_capture;
        loop.seed_periods = 0;
    }
    else
    {
        loop.seed_periods++;
    }
    loop.seed_sequence = capture->reference_period_sequence;
    if (loop.seed_periods < EPOCH_SEED_PERIODS)
    {
        return false;
    }
    span = capture->reference_capture - loop.seed_first;
    excess_q16 =
        ((int64_t)span - (int64_t)loop.seed_periods * (int64_t)TIMING_NOMINAL_TICKS) * 65536;
    kalman_seed(&loop.pps_kf, (int32_t)(excess_q16 / (int64_t)loop.seed_periods));
    loop.filtered_pps_period = kalman_period(&loop.pps_kf);
    return true;
}


// Requests either the step or, until it can be made, the frequency estimate.
static void acquire(const TimingSnapshot *snapshot, int32_t utc_offset_ticks)
{
    uint32_t step_period;
    int32_t predicted_ticks;

    if (loop.reference_epochs < EPOCH_ACQUIRE_DELAY)
    {
        request_frequency();
        return;
    }

    // The step can only start at the next epoch. Aim it at where that epoch
    // will be: this phase plus whatever the running interval adds against
    // the true second. Whole ticks here; the PI loop removes the fraction.
    predicted_ticks = wrap_phase(
        utc_offset_ticks + (int32_t)(snapshot->timer.running_period - loop.filtered_pps_period),
        loop.filtered_pps_period);

    loop.predicted_phase_q16 = 0;
    if (compute_step(loop.filtered_pps_period, predicted_ticks, &step_period))
    {
        loop.commanded_period = step_period;
        epoch_timer_set_period(step_period);
        loop.state = EPOCH_STATE_STEP_ACTIVE;
    }
    else
    {
        request_frequency();
    }
}


// The step is loaded and runs over the next interval: tracking starts with
// the first measurement of the stepped edge. Done whatever this epoch's
// captures; the caller requests the period after the step, or the step
// would run again.
static void follow_step(const TimingSnapshot *snapshot)
{
    enter_track(snapshot->timer.sequence);
}


static EpochUpdateKind track(const TimingSnapshot *snapshot, int32_t utc_offset_ticks)
{
    int32_t running_offset_ticks = (int32_t)(snapshot->timer.running_period - TIMING_NOMINAL_TICKS);
    int32_t kalman_offset_q16 = kalman_phase_q16(&loop.pps_kf);

    // The phase is wrapped to +/-half a second, so a wild edge cannot
    // overflow the Q16 conversion: it clamps, and the gate rejects it.
    // Predicted at the loop's own best estimate of the true period, the
    // Kalman estimate less the integral: with the Kalman estimate alone, the
    // phase would settle wherever the integral is (in ticks), as the servo
    // holds the prediction, not the phase, at zero.
    loop.predicted_phase_q16 = servo_phase_predict_q16(
        servo_phase_error_q16(utc_offset_ticks, EPOCH_PHASE_CALIBRATION_Q16), running_offset_ticks,
        servo_holdover_offset(&loop.pi, kalman_offset_q16));

    switch (servo_gate_check(&loop.gate, loop.predicted_phase_q16))
    {
        case EPOCH_PHASE_ACCEPTED:
            loop.holdover_epochs = 0;
            request_offset(servo_offset(&loop.pi, kalman_offset_q16, loop.predicted_phase_q16));
            return EPOCH_UPDATE_MEASURED;

        case EPOCH_PHASE_REJECTED:
            // An outlier edge: run this epoch as holdover.
            request_holdover();
            return EPOCH_UPDATE_HOLDOVER;

        case EPOCH_PHASE_LOST:
        default:
            // Consistently somewhere else: the reference moved.
            enter_acquire();
            request_frequency();
            return EPOCH_UPDATE_MEASURED;
    }
}


// --------------------------------------------------------------------------
// Interface
// --------------------------------------------------------------------------

void epoch_control_init(void)
{
    loop.state = EPOCH_STATE_ACQUIRE;
    loop.last_resync = 0;
    loop.last_pps_sequence = 0;
    loop.holdover_epochs = 0;
    loop.reference_epochs = 0;
    loop.discard_period = false;
    loop.seed_started = false;
    loop.filtered_pps_period = TIMING_NOMINAL_TICKS;
    loop.predicted_phase_q16 = 0;
    loop.commanded_offset_q16 = 0;
    loop.commanded_period = TIMING_NOMINAL_TICKS;

    loop.profile = &epoch_profile_precise;
    servo_pi_init(&loop.pi, loop.profile->kp_q16, loop.profile->ki_q16,
                  loop.profile->integral_limit_ticks, loop.profile->correction_limit_ticks);
    servo_pi_enable(&loop.pi, false);
    servo_gate_init(&loop.gate, loop.profile->gate_ticks, loop.profile->reject_limit);
    servo_fraction_init(&loop.fractional);
    kalman_init(&loop.pps_kf, TIMING_NOMINAL_TICKS, PPS_KF_Q, PPS_KF_R);
}


// With a usable reference edge. Holdover keeps synthesising the best
// frequency estimate, fractional part included: holding the last whole-tick
// period instead drifts up to 25 ns/s.
static EpochUpdateKind use_reference(const TimingSnapshot *snapshot, const EpochUpdate *update)
{
    loop.reference_epochs++;
    switch (loop.state)
    {
        case EPOCH_STATE_ACQUIRE:
            if (!update->phase_valid)
            {
                request_frequency();
                return EPOCH_UPDATE_INCOMPLETE;
            }
            if (!seed_frequency(&snapshot->capture))
            {
                request_frequency();
                return EPOCH_UPDATE_MEASURED;
            }
            acquire(snapshot, update->utc_offset_ticks);
            return EPOCH_UPDATE_MEASURED;

        case EPOCH_STATE_STEP_ACTIVE:
            follow_step(snapshot);
            request_frequency();
            return update->phase_valid ? EPOCH_UPDATE_MEASURED : EPOCH_UPDATE_INCOMPLETE;

        case EPOCH_STATE_TRACK:
            if (!update->phase_valid)
            {
                request_holdover();
                return EPOCH_UPDATE_HOLDOVER;
            }
            return track(snapshot, update->utc_offset_ticks);

        default: enter_acquire(); return EPOCH_UPDATE_INCOMPLETE;
    }
}


// Without one: hold the frequency, and never learn or steer.
static EpochUpdateKind hold(const TimingSnapshot *snapshot)
{
    switch (loop.state)
    {
        case EPOCH_STATE_STEP_ACTIVE:
            follow_step(snapshot);
            request_holdover();
            return EPOCH_UPDATE_HOLDOVER;

        case EPOCH_STATE_TRACK: request_holdover(); return EPOCH_UPDATE_HOLDOVER;

        case EPOCH_STATE_ACQUIRE:
        default: request_frequency(); return EPOCH_UPDATE_FREE_RUN;
    }
}


EpochUpdate epoch_control_update(const TimingSnapshot *snapshot, bool reference_usable,
                                 int32_t reference_offset_ticks)
{
    const PpsCaptureSample *capture = &snapshot->capture;
    EpochUpdate update;

    update_kalman(capture, reference_usable);
    if (!reference_usable)
    {
        // The first period once it is usable again may start at an edge
        // that was not.
        loop.discard_period = true;
    }

    update.phase_valid = capture->reference_capture_valid && capture->epoch_capture_valid;
    update.utc_offset_ticks =
        update.phase_valid
            ? wrap_phase((int32_t)(capture->epoch_capture - capture->reference_capture) +
                             reference_offset_ticks,
                         loop.filtered_pps_period)
            : 0;
    update.kind = reference_usable ? use_reference(snapshot, &update) : hold(snapshot);
    return update;
}


void epoch_control_restart(const EpochProfile *profile, bool slew)
{
    if (profile != NULL)
    {
        loop.profile = profile;
    }
    servo_pi_set_gains(&loop.pi, loop.profile->kp_q16, loop.profile->ki_q16,
                       loop.profile->integral_limit_ticks, loop.profile->correction_limit_ticks);
    servo_gate_set_limits(&loop.gate, loop.profile->gate_ticks, loop.profile->reject_limit);
    servo_gate_reset(&loop.gate);
    loop.reference_epochs = 0;
    loop.holdover_epochs = 0;
    loop.discard_period = true;
    loop.seed_started = false;
    if (slew && loop.state == EPOCH_STATE_TRACK)
    {
        servo_pi_reset_phase(&loop.pi);
        return;
    }
    enter_acquire();
    servo_pi_reset(&loop.pi);
}


int32_t epoch_control_holdover_offset_q16(void)
{
    return servo_holdover_offset(&loop.pi, kalman_phase_q16(&loop.pps_kf));
}


const EpochControl *epoch_control_state(void)
{
    return &loop;
}


const char *epoch_control_state_name(EpochSyncState state)
{
    switch (state)
    {
        case EPOCH_STATE_ACQUIRE: return "ACQ";
        case EPOCH_STATE_STEP_ACTIVE: return "STEP";
        case EPOCH_STATE_TRACK: return "TRACK";
        default: return "?";
    }
}
