#ifndef REMOTE_TZINFO_H
#define	REMOTE_TZINFO_H

#include <WiFi.h>
#include <math.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "pic.h"
#include "version.h"

#define REMOTE_TZINFO_HTTPS_DEFAULT  "https://"
#define REMOTE_TZINFO_SERVER_DEFAULT "rubidium.darksky.io"
#define REMOTE_TZINFO_PATH_DEFAULT   "/smol_clock/v2/tzinfo"

#define REMOTE_TZINFO_GNSS_ACCURACY_DEFAULT 1 // 11.1km uncertainty is sufficient for most use cases, even if directly on the coast, will still be inside territorial waters
#define REMOTE_TZINFO_GNSS_ACCURACY_MIN     ( -1 ) // -1 = disabled, do not sent gnss info to the remote txinfo server, even if available
#define REMOTE_TZINFO_GNSS_ACCURACY_MAX     6 // 6 decimal places = 11.1cm uncertainty, approaching the limit of the GNSS reciever
// -1 = disabled
// 0 decimal places = 111km uncertainty
// 1 decimal places = 11.1km uncertainty
// 2 decimal places = 1.11km uncertainty
// 3 decimal places = 111m uncertainty
// 4 decimal places = 11.1m uncertainty 
// 5 decimal places = 1.11m uncertainty
// 6 decimal places = 11.1cm uncertainty

#define REMOTE_TZINFO_ENABLED_DEFAULT  1
#define REMOTE_TZINFO_INTERVAL_DEFAULT 86400  // 1 day, sufficient for most use cases, low bandwidth
#define REMOTE_TZINFO_INTERVAL_MIN     900    // useful for testing
#define REMOTE_TZINFO_INTERVAL_MAX     604800 // 7 days, shortest DST interval is 29 days during Ramadan in Morocco, so this adequately covers that

void remote_tzinfo_set_server(const char* new_server);
void remote_tzinfo_set_path(const char* new_path);
void remote_tzinfo_regenerate_url(void);
float remote_tzinfo_round_acc(float value);
void remote_tzinfo_check(Stream *output);
void print_remote_tzinfo(Stream *output);
void print_tzinfo_source(Stream *output, TZINFO_SOURCE source);

#endif	/* REMOTE_TZINFO_H */
