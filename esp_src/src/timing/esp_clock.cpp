#include "esp_clock.h"

#include <string.h>

// After a step, the edge is reported as correcting for this long.
#define STEP_CORRECTING_US 2000000LL

typedef struct
{
    int64_t local_us;
    int64_t offset_us;
    int64_t delay_us;
} RawSample;

typedef struct
{
    int64_t anchor_local_us;
    int64_t anchor_utc_us;
    int64_t slew_start_us;
    int64_t slew_duration_us;
    int64_t step_local_us; // local time of the latest step
    RawSample window[ESP_CLOCK_WINDOW];
    uint8_t window_count;
    uint8_t window_next;
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


// The part of the slew applied by local_us.
static int64_t slewed(int64_t local_us)
{
    int64_t elapsed = local_us - clock_state.slew_start_us;
    int64_t slew = clock_state.status.slew_us;

    if (slew == 0 || elapsed <= 0)
    {
        return 0;
    }
    if (elapsed >= clock_state.slew_duration_us)
    {
        return slew;
    }
    return slew * elapsed / clock_state.slew_duration_us;
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


// ---- Frequency --------------------------------------------------------------

static void remember(const EspClockSample *sample)
{
    RawSample *slot = &clock_state.window[clock_state.window_next];

    slot->local_us = sample->local_us;
    slot->offset_us = sample->offset_us;
    slot->delay_us = sample->delay_us;
    clock_state.window_next = (uint8_t)((clock_state.window_next + 1) % ESP_CLOCK_WINDOW);
    if (clock_state.window_count < ESP_CLOCK_WINDOW)
    {
        clock_state.window_count++;
    }
}


/*
 * The raw offset (UTC - local) drifts at the frequency error, whatever the
 * model does, so a line through the raw offsets measures it directly. Only
 * samples whose round trip is near the window's best are used: queueing
 * adds delay on one side, so the slower exchanges carry the most asymmetry.
 * True with *ppb set if the samples span enough time.
 */
static bool estimate_frequency(int64_t now_us, int64_t *ppb, uint8_t *used)
{
    int64_t best_delay = INT64_MAX;
    int64_t limit;
    int64_t first = INT64_MAX;
    int64_t last = INT64_MIN;
    double sum_t = 0.0, sum_x = 0.0, sum_tt = 0.0, sum_tx = 0.0;
    double t0 = 0.0, x0 = 0.0;
    double n;
    uint8_t index;
    uint8_t count = 0;

    for (index = 0; index < clock_state.window_count; index++)
    {
        const RawSample *raw = &clock_state.window[index];

        if (now_us - raw->local_us <= ESP_CLOCK_WINDOW_MAX_US && raw->delay_us < best_delay)
        {
            best_delay = raw->delay_us;
        }
    }
    if (best_delay == INT64_MAX)
    {
        return false;
    }
    // Within half the best round trip again, or 1 ms, of the best.
    limit = best_delay + (best_delay / 2 > 1000 ? best_delay / 2 : 1000);
    for (index = 0; index < clock_state.window_count; index++)
    {
        const RawSample *raw = &clock_state.window[index];
        double t;
        double x;

        if (now_us - raw->local_us > ESP_CLOCK_WINDOW_MAX_US || raw->delay_us > limit)
        {
            continue;
        }
        if (count == 0)
        {
            t0 = (double)raw->local_us; // centred, for precision
            x0 = (double)raw->offset_us;
        }
        t = (double)raw->local_us - t0;
        x = (double)raw->offset_us - x0;
        sum_t += t;
        sum_x += x;
        sum_tt += t * t;
        sum_tx += t * x;
        first = raw->local_us < first ? raw->local_us : first;
        last = raw->local_us > last ? raw->local_us : last;
        count++;
    }
    if (count < 2 || last - first < ESP_CLOCK_FREQUENCY_MIN_US)
    {
        return false;
    }
    n = (double)count;
    // Slope in us per us, as ppb.
    *ppb = (int64_t)((n * sum_tx - sum_t * sum_x) / (n * sum_tt - sum_t * sum_t) * 1e9);
    *used = count;
    return true;
}


// ---- Updates ----------------------------------------------------------------

static void step(const EspClockSample *sample, int64_t now_us)
{
    // The sample's offset holds at now as well, to within the frequency
    // error over the few seconds since: negligible next to a step.
    clock_state.anchor_local_us = now_us;
    clock_state.anchor_utc_us = now_us + sample->offset_us;
    clock_state.status.slew_us = 0;
    clock_state.status.slew_end_us = now_us;
    clock_state.status.fast = false;
    clock_state.step_local_us = now_us;
    clock_state.status.valid = true;
    clock_state.status.steps++;
}


EspClockAction esp_clock_update(const EspClockSample *sample, int64_t now_us,
                                int64_t time_constant_us)
{
    EspClockStatus *status = &clock_state.status;
    int64_t predicted;
    int64_t error;
    int64_t old_ppb = status->frequency_ppb;
    int64_t new_ppb = old_ppb;
    int64_t utc_now;
    int64_t correction;
    int64_t duration;
    uint8_t used = 0;

    status->updates++;
    status->last = *sample;
    status->last_update_local_us = sample->local_us;
    remember(sample);
    if (now_us < sample->local_us)
    {
        now_us = sample->local_us;
    }
    if (!status->valid)
    {
        status->last_error_us = 0;
        step(sample, now_us);
        return ESP_CLOCK_STEPPED;
    }

    predicted = esp_clock_utc_us(sample->local_us);
    error = sample->local_us + sample->offset_us - predicted;
    status->last_error_us = error;
    if (magnitude(error) > ESP_CLOCK_STEP_US)
    {
        step(sample, now_us);
        return ESP_CLOCK_STEPPED;
    }

    if (estimate_frequency(now_us, &new_ppb, &used))
    {
        if (new_ppb > ESP_CLOCK_FREQUENCY_MAX_PPB)
        {
            new_ppb = ESP_CLOCK_FREQUENCY_MAX_PPB;
        }
        else if (new_ppb < -ESP_CLOCK_FREQUENCY_MAX_PPB)
        {
            new_ppb = -ESP_CLOCK_FREQUENCY_MAX_PPB;
        }
        status->frequency_samples++;
        status->frequency_used = used;
    }

    /*
     * Re-anchor where the model stands now, so the clock does not jump.
     * What is still wrong at now: the error at the sample, less what the
     * old slew has applied since the sample, plus the old frequency's error
     * since the sample (taking the new estimate as the truth).
     */
    utc_now = esp_clock_utc_us(now_us);
    correction = error - (slewed(now_us) - slewed(sample->local_us)) +
                 (now_us - sample->local_us) * (new_ppb - old_ppb) / 1000000000LL;
    clock_state.anchor_local_us = now_us;
    clock_state.anchor_utc_us = utc_now;
    status->frequency_ppb = (int32_t)new_ppb;

    status->slew_us = correction;
    clock_state.slew_start_us = now_us;
    status->fast = magnitude(correction) > ESP_CLOCK_GENTLE_US;
    if (status->fast)
    {
        duration = magnitude(correction) * (1000000 / ESP_CLOCK_FAST_SLEW_PPM);
        status->fast_slews++;
    }
    else
    {
        duration = magnitude(correction) * (1000000 / ESP_CLOCK_GENTLE_PPM);
        if (duration < time_constant_us)
        {
            duration = time_constant_us;
        }
    }
    if (duration < 1000000)
    {
        duration = 1000000;
    }
    clock_state.slew_duration_us = duration;
    status->slew_end_us = now_us + duration;
    return status->fast ? ESP_CLOCK_SLEWED_FAST : ESP_CLOCK_SLEWED;
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
         magnitude(status->slew_us - slewed(local_us)) * 1000 + age * ppm / 1000;
    return ns > (int64_t)UINT32_MAX ? UINT32_MAX : (uint32_t)ns;
}


bool esp_clock_correcting(int64_t local_us)
{
    const EspClockStatus *status = &clock_state.status;

    if (!status->valid)
    {
        return false;
    }
    if (status->steps != 0 && local_us - clock_state.step_local_us < STEP_CORRECTING_US)
    {
        return true;
    }
    return status->fast && local_us < status->slew_end_us;
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
