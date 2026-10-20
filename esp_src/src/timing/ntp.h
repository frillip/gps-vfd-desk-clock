/*
 * NTP time on the ESP: polls the server (ntp_client), disciplines the clock
 * model (esp_clock), drives the ESP PPS (pps) and builds SML-TIME-NTP for
 * each PPS edge.
 *
 * Polling: at once when Wi-Fi comes up, then every 64, 128, 256 s while the
 * frequency is being learnt, then every ntp.interval seconds. A failed poll
 * is retried after 60 s, doubling up to the interval.
 *
 * Status: synced while an update is no older than 2 intervals + 5 min.
 * After that the clock coasts on its frequency estimate (source ESP,
 * HOLDOVER) and the uncertainty keeps growing; after 24 h it is reported as
 * FREE_RUNNING. TIME-NTP is only built once the clock has been set.
 */
#ifndef NTP_H
#define NTP_H

#include <Arduino.h>
#include <stdint.h>

extern "C"
{
#include "../pic/sml/sml.h"
}

void ntp_init(void);
void ntp_service(bool network_up);
// Poll now (esp-ntp-resync, SML_USER_ESP_NTP_RESYNC).
void ntp_resync(void);

bool ntp_is_valid(void);  // the clock has been set
bool ntp_is_synced(void); // ... and recently updated
// UTC now, whole seconds, if the clock is set.
bool ntp_utc_now(uint32_t *utc_seconds);
// UTC at a local time (esp_timer microseconds), if the clock is set.
bool ntp_utc_at(int64_t local_us, int64_t *utc_us);

// After each ESP PPS edge: the TIME-NTP labelling it. True once per edge.
bool ntp_take_report(SML_TIME_NTP_PAYLOAD *payload);

void ntp_print(Stream *output);

#endif // NTP_H
