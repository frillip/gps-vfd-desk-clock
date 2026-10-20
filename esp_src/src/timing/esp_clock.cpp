#include "esp_clock.h"

#include <string.h>

typedef struct
{
    int64_t anchor_local_us;
    int64_t anchor_utc_us;
    int64_t slew_us; // error being slewed in from slew_start
    int64_t slew_start_us;
    int64_t slew_duration_us;
    bool have_reference; // a raw sample to measure frequency from
    int64_t reference_local_us;
    int64_t reference_offset_us;
    EspClockStatus status;
} EspClock;

static EspClock clock_state;

static int64_t magnitude(int64_t value)
{
    return value < 0 ? -value : value;
}


void esp_clock_init(void)
{
    memset(&clock_state, 0, sizeof(clock_state));
}


bool esp_clock_is_valid(void)
{
    return clock_state.status.valid;
}


static int64_t slewed(int64_t local_us)
{
    int64_t elapsed = local_us - clock_state.slew_start_us;

    if (clock_state.slew_us == 0 || elapsed <= 0)
    {
        return 0;
    }
    if (elapsed >= clock_state.slew_duration_us)
    {
        return clock_state.slew_us;
    }
    return clock_state.slew_us * elapsed / clock_state.slew_duration_us;
}


int64_t esp_clock_utc_us(int64_t local_us)
{
    int64_t elapsed = local_us - clock_state.anchor_local_us;

    return clock_state.anchor_utc_us + elapsed +
           elapsed * clock_state.status.frequency_ppb / 1000000000LL + slewed(local_us);
}


int64_t esp_clock_local_us(int64_t utc_us)
{
    int64_t local = clock_state.anchor_local_us + (utc_us - clock_state.anchor_utc_us);
    int pass;

    // Newton steps: the model's slope is 1 to within a few hundred ppm.
    for (pass = 0; pass < 3; pass++)
    {
        local -= esp_clock_utc_us(local) - utc_us;
    }
    return local;
}


static void step(const EspClockSample *sample)
{
    clock_state.anchor_local_us = sample->local_us;
    clock_state.anchor_utc_us = sample->local_us + sample->offset_us;
    clock_state.slew_us = 0;
    clock_state.have_reference = true;
    clock_state.reference_local_us = sample->local_us;
    clock_state.reference_offset_us = sample->offset_us;
    clock_state.status.valid = true;
    clock_state.status.steps++;
}


static void learn_frequency(const EspClockSample *sample)
{
    int64_t interval = sample->local_us - clock_state.reference_local_us;
    int64_t observed;
    EspClockStatus *status = &clock_state.status;

    if (!clock_state.have_reference || interval < ESP_CLOCK_FREQUENCY_MIN_US)
    {
        return;
    }
    // Drift of the raw offset per local microsecond, in ppb.
    observed = (sample->offset_us - clock_state.reference_offset_us) * 1000000000LL / interval;
    if (status->frequency_samples == 0)
    {
        status->frequency_ppb = (int32_t)observed;
    }
    else
    {
        status->frequency_ppb += (int32_t)((observed - status->frequency_ppb) / 4);
    }
    if (status->frequency_ppb > ESP_CLOCK_FREQUENCY_MAX_PPB)
    {
        status->frequency_ppb = ESP_CLOCK_FREQUENCY_MAX_PPB;
    }
    else if (status->frequency_ppb < -ESP_CLOCK_FREQUENCY_MAX_PPB)
    {
        status->frequency_ppb = -ESP_CLOCK_FREQUENCY_MAX_PPB;
    }
    status->frequency_samples++;
    clock_state.reference_local_us = sample->local_us;
    clock_state.reference_offset_us = sample->offset_us;
}


EspClockAction esp_clock_update(const EspClockSample *sample)
{
    EspClockStatus *status = &clock_state.status;

    status->updates++;
    status->last = *sample;
    status->last_update_local_us = sample->local_us;
    if (!status->valid)
    {
        status->last_error_us = 0;
        step(sample);
        return ESP_CLOCK_STEPPED;
    }

    int64_t predicted = esp_clock_utc_us(sample->local_us);
    int64_t error = sample->local_us + sample->offset_us - predicted;

    status->last_error_us = error;
    if (magnitude(error) > ESP_CLOCK_STEP_US)
    {
        step(sample);
        return ESP_CLOCK_STEPPED;
    }
    // Frequency first, so the re-anchored model runs at the new rate.
    learn_frequency(sample);
    clock_state.anchor_local_us = sample->local_us;
    clock_state.anchor_utc_us = predicted;
    clock_state.slew_us = error;
    clock_state.slew_start_us = sample->local_us;
    clock_state.slew_duration_us = magnitude(error) * (1000000 / ESP_CLOCK_SLEW_PPM);
    if (clock_state.slew_duration_us < 1000000)
    {
        clock_state.slew_duration_us = 1000000;
    }
    return ESP_CLOCK_SLEWED;
}


uint32_t esp_clock_uncertainty_ns(int64_t local_us)
{
    const EspClockStatus *status = &clock_state.status;
    int64_t age = local_us - status->last_update_local_us;
    int64_t ppm = status->frequency_samples >= 2 ? 2 : status->frequency_samples == 1 ? 5 : 50;
    int64_t ns;

    if (!status->valid)
    {
        return UINT32_MAX;
    }
    if (age < 0)
    {
        age = 0;
    }
    ns = magnitude(status->last.delay_us) * 500 + (int64_t)status->last.root_delay_us * 500 +
         (int64_t)status->last.root_dispersion_us * 1000 +
         magnitude(clock_state.slew_us - slewed(local_us)) * 1000 + age * ppm / 1000;
    return ns > (int64_t)UINT32_MAX ? UINT32_MAX : (uint32_t)ns;
}


int32_t esp_clock_period_adjust_q16(void)
{
    // A UTC second lasts 1e6 / (1 + f) local microseconds, about 1e6 x (1 - f).
    return (int32_t)(-(int64_t)clock_state.status.frequency_ppb * 65536 / 1000);
}


const EspClockStatus *esp_clock_status(void)
{
    return &clock_state.status;
}
