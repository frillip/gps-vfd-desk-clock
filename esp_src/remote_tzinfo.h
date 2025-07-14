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
#define REMOTE_TZINFO_GNSS_ACCURACY_DEFAULT 1
#define REMOTE_TZINFO_ENABLED_DEFAULT 1
#define REMOTE_TZINFO_INTERVAL_DEFAULT 900

void remote_tzinfo_set_server(const char* new_server);
void remote_tzinfo_set_path(const char* new_path);
void remote_tzinfo_regenerate_url(void);
float remote_tzinfo_round_acc(float value);
void remote_tzinfo_check(Stream *output);
void print_remote_tzinfo(Stream *output);
void print_tzinfo_source(Stream *output, TZINFO_SOURCE source);

#endif	/* REMOTE_TZINFO_H */
