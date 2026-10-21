/*
 * Time zone from the tzinfo server (protocol v1), sent to the PIC as
 * SML-TZ-STATE.
 *
 * Lookups (only with tzinfo enabled and Wi-Fi up):
 *   - Location: the PIC's GNSS position, rounded to tzinfo.gnss_accuracy
 *     decimal places, while it has a fix; otherwise none, and the server uses
 *     the clock's IP address (GeoIP). With gnss_accuracy -1 no position is
 *     ever sent. The first lookup after start waits up to 60 s for a fix,
 *     unless the PIC reports no receiver, or no schedule is cached (then
 *     GeoIP at once, and the GNSS position refines it once there is a fix).
 *   - When: at start, as soon as tzinfo is enabled, every tzinfo.interval
 *     seconds (sooner if the server's valid_until says so), when the rounded
 *     position changes, and on request. A failure is retried after 5 min,
 *     doubling to 1 h; while no schedule is held at all, after 30 s,
 *     doubling to 5 min.
 *
 * The latest schedule is kept in NVS and sent at start as source CACHE until
 * a lookup replaces it.
 *
 * TZ-STATE is sent to a PIC in SML mode: when the schedule changes, when
 * STAT-UI shows the PIC holds none (it reset), and every 10 min. Nothing is
 * sent while tzinfo is disabled: the PIC then uses its manual offsets.
 */
#ifndef TZINFO_H
#define TZINFO_H

#include <Arduino.h>
#include <stdint.h>

#include "tz_schedule.h"

void tzinfo_init(Stream *output);
void tzinfo_service(bool network_up);

// Look up now and apply the result (esp-tzinfo-update, ESP_TZINFO_UPDATE).
bool tzinfo_update(Stream *output);
// Look up now and print the result without using it (esp-tzinfo-check).
bool tzinfo_check(Stream *output);
void tzinfo_set_enabled(bool enabled);
void tzinfo_set_server(const char *server);
void tzinfo_set_path(const char *path);

// The schedule held, valid or not.
const TzSchedule *tzinfo_schedule(void);
bool tzinfo_last_ok(void);
uint32_t tzinfo_updated_utc(void); // last successful lookup, 0 if none
uint32_t tzinfo_refresh_utc(void); // next lookup due, 0 if unknown

// Local civil time now as the PIC shows it (STAT-UI offsets), else by the
// schedule held, else UTC. False only if UTC itself is unknown.
bool tzinfo_local_now(uint32_t *local_seconds);

void tzinfo_print(Stream *output);

#endif // TZINFO_H
