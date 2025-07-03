#include "allan_dev.h"

volatile uint32_t last_pps_time = 0;
volatile uint32_t timer_overflows = 0;
extern int32_t pps_buffer[MAX_PPS_SAMPLES];
extern uint16_t pps_index;
extern uint32_t pps_seq_count;

// Function to Compute Allan Deviation
float compute_allan_deviation(uint16_t tau) {
    if (pps_seq_count < 2 * tau) return -1;

    int64_t sum = 0;
    for (uint16_t i = 0; i < pps_index - 2 * tau; i++) {
        int32_t diff = pps_buffer[i + tau] - pps_buffer[i];
        int64_t y_k = ((int64_t)diff << 16) / pps_buffer[i];  // Q16.16
        sum += (y_k * y_k) >> 16;  // accumulate in Q16.16
    }

    int64_t avg = sum / (2 * (pps_index - 2 * tau));

    // Convert fixed-point average to float before sqrt
    float avg_float = (float)avg / ALLAN_FIXED_POINT_SCALE;
    
    return sqrt(avg_float);
}

// Function to Analyze Clock Stability
void analyze_clock_stability(void) {
    float adev1s = compute_allan_deviation(1);
    float adev10s = compute_allan_deviation(10);
    float adev100s = compute_allan_deviation(100);

    printf("ADEV (1s): %f\n", (double)adev1s);
    printf("ADEV (10s): %f\n", (double)adev10s);
    printf("ADEV (100s): %f\n", (double)adev100s);
}
