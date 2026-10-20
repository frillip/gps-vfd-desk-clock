/*
 * NTP client: one poll is a burst of NTP_CLIENT_BURST exchanges with the
 * server, NTP_CLIENT_SPACING_MS apart, run in its own task so the network
 * never blocks the main loop. The exchange with the shortest round trip is
 * the poll's sample (the least queueing, so the least asymmetry).
 *
 * The server's name is resolved by the caller, in the loop task, not here.
 * The Arduino core's Network.hostByName() (used by HTTPClient) calls lwIP's
 * dns_clear_cache() without the TCPIP core lock the first time after the
 * addresses change (so at least once after every start). dns_clear_cache()
 * aborts any query still pending, removing its UDP pcb, and lwIP asserts
 * in udp_remove(). A lookup pending in this task when the loop starts its
 * first HTTPS request crashed the ESP. With every DNS lookup in the loop
 * task, none is ever pending when that happens.
 *
 * T1 and T4 are esp_timer microseconds taken immediately around sendto and
 * recvfrom in a task that outranks the loop, so they carry no loop latency.
 */
#ifndef NTP_CLIENT_H
#define NTP_CLIENT_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_clock.h"
#include "ntp_packet.h"

#define NTP_CLIENT_BURST      4
#define NTP_CLIENT_SPACING_MS 2000
#define NTP_CLIENT_TIMEOUT_MS 1000

typedef struct
{
    bool ok;               // at least one good exchange
    EspClockSample sample; // the best one
    uint8_t replies;       // good exchanges in the burst
    bool dns_failed;       // set by the caller (ntp.cpp)
    NtpReplyResult last_result;
    char kiss[5]; // kiss code, if the server sent one
    char address[16];
} NtpPollResult;

typedef struct
{
    uint32_t polls;
    uint32_t good_polls;
    uint32_t exchanges;
    uint32_t timeouts;
    uint32_t rejected[NTP_REPLY_COUNT];
} NtpClientStats;

void ntp_client_init(void);
// Starts a poll of the server at ipv4 (a.b.c.d = a << 24 | ... | d) unless
// one is running. False if one is running.
bool ntp_client_poll(uint32_t ipv4);
bool ntp_client_busy(void);
// The result of a finished poll, once.
bool ntp_client_take(NtpPollResult *result);
const NtpClientStats *ntp_client_stats(void);

#endif // NTP_CLIENT_H
