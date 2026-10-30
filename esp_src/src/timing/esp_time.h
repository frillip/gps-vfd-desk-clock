/*
 * The ESP's best idea of UTC now, for its own scheduling and logs: the NTP
 * clock if it has been set, else the PIC's latest labelled epoch. Not for
 * timing: TIME-NTP and the ESP PPS use the NTP clock alone.
 */
#ifndef ESP_TIME_H
#define ESP_TIME_H

#include <stdbool.h>
#include <stdint.h>

bool esp_time_utc_now(uint32_t *utc_seconds);

#endif // ESP_TIME_H
