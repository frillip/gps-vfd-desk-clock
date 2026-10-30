/*
 * Dual-stack connections with fallback, as any dual-stack host makes them:
 * a name's IPv6 and IPv4 addresses are tried in turn, the preferred family
 * first, so a network whose IPv6 (or IPv4) does not route still works.
 *
 * IPv6 is preferred while there is a global IPv6 address (Wi-Fi's SLAAC),
 * unless an IPv6 connection or NTP poll failed within NET_FAMILY_HOLD_MS
 * and IPv4 has not failed since: then IPv4 goes first. A success clears its
 * family's failure. NTP and HTTP(S) share what is learnt, so a broken
 * family costs one timeout an hour, not one per request.
 *
 * Names are resolved here, one family at a time (lwip_getaddrinfo), in the
 * loop task only: see ntp_client.h for why every DNS lookup stays there.
 */
#ifndef NET_CONNECT_H
#define NET_CONNECT_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <stdint.h>

#define NET_ADDRESSES      2                      // one per family
#define NET_FAMILY_HOLD_MS (60UL * 60UL * 1000UL) // 1 h

// host's addresses in the order to try them: a literal as it is (IPv6
// bracketed or not), else its
// IPv6 address (only with a global IPv6 address of our own) and its IPv4
// address, preferred family first. The count, 0 if none resolved.
int net_resolve(const char *host, IPAddress addresses[NET_ADDRESSES]);

// The clock has a global IPv6 address (SLAAC done): until it has, names
// resolve to IPv4 only.
bool net_ipv6_ready(void);

// How a connection or poll to address went: a failure holds its family back.
void net_note_result(const IPAddress &address, bool worked);

// Connects for a GET of url (http:// or https://, a name, an IPv4 literal
// or a bracketed IPv6 one, an optional port) by the first of its addresses
// that answers: secure, verified against ca for the URL's host name, or
// plain. Then http.begin() on the same client and url reuses the
// connection. used: the address connected to. False if none answered.
bool net_http_connect(NetworkClientSecure &secure, NetworkClient &plain, const char *url,
                      const char *ca, uint32_t timeout_ms, IPAddress *used);

// host as it goes in a URL: an IPv6 literal in brackets, anything else as
// it is.
String net_url_host(const char *host);

// A family's state, for wifi -v: IPv6 or IPv4, whether held back, and how
// long ago it last failed (0 if not).
void net_connect_print(Stream *output);

#endif // NET_CONNECT_H
