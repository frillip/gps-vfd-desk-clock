/*
 * NTP time on the ESP: polls the server (ntp_client), disciplines the clock
 * model (esp_clock), drives the ESP PPS (pps) and builds SML-TIME-NTP for
 * each PPS edge.
 *
 * Polling: at once when Wi-Fi comes up, then after 64, 128, 256 s while the
 * frequency is being learnt, then adaptively from 128 s up to ntp.interval:
 * twice as long after 4 corrections in a row under 500 us, half as long
 * after one over 2 ms. A failed poll is retried after 60 s, doubling up to
 * the interval.
 *
 * Each correction is slewed in by the clock model (esp_clock.h): gently,
 * over about two polls, so the ESP PPS moves slowly enough for the PIC to
 * follow; a large one quickly, with SML_NTP_CORRECTING set in TIME-NTP.
 *
 * Status: synced while an update is no older than 2 x ntp.interval + 5 min.
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

// One line, or everything (esp-ntp-info -v).
void ntp_print(Stream *output, bool verbose);

#endif // NTP_H
