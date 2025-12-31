#ifndef USER_PREFS_H
#define	USER_PREFS_H

#include <Arduino.h>
#include <Preferences.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <nvs_flash.h>
#include "remote_tzinfo.h"
#include "updater_pull.h"
#include "ntp_settings.h"

#define PREFERENCES_NAMESPACE_STRING	"CLOCK_PREFS"
#define PREFERENCES_NAMESPACE_KEY_STRUCT "PREF_STRUCT"
#define USER_PREFS_HEADER_VALUE		0xC0FFEEEE
#define RW_MODE false
#define RO_MODE true

#define PREFERENCES_MAX_STRING_SIZE 100
#define PREFERENCES_STORED_STRING_SIZE ( PREFERENCES_MAX_STRING_SIZE + 4 )

typedef union
{
  struct __attribute__ ((packed)) _user_prefs_data_struct
  {
    uint32_t header;
    struct __attribute__((packed))
    {
        struct __attribute__((packed))
        {
            bool enabled : 1; // extern bool telnet_enabled; // esp-telnet-enable // esp-telnet-disable // TELNET_ENABLED_DEFAULT 1
            uint16_t padding : 15;
        } flags;
        uint16_t port; // extern uint16_t telnet_port; // esp-telnet-port // TELNET_PORT_DEFAULT 23
    } telnet;
    struct __attribute__((packed))
    {
		  char server[PREFERENCES_STORED_STRING_SIZE]; // extern String ntp_server; // #define CLOCK_NTP_SERVER_DEFAULT "rubidium.darksky.io" // esp-ntp-set-interval
      uint32_t interval; // uint32_t ntp_interval; // #define CLOCK_NTP_INTERVAL_DEFAULT 1800 // esp-ntp-set-server
    } ntp;
    struct __attribute__((packed))
    {
        struct __attribute__((packed))
        {
            bool enabled : 1;  // extern bool remote_tzinfo_enabled; // #define REMOTE_TZINFO_ENABLED_DEFAULT 1 // esp-tzinfo-enable
            uint16_t padding : 15;
        } flags;
        char server[PREFERENCES_STORED_STRING_SIZE]; // extern String remote_tzinfo_json_server; // #define REMOTE_TZINFO_SERVER_DEFAULT "rubidium.darksky.io" // esp-tzinfo-set-server // remote_tzinfo_regenerate_url()
        char path[PREFERENCES_STORED_STRING_SIZE]; // extern String remote_tzinfo_json_path; // #define REMOTE_TZINFO_PATH_DEFAULT   "/smol_clock/v2/tzinfo" // esp-tzinfo-set-path // remote_tzinfo_regenerate_url()
        uint32_t interval; // extern uint32_t remote_tzinfo_interval; // #define REMOTE_TZINFO_INTERVAL_DEFAULT 900 // esp-tzinfo-set-interval
        int32_t gnss_accuracy; // extern int32_t remote_tzinfo_gnss_accuracy; // #define REMOTE_TZINFO_GNSS_ACCURACY_DEFAULT 1 // esp-tzinfo-set-acc
    } rtzinfo;
    struct __attribute__((packed))
    {
        struct __attribute__((packed))
        {
            uint16_t auto_enabled : 1; // extern bool updater_auto_enabled; // #define UPDATE_AUTO_ENABLED_DEFAULT 0 // esp-update-auto-enable
            uint16_t auto_time : 8; // extern uint8_t updater_auto_check_local_time; // #define UPDATE_AUTO_CHECK_LOCAL_TIME_DEFAULT 4 // esp-update-auto-time
            uint16_t padding : 7;
        } flags;
        char server[PREFERENCES_STORED_STRING_SIZE]; // extern String updater_json_server; // esp-update-set-server // updater_regenerate_url()
        char path[PREFERENCES_STORED_STRING_SIZE]; // extern String updater_json_path; // esp-update-set-path // updater_regenerate_url()
        char config[PREFERENCES_STORED_STRING_SIZE]; // extern String updater_config_string; // #define UPDATER_CONFIG_STRING_DEFAULT "" // esp-update-set-config
    } updater;
    uint32_t checksum; // unused
  } fields;
  uint8_t raw[sizeof(struct _user_prefs_data_struct)];
} USER_PREFS_DATA_STRUCT;

static_assert(sizeof(USER_PREFS_DATA_STRUCT) % 2 == 0, "USER_PREFS_DATA_STRUCT must be 16-bit aligned");

void user_prefs_init(void);
bool user_prefs_validate(void);
void user_prefs_apply(void);
void user_prefs_sync(void);
void user_prefs_reset(void);
void user_prefs_load(void);
void user_prefs_save(void);
void user_prefs_print(Stream *output);
void user_prefs_erase(void);

#endif	/* USER_PREFS_H */
