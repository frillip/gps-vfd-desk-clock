#include "kalman.h"

// Initialize Kalman Filter
void kalman_init(KalmanFilter *kf, int32_t initial_estimate, int32_t process_noise, int32_t measurement_noise)
{
    kf->X_est = initial_estimate * KALMAN_FIXED_POINT_SCALE;  // Convert to fixed-point
    kf->P_est = KALMAN_FIXED_POINT_SCALE;  // Initial error covariance
    kf->Q = process_noise * KALMAN_FIXED_POINT_SCALE;  // Process noise
    kf->R = measurement_noise * KALMAN_FIXED_POINT_SCALE;  // Measurement noise
    kf->K = 0;  // Initial Kalman Gain
}

// Kalman Filter Update Function
int32_t kalman_update(KalmanFilter *kf, int32_t measured_value)
{
    int64_t measured_fixed = ((int64_t)measured_value) * KALMAN_FIXED_POINT_SCALE;
    // Prediction step
    kf->P_est += kf->Q;
    // Compute Kalman Gain
    kf->K = (int32_t)((kf->P_est << 16) / (kf->P_est + kf->R));  // Shift left by 16 for precision
    // Update estimate
    kf->X_est = kf->X_est + ((kf->K * (measured_fixed - kf->X_est)) >> 16);
    // Update covariance
    kf->P_est = ((KALMAN_FIXED_POINT_SCALE - kf->K) * kf->P_est) >> 16;

    return (int32_t)(kf->X_est / KALMAN_FIXED_POINT_SCALE);
}

void kalman_init_f(KalmanFilter_f *kf, float initial_estimate, float process_noise, float measurement_noise) {
    kf->X_est = initial_estimate;
    kf->P_est = 1.0f;            // Initial error covariance
    kf->Q = process_noise;       // Expected system noise
    kf->R = measurement_noise;   // Expected measurement noise
    kf->K = 0.0f;
}

float kalman_update_f(KalmanFilter_f *kf, float measured_value) {
    // Prediction step
    kf->P_est += kf->Q;

    // Compute Kalman Gain
    kf->K = kf->P_est / (kf->P_est + kf->R);

    // Update estimate
    kf->X_est = kf->X_est + kf->K * (measured_value - kf->X_est);

    // Update error covariance
    kf->P_est = (1.0f - kf->K) * kf->P_est;

    return kf->X_est;
}