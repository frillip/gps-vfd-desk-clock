/*
 * NTP time on the ESP: polls up to NTP_SERVERS servers (ntp_client), one at
 * a time, chooses the one to follow (ntp_select), disciplines the clock
 * model (esp_clock) by it alone, drives the ESP PPS (pps) and builds
 * SML-TIME-NTP for each PPS edge. The PIC sees one NTP source: this clock.
 *
 * Each server is its own client: it is resolved at each poll (the first
 * record lwIP returns for each family, preferred family first, the other
 * if the first never answers), and keeps its own reach, samples, jitter
 * and root distance. Two servers may resolve to the same address. The
 * first good sample of all sets the clock; after that, after every poll,
 * the servers whose offsets agree within their root distances are found,
 * and the closest of those followed (ntp_select.h). Changing server
 * restarts the frequency window (esp_clock_new_reference).
 *
 * Polling: each server at once when Wi-Fi comes up, then after 64, 128, 256 s while the
 * frequency is being learnt, then adaptively from 128 s up to ntp.interval:
 * twice as long after 4 corrections in a row under 500 us, half as long
 * after one over 2 ms, counting the corrections from the server followed;
 * every server is polled at that interval. A failed poll is retried after
 * 60 s, doubling up to the interval.
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
// Poll now (ntp sync, SML_USER_ESP_NTP_SYNC).
void ntp_resync(void);

bool ntp_is_valid(void);  // the clock has been set
bool ntp_is_synced(void); // ... and recently updated
// UTC now, whole seconds, if the clock is set.
bool ntp_utc_now(uint32_t *utc_seconds);
// UTC at a local time (esp_timer microseconds), if the clock is set.
bool ntp_utc_at(int64_t local_us, int64_t *utc_us);

// After each ESP PPS edge: the TIME-NTP labelling it. True once per edge.
bool ntp_take_report(SML_TIME_NTP_PAYLOAD *payload);

// The usable servers have no majority (ntp_select.h): the clock follows
// one the others do not vouch for.
bool ntp_servers_disagree(void);

// Two lines for the status, or everything (ntp -v).
void ntp_print(Stream *output, bool verbose);
// Every server, as chronyc sources.
void ntp_print_sources(Stream *output, bool verbose);
// The server followed and the clock, as chronyc tracking.
void ntp_print_tracking(Stream *output);

#endif // NTP_H
