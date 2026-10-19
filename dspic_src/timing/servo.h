/*
 * Epoch phase servo and fractional period synthesis.
 *
 * Everything is expressed as an offset from the nominal period, in Q16.16
 * timer ticks held in int32_t:
 *
 *     commanded offset = Kalman offset - PI correction(phase error)
 *     period (ticks)   = nominal + commanded offset, dithered so the
 *                        fractional part is honoured on average
 *
 * Phase error is also Q16, so a sub-tick calibration can be applied before
 * the servo sees it (see servo_phase_error_q16()).
 */
#ifndef SERVO_H
#define SERVO_H

#include <stdbool.h>
#include <stdint.h>

#include "q16.h"

// Gain in Q16, from a ratio: EPOCH_Q16_GAIN(1, 8) = 0.125.
#define EPOCH_Q16_GAIN(numerator, denominator) Q16_RATIO(numerator, denominator)

/*
 * Measured phase = OC capture - PPS capture. The OC edge is synchronous with
 * the capture clock, so it is captured exactly; the PPS edge is asynchronous,
 * so its capture is the true time rounded down to a tick, on average 0.5 tick
 * early. A measured phase of 0 therefore means the epoch is on average half a
 * tick (12.5 ns) early. Subtracting half a tick removes that bias. Any fixed
 * delay measured on the hardware (pin, buffer, cable) belongs here as well.
 */
#define EPOCH_PHASE_CALIBRATION_Q16 Q16_HALF

typedef struct
{
    // Gains, Q16: period ticks of correction per tick of phase error.
    int32_t kp_q16;
    int32_t ki_q16;

    // Limits, Q16 ticks.
    int32_t integral_limit_q16;
    int32_t correction_limit_q16;

    // State, Q16 ticks. The integral is the servo's estimate of the Kalman
    // filter's frequency error, so it stays valid across a phase step.
    int32_t integral_q16;

    // Last outputs, for diagnostics.
    int32_t proportional_q16;
    int32_t correction_q16;

    bool enabled;
} EpochPiServo;

// Phase error in Q16 ticks: positive = epoch late. Whole-tick measurements
// are clamped to +/-Q16_TICK_LIMIT before conversion.
int32_t servo_phase_error_q16(int32_t measured_ticks, int32_t calibration_q16);

/*
 * Phase one epoch ahead, Q16 ticks.
 *
 * A period requested after measuring epoch N is loaded by the epoch N+1
 * interrupt, so the first edge it can move is N+2: the interval N -> N+1 is
 * already running on the period loaded at N. Whatever is commanded now acts
 * on the phase at N+1, which is already determined:
 *
 *     phase(N+1) = phase(N) + (running period - true period)
 *
 * with the true period estimated by the Kalman filter. Feeding the servo
 * this prediction instead of the measured phase(N) removes the one-epoch
 * transport delay from the loop. Without it, a step lands one second of
 * frequency error away from zero (76 ticks for a -1.9 ppm crystal), and the
 * PI loop overshoots and winds its integral up recovering from it.
 *
 * running_offset_ticks: period loaded for the current interval - nominal.
 * The result is clamped to +/-Q16_TICK_LIMIT ticks.
 */
int32_t servo_phase_predict_q16(int32_t phase_error_q16, int32_t running_offset_ticks,
                                int32_t kalman_offset_q16);

/*
 * Phase measurement gate.
 *
 * The Kalman filter gates PPS *periods*, but the phase loop sees each PPS
 * edge directly: without a gate, one edge 5 us late moves the output by
 * Kp x 5 us (625 ns), and one more than the acquisition limit away throws
 * the clock back into acquisition and steps it.
 *
 * The gate works on the predicted phase (servo_phase_predict_q16), which
 * the servo holds at zero, so the expected value is zero whatever the
 * period being run. A measurement outside +/-limit is rejected: the servo
 * is not updated and the epoch runs in holdover. reject_limit rejections in
 * a row mean the reference itself has moved (a receiver time step, a new
 * antenna position, a cold restart), and the caller re-acquires.
 */
typedef enum
{
    EPOCH_PHASE_ACCEPTED,
    EPOCH_PHASE_REJECTED, // outlier: hold over this epoch
    EPOCH_PHASE_LOST      // reject_limit outliers in a row: re-acquire
} EpochPhaseGateResult;

typedef struct
{
    int32_t limit_q16;
    uint8_t reject_limit;
    uint8_t consecutive_rejects;
    uint32_t rejected; // total, for diagnostics
} EpochPhaseGate;

void servo_gate_init(EpochPhaseGate *gate, int32_t limit_ticks, uint8_t reject_limit);
// Clears the run of rejections; use on entering tracking.
void servo_gate_reset(EpochPhaseGate *gate);
EpochPhaseGateResult servo_gate_check(EpochPhaseGate *gate, int32_t predicted_phase_q16);

void servo_pi_init(EpochPiServo *servo, int32_t kp_q16, int32_t ki_q16,
                   int32_t integral_limit_ticks, int32_t correction_limit_ticks);
// Disabling freezes the integral.
void servo_pi_enable(EpochPiServo *servo, bool enabled);
// Clears all state, including the integral. Use when the reference changes.
void servo_pi_reset(EpochPiServo *servo);
// Clears the proportional output only and keeps the integral. Use after a
// phase step, where the frequency knowledge is still good.
void servo_pi_reset_phase(EpochPiServo *servo);
void servo_pi_preload(EpochPiServo *servo, int32_t integral_q16);

// One new phase measurement. Returns the correction, Q16 ticks.
int32_t servo_pi_update(EpochPiServo *servo, int32_t phase_error_q16);

// Tracking: Kalman offset minus the PI correction for this measurement.
int32_t servo_offset(EpochPiServo *servo, int32_t kalman_offset_q16, int32_t phase_error_q16);
// Holdover: no phase measurement this epoch. Keeps running at the best
// frequency estimate, Kalman offset minus the integral, with no
// proportional term, and leaves the servo state untouched.
int32_t servo_holdover_offset(const EpochPiServo *servo, int32_t kalman_offset_q16);

typedef struct
{
    // Fractional ticks owed, Q0.16. The integer part of each period goes to
    // PR3:PR2; fractions accumulate until a whole tick is due.
    uint16_t accumulator_q16;
} EpochFractionalAccumulator;

void servo_fraction_init(EpochFractionalAccumulator *accumulator);
void servo_fraction_reset(EpochFractionalAccumulator *accumulator);
// Timer period, in ticks, for one epoch at nominal + offset. Over any run of
// calls the sum of returned periods equals the sum of the requested periods
// to within one tick.
uint32_t servo_fraction_next(EpochFractionalAccumulator *accumulator, uint32_t nominal_period,
                             int32_t offset_q16);

#endif // SERVO_H
