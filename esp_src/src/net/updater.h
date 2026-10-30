/*
 * OTA updates pulled from https://<updater.server><updater.path>: a JSON
 * manifest in ESP32-OTA-Pull's format, matched against ESP_VERSION and
 * updater.config, then the image it names. Done here rather than by the
 * library so that both requests connect with IPv6/IPv4 fallback
 * (net_connect.h); the library makes its own connections.
 *
 * Automatic updates: once a day, in the local hour updater.auto_hour, the
 * ESP checks and, if there is an update, installs it and restarts. Not in
 * the hour it started in, so an update does not run twice.
 */
#ifndef UPDATER_H
#define UPDATER_H

#include <Arduino.h>
#include <stdint.h>

void updater_set_server(const char *server);
void updater_set_path(const char *path);
void updater_set_config(const char *config);

// Once a minute; local_seconds is local civil time, valid if have_local.
void updater_service(bool have_local, uint32_t local_seconds);

// Reports to output. check: is there an update? pull: install it, restart
// later. force: install whatever is offered, any version, and restart.
bool updater_check(Stream *output);
void updater_pull(Stream *output);
void updater_force(Stream *output);

bool updater_is_available(void);
bool updater_is_running(void);

#endif // UPDATER_H
