/*
 * Wi-Fi: WiFiManager for setup (a non-blocking access point when nothing is
 * saved or on request), and reconnection every 30 s while the link is down.
 */
#ifndef WIFI_LINK_H
#define WIFI_LINK_H

#include <Arduino.h>
#include <stdint.h>

void wifi_link_init(Stream *output);
void wifi_link_service(void);

bool wifi_link_is_up(void);
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

// SSID, state, address and signal; never the passphrase.
// One line, or everything (wifi -v).
void wifi_link_print(Stream *output, bool verbose);
void wifi_link_scan(Stream *output);

#endif // WIFI_LINK_H
