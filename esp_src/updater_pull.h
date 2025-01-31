//#ifndef UPDATER_PULL_H
//#define	UPDATER_PULL_H

#include <Arduino.h>
#include <stdint.h>
#include "serial_console.h"
#include <ESP32OTAPull.h>

#define UPDATER_JSON_URL_DEFAULT  "https://rubidium.darksky.io/smol_clock/v2/update.json"

bool updater_check(Stream *output);
void updater_pull(Stream *output);
void updater_pull_reboot(Stream *output);
void updater_callback_percent(int offset, int totallength);
const char *updater_errtext(int code);

//#endif	/* UPDATER_PULL_H */
