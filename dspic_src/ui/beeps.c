#include "beeps.h"

#include "../drivers/buzzer.h"

#define SECONDS_PER_DAY  86400UL
#define SECONDS_PER_HOUR 3600UL

static bool enabled;

void beeps_init(void)
{
    enabled = false;
}


void beeps_set_enabled(bool on)
{
    enabled = on;
}


bool beeps_enabled(void)
{
    return enabled;
}


uint8_t beeps_chime_count(uint32_t local_seconds)
{
    uint32_t of_day = local_seconds % SECONDS_PER_DAY;
    uint8_t hour;

    if (of_day % BEEPS_CHIME_INTERVAL_S != 0U)
    {
        return 0;
    }
    if (of_day % SECONDS_PER_HOUR != 0U)
    {
        return 1;
    }
    hour = (uint8_t)(of_day / SECONDS_PER_HOUR % 12U);
    return hour == 0U ? 12U : hour;
}


void beeps_epoch(bool known, bool leap_second, uint32_t local_seconds)
{
    uint8_t count;

    if (!enabled || !known || leap_second)
    {
        return;
    }
    count = beeps_chime_count(local_seconds);
    if (count != 0U)
    {
        (void)buzzer_play(&buzzer_chime, count, BUZZER_PRIORITY_CHIME);
    }
}


void beeps_button(void)
{
    if (enabled)
    {
        (void)buzzer_play(&buzzer_button, 1U, BUZZER_PRIORITY_BUTTON);
    }
}
