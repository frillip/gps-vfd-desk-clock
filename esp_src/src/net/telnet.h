/*
 * The telnet console: the same commands as the serial console, and a copy
 * of the console output while a client is connected. No authentication yet.
 */
#ifndef TELNET_H
#define TELNET_H

#include <Arduino.h>
#include <stdint.h>

void telnet_init(Stream *output);
void telnet_service(void);

void telnet_start(Stream *output);
void telnet_stop(Stream *output);
void telnet_enable(Stream *output);
void telnet_disable(Stream *output);
void telnet_set_port(Stream *output, uint16_t port);

bool telnet_is_running(void);
// The connected client's stream, or NULL.
Stream *telnet_client(void);

#endif // TELNET_H
