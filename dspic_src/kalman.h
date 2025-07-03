/* 
 * File:   kalman.h
 * Author: Frillip
 *
 * Created on October 23, 2021, 7:36 PM
 */

#ifndef KALMAN_H
#define	KALMAN_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <math.h>
#include <xc.h>
    
typedef struct {
    int64_t X_est;  // Estimated PPS interval (fixed-point)
    int64_t P_est;  // Error covariance (fixed-point)
    int32_t Q;      // Process noise (scaled by 65536)
    int32_t R;      // Measurement noise (scaled by 65536)
    int32_t K;      // Kalman Gain (scaled by 65536)
} KalmanFilter;

typedef struct {
    float X_est;  // Current estimate
    float P_est;  // Error covariance
    float Q;      // Process noise
    float R;      // Measurement noise
    float K;      // Kalman gain
} KalmanFilter_f;

#define KALMAN_FIXED_POINT_SCALE (1UL << 16)  // Scale factor = 65536

void kalman_init(KalmanFilter *kf, int32_t initial_estimate, int32_t process_noise, int32_t measurement_noise);
int32_t kalman_update(KalmanFilter *kf, int32_t measured_value);

void kalman_init_f(KalmanFilter_f *kf, float initial_estimate, float process_noise, float measurement_noise);
float kalman_update_f(KalmanFilter_f *kf, float measured_value);

#ifdef	__cplusplus
}
#endif

#endif	/* KALMAN_H */

