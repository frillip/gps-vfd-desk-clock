/*
 * NTPv4 client packets (RFC 5905), portable for the host tests.
 *
 * The request carries a random transmit timestamp rather than our time: the
 * server echoes it as the origin timestamp, which ties the reply to the
 * request. Our own send and receive times (T1, T4) are local microseconds
 * taken around the exchange; the server's (T2, T3) are UTC.
 */
#ifndef NTP_PACKET_H
#define NTP_PACKET_H

#include <stdbool.h>
#include <stdint.h>

#define NTP_PACKET_BYTES 48U
#define NTP_PORT         123U

typedef enum
{
    NTP_REPLY_OK = 0,
    NTP_REPLY_SHORT,          // under 48 bytes
    NTP_REPLY_NOT_SERVER,     // mode is not 4 (server)
    NTP_REPLY_WRONG_ORIGIN,   // not an answer to our request
    NTP_REPLY_KISS,           // stratum 0: kiss-o'-death (rate limit, deny)
    NTP_REPLY_UNSYNCHRONISED, // leap indicator 3, or stratum over 15
    NTP_REPLY_BAD_TIME,       // zero or reversed server timestamps
    NTP_REPLY_COUNT
} NtpReplyResult;

typedef struct
{
    uint8_t leap; // leap indicator
    uint8_t stratum;
    uint32_t root_delay_us;
    uint32_t root_dispersion_us;
    int64_t receive_utc_us;  // T2
    int64_t transmit_utc_us; // T3
    char kiss[5];            // the kiss code when stratum is 0
} NtpReply;

void ntp_packet_request(uint8_t *packet, uint64_t cookie);
NtpReplyResult ntp_packet_parse(const uint8_t *packet, uint32_t length, uint64_t cookie,
                                NtpReply *reply);
const char *ntp_reply_name(NtpReplyResult result);

// NTP 64-bit timestamp to Unix microseconds. Era 0 for seconds from
// 0x80000000 (1968-2036), era 1 below that (2036-2104).
int64_t ntp_timestamp_to_unix_us(uint64_t timestamp);

#endif // NTP_PACKET_H
