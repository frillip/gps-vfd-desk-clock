//#ifndef UPDATER_PUSH_H
//#define	UPDATER_PUSH_H

#include <WiFi.h>
#include <ESPmDNS.h>
#include <NetworkUdp.h>
#include <ArduinoOTA.h>

#define UPDATER_PUSH_PORT_DEFAULT 3232
#define UPDATER_PUSH_PASSWORD_DEFAULT "update"

void updater_push_init(uint16_t port, const char* hostname, const char* password);

//#endif	/* UPDATER_PUSH_H */
