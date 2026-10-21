/*
 * Oscillator period estimator: a one-state Kalman filter on the number of
 * timer ticks between consecutive GNSS PPS edges.
 *
 * State: x = measured period - nominal period, Q16.16 ticks (int32_t).
 * Model: x is a random walk (process variance Q per second); each period
 *        measurement carries noise of variance R.
 *
 * The covariance is held normalised by R (p = P / R) in Q2.30. In those
 * units the posterior covariance equals the gain, p = K, so an update is a
 * single division. The gain sequence depends only on Q and R, never on the
 * data; it converges to a constant within a few hundred seconds.
 *
 * Measurements are gated: an innovation larger than KALMAN_GATE_SIGMA
 * standard deviations is rejected without touching the estimate, so a
 * glitch edge or a receiver time step cannot pull the frequency. After
 * KALMAN_REJECT_LIMIT rejections in a row the filter re-acquires from the
 * next measurement, which covers a genuine change of reference.
 */
#ifndef KALMAN_H
#define KALMAN_H

#include <stdbool.h>
#include <stdint.h>

#include "q16.h"

// Variances are in tick^2, Q16.16.
#define KALMAN_VARIANCE_Q16(numerator, denominator) \
    ((uint32_t)((((uint32_t)(numerator) * UINT32_C(65536)) + ((denominator) / 2)) / (denominator)))

// Process variance: frequency wander of about 0.03 tick/s per second
// (0.8 ppb), 1/1024 tick^2.
#define PPS_KF_Q KALMAN_VARIANCE_Q16(1, 1024)
// Measurement variance: one PPS period is the difference of two captures,
// each quantised to a tick and carrying receiver PPS jitter, about 1 tick^2.
// Modelled accuracy is insensitive to this between 0.5 and 4.
#define PPS_KF_R KALMAN_VARIANCE_Q16(1, 1)

#define KALMAN_Q30_ONE      (UINT32_C(1) << 30)
#define KALMAN_GATE_SIGMA   8U // reject innovations beyond 8 sigma
#define KALMAN_REJECT_LIMIT 4U // consecutive rejections before re-acquiring

typedef enum
{
    KALMAN_ACCEPTED,
    KALMAN_ACQUIRED,   // first measurement, or first after re-acquisition
    KALMAN_REJECTED,   // outlier or outside the representable range
    KALMAN_REACQUIRING // rejected, and the next measurement will re-acquire
} KalmanResult;

typedef struct
{
    uint32_t accepted;
    uint32_t rejected;
    uint32_t reacquisitions;
} KalmanStats;

typedef struct
{
    int32_t offset_q16;                // period - nominal, Q16 ticks
    uint32_t variance_ratio_q30;       // P / R, Q2.30
    uint32_t process_ratio_q30;        // Q / R, Q2.30
    uint32_t measurement_variance_q16; // R, tick^2 Q16
    uint32_t gain_q30;                 // last gain, Q2.30 (0..2^30)
    uint32_t nominal_period;           // ticks
    uint8_t consecutive_rejects;
    bool initialized;
    KalmanStats stats;
} KalmanFilter;

// Q and R in tick^2, Q16. R must be non-zero and Q no larger than R.
// Returns false on bad input.
bool kalman_init(KalmanFilter *kf, uint32_t nominal_period, uint32_t process_variance_q16,
                 uint32_t measurement_variance_q16);
// Forgets the estimate; the next measurement is accepted directly.
void kalman_reset(KalmanFilter *kf);
// Starts from an estimate measured elsewhere (period - nominal, Q16), with
// one measurement's worth of confidence, as a first measurement would.
void kalman_seed(KalmanFilter *kf, int32_t offset_q16);

// One validated PPS-to-PPS period, in ticks.
KalmanResult kalman_update(KalmanFilter *kf, uint32_t measured_period);

// Estimated period - nominal, Q16 ticks. The servo works in this domain.
int32_t kalman_phase_q16(const KalmanFilter *kf);
// Estimated period rounded to whole ticks.
uint32_t kalman_period(const KalmanFilter *kf);
// Estimated period, i.e. oscillator ticks per second, as Hz with 3 dp.
void kalman_hz(const KalmanFilter *kf, uint32_t *whole_hz, uint16_t *fractional_mhz);

#endif // KALMAN_H
