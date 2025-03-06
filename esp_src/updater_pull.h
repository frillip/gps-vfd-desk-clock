//#ifndef UPDATER_PULL_H
//#define	UPDATER_PULL_H

#include <Arduino.h>
#include <stdint.h>
#include "serial_console.h"
#include <ESP32OTAPull.h>

#define UPDATER_JSON_HTTPS_DEFAULT  "https://"
#define UPDATER_JSON_SERVER_DEFAULT "rubidium.darksky.io"
#define UPDATER_JSON_PATH_DEFAULT   "/smol_clock/v2/update.json"

void updater_set_server(const char* new_server);
void updater_set_path(const char* new_path);
void updater_regenerate_url(void);
bool updater_check(Stream *output);
void updater_pull(Stream *output);
void updater_force(Stream *output);
void updater_callback_percent(int offset, int totallength);
const char *updater_errtext(int code);

//#endif	/* UPDATER_PULL_H */
