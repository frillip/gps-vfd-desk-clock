/*
 * Wi-Fi: WiFiManager for setup (a non-blocking access point when nothing is
 * saved or on request), and reconnection every 30 s while the link is down.
 *
 * Dual stack: IPv4 by DHCP, IPv6 by SLAAC (a link-local address, then
 * global ones from the router's advertisements), DNS servers from DHCP or
 * the advertisements' RDNSS. lwIP does the rest: names resolve to IPv6 once
 * there is a global IPv6 address (Network.hostByName), the Telnet server
 * listens on both, and pings are answered on both. Each change of the
 * addresses is printed.
 */
#ifndef WIFI_LINK_H
#define WIFI_LINK_H

#include <Arduino.h>
#include <stdint.h>

void wifi_link_init(Stream *output);
void wifi_link_service(void);

bool wifi_link_is_up(void);
// The global IPv6 address, if there is one (the first, as lwIP prefers it).
bool wifi_link_ipv6(IPAddress *address);
// Where the clock can be reached: 192.0.2.5 and 2001:db8::5, or with a
// port 192.0.2.5:23 and [2001:db8::5]:23. Nothing without a link.
void wifi_link_print_addresses(Stream *output, uint16_t port);
// SML_WIFI_* for STAT-NET.
uint8_t wifi_link_state(void);
const char *wifi_link_hostname(void);

// Forget the credentials, and with clear() restart.
void wifi_link_forget(void);
void wifi_link_clear(Stream *output);
// Open the setup access point now (credentials kept until replaced).
void wifi_link_setup(Stream *output);
void wifi_link_reconnect(void);
void wifi_link_disconnect(void);

// SSID, state, addresses and signal; never the passphrase.
// One line, or everything (wifi -v): every IPv6 address, the DNS servers.
void wifi_link_print(Stream *output, bool verbose);
void wifi_link_scan(Stream *output);

#endif // WIFI_LINK_H
