#include "kalman.h"

#include <stddef.h>

// Normalised covariance is capped at 2.0, so cap + Q/R (Q <= R) fits in
// 32 bits. Only reachable after a long run of rejections.
#define VARIANCE_RATIO_MAX (2U * KALMAN_Q30_ONE)

// K = p / (1 + p), Q2.30 in and out.
static uint32_t gain_from_ratio(uint32_t p_minus_q30)
{
    uint64_t numerator = (uint64_t)p_minus_q30 << 30;
    uint32_t denominator = p_minus_q30 + KALMAN_Q30_ONE; // <= 3.0: fits

    return (uint32_t)((numerator + (denominator >> 1)) / denominator);
}


// True if the innovation is within KALMAN_GATE_SIGMA standard deviations.
// Innovation variance S = P- + R = R (1 + p-). Compared squared, so no
// square root: innovation^2 <= G^2 * R * (1 + p-), in Q32.
static bool within_gate(const KalmanFilter *kf, int32_t innovation_q16, uint32_t p_minus_q30)
{
    uint64_t magnitude =
        (uint64_t)(innovation_q16 < 0 ? -(int64_t)innovation_q16 : (int64_t)innovation_q16);
    uint64_t innovation_squared_q32 = magnitude * magnitude;
    uint64_t one_plus_p_q16 = ((uint64_t)KALMAN_Q30_ONE + p_minus_q30) >> 14;
    uint64_t variance_q32 = (uint64_t)kf->measurement_variance_q16 * one_plus_p_q16;
    uint64_t gate_squared = (uint64_t)KALMAN_GATE_SIGMA * KALMAN_GATE_SIGMA;

    return innovation_squared_q32 <= gate_squared * variance_q32;
}


static void acquire(KalmanFilter *kf, int32_t measured_offset_q16)
{
    kf->offset_q16 = measured_offset_q16;
    kf->variance_ratio_q30 = KALMAN_Q30_ONE; // P = R: one measurement's worth
    kf->gain_q30 = KALMAN_Q30_ONE;
    kf->consecutive_rejects = 0;
    kf->initialized = true;
}


static KalmanResult reject(KalmanFilter *kf)
{
    kf->stats.rejected++;
    if (++kf->consecutive_rejects >= KALMAN_REJECT_LIMIT)
    {
        kf->initialized = false;
        kf->stats.reacquisitions++;
        return KALMAN_REACQUIRING;
    }
    return KALMAN_REJECTED;
}


bool kalman_init(KalmanFilter *kf, uint32_t nominal_period, uint32_t process_variance_q16,
                 uint32_t measurement_variance_q16)
{
    if (kf == NULL || nominal_period == 0U || measurement_variance_q16 == 0U ||
        process_variance_q16 > measurement_variance_q16)
    {
        return false;
    }
    kf->nominal_period = nominal_period;
    kf->measurement_variance_q16 = measurement_variance_q16;
    kf->process_ratio_q30 =
        (uint32_t)((((uint64_t)process_variance_q16 << 30) + (measurement_variance_q16 / 2U)) /
                   measurement_variance_q16);
    kf->stats.accepted = 0;
    kf->stats.rejected = 0;
    kf->stats.reacquisitions = 0;
    kalman_reset(kf);
    return true;
}


void kalman_seed(KalmanFilter *kf, int32_t offset_q16)
{
    if (kf != NULL)
    {
        acquire(kf, offset_q16);
    }
}


void kalman_reset(KalmanFilter *kf)
{
    if (kf == NULL)
    {
        return;
    }
    kf->offset_q16 = 0;
    kf->variance_ratio_q30 = KALMAN_Q30_ONE;
    kf->gain_q30 = 0;
    kf->consecutive_rejects = 0;
    kf->initialized = false;
}


KalmanResult kalman_update(KalmanFilter *kf, uint32_t measured_period)
{
    // Two's complement difference: correct for any period near nominal.
    int32_t measured_offset = (int32_t)(measured_period - kf->nominal_period);
    int32_t measured_offset_q16;
    int32_t innovation_q16;
    uint32_t p_minus_q30;

    if (measured_offset > Q16_TICK_LIMIT || measured_offset < -Q16_TICK_LIMIT)
    {
        return reject(kf);
    }
    measured_offset_q16 = Q16_FROM_TICKS(measured_offset);

    if (!kf->initialized)
    {
        acquire(kf, measured_offset_q16);
        kf->stats.accepted++;
        return KALMAN_ACQUIRED;
    }

    // Predict: P- = P + Q, in units of R.
    p_minus_q30 = kf->variance_ratio_q30 + kf->process_ratio_q30;
    if (p_minus_q30 > VARIANCE_RATIO_MAX)
    {
        p_minus_q30 = VARIANCE_RATIO_MAX;
    }

    // Both offsets are within +/-Q16_TICK_LIMIT ticks, so this fits int32_t.
    innovation_q16 = measured_offset_q16 - kf->offset_q16;
    if (!within_gate(kf, innovation_q16, p_minus_q30))
    {
        kf->variance_ratio_q30 = p_minus_q30; // time still passed
        return reject(kf);
    }

    // Update: x += K (z - x); P = (1 - K) P- which, normalised, is K.
    kf->gain_q30 = gain_from_ratio(p_minus_q30);
    kf->offset_q16 += (int32_t)q_round_shift((int64_t)kf->gain_q30 * innovation_q16, 30U);
    kf->variance_ratio_q30 = kf->gain_q30;
    kf->consecutive_rejects = 0;
    kf->stats.accepted++;
    return KALMAN_ACCEPTED;
}


int32_t kalman_phase_q16(const KalmanFilter *kf)
{
    return (kf != NULL) ? kf->offset_q16 : 0;
}


uint32_t kalman_period(const KalmanFilter *kf)
{
    if (kf == NULL)
    {
        return 0;
    }
    return kf->nominal_period + (uint32_t)q16_round_to_ticks(kf->offset_q16);
}


void kalman_hz(const KalmanFilter *kf, uint32_t *whole_hz, uint16_t *fractional_mhz)
{
    int32_t whole_offset;
    uint16_t fraction;
    uint32_t milli;

    if (kf == NULL || whole_hz == NULL || fractional_mhz == NULL)
    {
        return;
    }
    q16_split(kf->offset_q16, &whole_offset, &fraction);
    // fraction * 1000 < 2^26: fits 32 bits.
    milli = ((uint32_t)fraction * UINT32_C(1000) + (uint32_t)Q16_HALF) >> Q16_SHIFT;
    *whole_hz = kf->nominal_period + (uint32_t)whole_offset;
    if (milli >= UINT32_C(1000))
    {
        (*whole_hz)++;
        milli -= UINT32_C(1000);
    }
    *fractional_mhz = (uint16_t)milli;
}
