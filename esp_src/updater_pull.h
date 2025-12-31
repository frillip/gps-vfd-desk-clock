//#ifndef UPDATER_PULL_H
//#define	UPDATER_PULL_H

#include <Arduino.h>
#include <stdint.h>
#include <time.h>
#include "serial_console.h"
#include <ESP32OTAPull.h>
#include "telnet.h"

#define UPDATER_JSON_HTTPS_DEFAULT            "https://"
#define UPDATER_JSON_SERVER_DEFAULT           "rubidium.darksky.io"
#define UPDATER_JSON_PATH_DEFAULT             "/smol_clock/v2/update.json"
#define UPDATER_CONFIG_STRING_DEFAULT         ""

#define UPDATE_AUTO_ENABLED_DEFAULT           0
#define UPDATE_AUTO_CHECK_INTERVAL_DEFAULT    1800 // Not used
#define UPDATE_AUTO_CHECK_LOCAL_HOUR_DEFAULT  4

void updater_set_server(const char* new_server);
void updater_set_path(const char* new_path);
void updater_set_config(const char* new_config);
void updater_regenerate_url(void);
void updater_auto_check(void);
bool updater_auto(void);
bool updater_check(Stream *output);
void updater_pull(Stream *output);
void updater_force(Stream *output);
void updater_callback_percent(int offset, int totallength);
const char *updater_errtext(int code);

//#endif	/* UPDATER_PULL_H */
