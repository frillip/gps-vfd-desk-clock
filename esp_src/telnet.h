#ifndef TELNET_H
#define	TELNET_H

#include <Arduino.h>
#include <stdint.h>
#include <time.h>
#include <ezTime.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include "serial_console.h"
#include <HardwareSerial.h>
#include <ESPTelnetStream.h>

#define TELNET_ENABLED_DEFAULT 1  // For now, enable by default FIX FOR PROD
#define TELNET_PORT_DEFAULT 23    // Port 23 is standard
#define TELNET_BUFFER_LENGTH 100

void telnet_init(void);
void telnet_stop(void);
void telnet_enable(void);
void telnet_disable(void);
void telnet_set_port(uint16_t telnet_port_new);
void onTelnetConnect(String ip);
void onTelnetDisconnect(String ip);
void onTelnetReconnect(String ip);
void onTelnetConnectionAttempt(String ip);
void onTelnetInput(String str);

bool telnet_char_available(void);
void telnet_console_task(void);

#endif	/* TELNET_H */
