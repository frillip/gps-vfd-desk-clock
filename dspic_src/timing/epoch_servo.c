#include "epoch_servo.h"

#include <stddef.h>

int32_t epoch_phase_error_q16(int32_t measured_ticks, int32_t calibration_q16)
{
    int32_t clamped = q16_clamp(measured_ticks, Q16_TICK_LIMIT);
    return Q16_FROM_TICKS(clamped) - calibration_q16;
}


int32_t epoch_phase_predict_q16(int32_t phase_error_q16, int32_t running_offset_ticks,
                                int32_t kalman_offset_q16)
{
    const int64_t limit_q16 = (int64_t)Q16_FROM_TICKS(Q16_TICK_LIMIT);
    // Three terms of up to 2^30 each: sum in 64 bits, then clamp.
    int64_t predicted_q16 =
        (int64_t)phase_error_q16 +
        (int64_t)Q16_FROM_TICKS(q16_clamp(running_offset_ticks, Q16_TICK_LIMIT)) -
        (int64_t)kalman_offset_q16;

    if (predicted_q16 > limit_q16)
    {
        predicted_q16 = limit_q16;
    }
    else if (predicted_q16 < -limit_q16)
    {
        predicted_q16 = -limit_q16;
    }
    return (int32_t)predicted_q16;
}


void epoch_phase_gate_init(EpochPhaseGate *gate, int32_t limit_ticks, uint8_t reject_limit)
{
    if (gate == NULL)
    {
        return;
    }
    if (limit_ticks < 0)
    {
        limit_ticks = -limit_ticks;
    }
    gate->limit_q16 = Q16_FROM_TICKS(q16_clamp(limit_ticks, Q16_TICK_LIMIT));
    gate->reject_limit = (reject_limit == 0U) ? 1U : reject_limit;
    gate->rejected = 0;
    epoch_phase_gate_reset(gate);
}


void epoch_phase_gate_reset(EpochPhaseGate *gate)
{
    if (gate != NULL)
    {
        gate->consecutive_rejects = 0;
    }
}


EpochPhaseGateResult epoch_phase_gate_check(EpochPhaseGate *gate, int32_t predicted_phase_q16)
{
    if (gate == NULL)
    {
        return EPOCH_PHASE_ACCEPTED;
    }
    if (predicted_phase_q16 <= gate->limit_q16 && predicted_phase_q16 >= -gate->limit_q16)
    {
        gate->consecutive_rejects = 0;
        return EPOCH_PHASE_ACCEPTED;
    }
    gate->rejected++;
    if (++gate->consecutive_rejects >= gate->reject_limit)
    {
        gate->consecutive_rejects = 0;
        return EPOCH_PHASE_LOST;
    }
    return EPOCH_PHASE_REJECTED;
}


void epoch_pi_init(EpochPiServo *servo, int32_t kp_q16, int32_t ki_q16,
                   int32_t integral_limit_ticks, int32_t correction_limit_ticks)
{
    if (servo == NULL)
    {
        return;
    }
    if (integral_limit_ticks < 0)
    {
        integral_limit_ticks = -integral_limit_ticks;
    }
    if (correction_limit_ticks < 0)
    {
        correction_limit_ticks = -correction_limit_ticks;
    }
    // Gains above 1.0 would make a full-range phase error overflow the
    // correction; they would also make the loop unstable.
    servo->kp_q16 = (kp_q16 < 0) ? 0 : q16_clamp(kp_q16, Q16_ONE);
    servo->ki_q16 = (ki_q16 < 0) ? 0 : q16_clamp(ki_q16, Q16_ONE);
    servo->integral_limit_q16 = Q16_FROM_TICKS(q16_clamp(integral_limit_ticks, Q16_TICK_LIMIT));
    servo->correction_limit_q16 = Q16_FROM_TICKS(q16_clamp(correction_limit_ticks, Q16_TICK_LIMIT));
    servo->enabled = false;
    epoch_pi_reset(servo);
}


void epoch_pi_enable(EpochPiServo *servo, bool enabled)
{
    if (servo != NULL)
    {
        servo->enabled = enabled;
    }
}


void epoch_pi_reset(EpochPiServo *servo)
{
    if (servo == NULL)
    {
        return;
    }
    servo->integral_q16 = 0;
    epoch_pi_reset_phase(servo);
}


void epoch_pi_reset_phase(EpochPiServo *servo)
{
    if (servo == NULL)
    {
        return;
    }
    servo->proportional_q16 = 0;
    servo->correction_q16 = servo->integral_q16;
}


void epoch_pi_preload(EpochPiServo *servo, int32_t integral_q16)
{
    if (servo != NULL)
    {
        servo->integral_q16 = q16_clamp(integral_q16, servo->integral_limit_q16);
    }
}


int32_t epoch_pi_update(EpochPiServo *servo, int32_t phase_error_q16)
{
    int32_t proportional_q16;
    int32_t integral_step_q16;
    int32_t candidate_integral_q16;
    int32_t candidate_correction_q16;

    if (servo == NULL || !servo->enabled)
    {
        return 0;
    }

    proportional_q16 = q16_multiply(servo->kp_q16, phase_error_q16);
    integral_step_q16 = q16_multiply(servo->ki_q16, phase_error_q16);

    // Both terms are at most the tick limit, so the sums fit int32_t.
    candidate_integral_q16 =
        q16_clamp(servo->integral_q16 + integral_step_q16, servo->integral_limit_q16);
    candidate_correction_q16 = proportional_q16 + candidate_integral_q16;

    // Conditional integration: do not integrate further into saturation.
    if (!((candidate_correction_q16 > servo->correction_limit_q16 && integral_step_q16 > 0) ||
          (candidate_correction_q16 < -servo->correction_limit_q16 && integral_step_q16 < 0)))
    {
        servo->integral_q16 = candidate_integral_q16;
    }

    servo->proportional_q16 = proportional_q16;
    servo->correction_q16 =
        q16_clamp(proportional_q16 + servo->integral_q16, servo->correction_limit_q16);
    return servo->correction_q16;
}


int32_t epoch_servo_offset_command(EpochPiServo *servo, int32_t kalman_offset_q16,
                                   int32_t phase_error_q16)
{
    // Positive phase error means the epoch is late: a positive correction
    // shortens the period.
    return kalman_offset_q16 - epoch_pi_update(servo, phase_error_q16);
}


int32_t epoch_servo_holdover_offset(const EpochPiServo *servo, int32_t kalman_offset_q16)
{
    if (servo == NULL || !servo->enabled)
    {
        return kalman_offset_q16;
    }
    return kalman_offset_q16 - servo->integral_q16;
}


void epoch_fractional_init(EpochFractionalAccumulator *accumulator)
{
    if (accumulator != NULL)
    {
        accumulator->accumulator_q16 = 0;
    }
}


void epoch_fractional_reset(EpochFractionalAccumulator *accumulator)
{
    epoch_fractional_init(accumulator);
}


uint32_t epoch_fractional_next(EpochFractionalAccumulator *accumulator, uint32_t nominal_period,
                               int32_t offset_q16)
{
    int32_t whole_offset;
    uint16_t fraction;
    uint32_t owed;
    uint32_t period;

    if (accumulator == NULL)
    {
        return nominal_period;
    }
    q16_split(offset_q16, &whole_offset, &fraction);
    period = nominal_period + (uint32_t)whole_offset;

    owed = (uint32_t)accumulator->accumulator_q16 + fraction;
    if (owed >= (uint32_t)Q16_ONE)
    {
        owed -= (uint32_t)Q16_ONE;
        period++;
    }
    accumulator->accumulator_q16 = (uint16_t)owed;
    return (period != 0U) ? period : UINT32_C(1);
}
