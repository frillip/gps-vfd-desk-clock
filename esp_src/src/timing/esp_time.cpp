#include "esp_time.h"

#include "../pic/pic_link.h"
#include "ntp.h"

bool esp_time_utc_now(uint32_t *utc_seconds)
{
    return ntp_utc_now(utc_seconds) || pic_link_utc_now(utc_seconds);
}
