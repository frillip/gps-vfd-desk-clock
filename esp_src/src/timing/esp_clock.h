/*
 * The ESP's UTC: a model of UTC against the local microsecond timer
 * (esp_timer, the APB crystal), disciplined by NTP samples. Portable, no
 * Arduino dependency, so the host tests can drive it.
 *
 *   utc(t) = anchor_utc + (t - anchor) x (1 + frequency) + slew(t)
 *
 * Each sample is the best exchange of an NTP poll: the offset UTC - local at
 * the local midpoint of the exchange, and its round trip.
 *   - The first sample, or one more than ESP_CLOCK_STEP_US from the model,
 *     steps the clock.
 *   - Otherwise the model is re-anchored where it stands (no jump) and the
 *     error is slewed in at ESP_CLOCK_SLEW_PPM, so the PPS edge never jumps.
 *   - Frequency: from the raw offsets of samples at least
 *     ESP_CLOCK_FREQUENCY_MIN_US apart, the first taken as is, the rest
 *     averaged in with weight 1/4.
 *
 * The uncertainty is a +/- bound in the sense of SML: half the round trip,
 * the server's root delay / 2 and root dispersion, the part of the error not
 * yet slewed in, and a drift allowance since the sample (50 ppm until the
 * frequency is known, 5 ppm after one estimate, 2 ppm after more).
 */
#ifndef ESP_CLOCK_H
#define ESP_CLOCK_H

#include <stdbool.h>
#include <stdint.h>

#define ESP_CLOCK_STEP_US           128000LL
#define ESP_CLOCK_SLEW_PPM          100
#define ESP_CLOCK_FREQUENCY_MIN_US  60000000LL
#define ESP_CLOCK_FREQUENCY_MAX_PPB 500000

typedef struct
{
    int64_t local_us;  // local time of the exchange midpoint
    int64_t offset_us; // UTC - local, at local_us
    int64_t delay_us;  // round trip minus the server's processing
    uint32_t root_delay_us;
    uint32_t root_dispersion_us;
    uint8_t stratum;
} EspClockSample;

typedef enum
{
    ESP_CLOCK_STEPPED,
    ESP_CLOCK_SLEWED,
} EspClockAction;

typedef struct
{
    bool valid;
    uint32_t updates;
    uint32_t steps;
    uint32_t frequency_samples;
    int32_t frequency_ppb;        // UTC rate against the local timer: + = local slow
    int64_t last_update_local_us; // local time of the latest sample
    int64_t last_error_us;        // that sample against the model before it
    EspClockSample last;          // the latest sample
} EspClockStatus;

void esp_clock_init(void);
EspClockAction esp_clock_update(const EspClockSample *sample);

bool esp_clock_is_valid(void);
// Valid only while esp_clock_is_valid().
int64_t esp_clock_utc_us(int64_t local_us);
// The local time at which UTC reaches utc_us.
int64_t esp_clock_local_us(int64_t utc_us);
uint32_t esp_clock_uncertainty_ns(int64_t local_us);
// Local microseconds to add to 1 000 000 for one UTC second, Q16 (the PPS
// uses it to run on while the loop is busy).
int32_t esp_clock_period_adjust_q16(void);

const EspClockStatus *esp_clock_status(void);

#endif // ESP_CLOCK_H
