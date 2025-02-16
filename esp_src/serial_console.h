#ifndef SERIAL_CONSOLE_H
#define	SERIAL_CONSOLE_H

#include <Arduino.h>
#include <stdint.h>
#include <time.h>
#include <ezTime.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include "enums.h"
#include "serial_proto.h"
#include "esp_pps.h"
#include "gnss_pps_esp.h"
#include "pic.h"
#include <HardwareSerial.h>
#include "updater.h"

#define USER_CMD_STAGE_1_ESP_STRING "esp-"
#define USER_CMD_STAGE_1_PIC_STRING "pic-"
#define USER_CMD_STAGE_1_RST_STRING "rst-"
#define USER_CMD_STAGE_1_HELP_STRING "help"
#define USER_CMD_STAGE_1_LENGTH 4

#define USER_CMD_ESP_WIFI_PREFIX_STRING "wifi-"
#define USER_CMD_ESP_WIFI_PREFIX_LENGTH 5

#define USER_CMD_PIC_INFO_STRING "pic-info"
#define USER_CMD_PIC_RESYNC_STRING "pic-resync"
#define USER_CMD_PIC_RESET_STRING "pic-reset"
#define USER_CMD_PIC_SET_RTC_STRING "pic-set-rtc"
#define USER_CMD_PIC_SET_TZ_OFFSET_STRING "pic-set-tz-offset"
#define USER_CMD_PIC_SET_DST_OFFSET_STRING "pic-set-dst-offset"
#define USER_CMD_PIC_SET_DST_AUTO_STRING "pic-set-dst-auto"
#define USER_CMD_PIC_SET_DST_ACTIVE_STRING "pic-set-dst-active"
#define USER_CMD_PIC_SET_ALARM_ENABLED_STRING "pic-set-alarm-enabled"
#define USER_CMD_PIC_SET_ALARM_STRING "pic-set-alarm"
#define USER_CMD_PIC_SET_DELTA_STRING "pic-set-delta"
#define USER_CMD_PIC_SET_BEEPS_STRING "pic-set-beeps"
#define USER_CMD_PIC_SET_DISPLAY_STRING "pic-set-display"
#define USER_CMD_PIC_SET_BRIGHTNESS_AUTO_STRING "pic-set-brightness-auto"
#define USER_CMD_PIC_SET_BRIGHTNESS_STRING "pic-set-brightness"
#define USER_CMD_PIC_SHOW_CONFIG_STRING "pic-show-config"
#define USER_CMD_PIC_SHOW_EEPROM_STRING "pic-show-eeprom"
#define USER_CMD_PIC_CLEAR_ALL_STRING "pic-clear-all"
#define USER_CMD_PIC_SAVE_STRING "pic-save"
#define USER_CMD_PIC_BOOTLOADER_ENTER_STRING "pic-bootloader-enter"
#define USER_CMD_PIC_BOOTLOADER_EXIT_STRING "pic-bootloader-exit"

#define USER_CMD_ESP_RESET_STRING "esp-reset"

#define USER_CMD_ESP_NTP_SET_INTERVAL_STRING "esp-ntp-set-interval"
#define USER_CMD_ESP_NTP_SET_SERVER_STRING "esp-ntp-set-server"
#define USER_CMD_ESP_NTP_RESYNC_STRING "esp-ntp-resync"

#define USER_CMD_ESP_WIFI_INFO_STRING "esp-wifi-info"
#define USER_CMD_ESP_WIFI_SCAN_STRING "esp-wifi-scan"
#define USER_CMD_ESP_WIFI_CONNECT_STRING "esp-wifi-connect"
#define USER_CMD_ESP_WIFI_DISCONNECT_STRING "esp-wifi-disconnect"
#define USER_CMD_ESP_WIFI_SSID_STRING "esp-wifi-ssid"
#define USER_CMD_ESP_WIFI_PASS_STRING "esp-wifi-pass"
#define USER_CMD_ESP_WIFI_DHCP_STRING "esp-wifi-dhcp"
#define USER_CMD_ESP_WIFI_IP_STRING "esp-wifi-ip"
#define USER_CMD_ESP_WIFI_MASK_STRING "esp-wifi-mask"
#define USER_CMD_ESP_WIFI_GATEWAY_STRING "esp-wifi-gateway"
#define USER_CMD_ESP_WIFI_CLEAR_STRING "esp-wifi-clear"
#define USER_CMD_ESP_WIFI_SETUP_STRING "esp-wifi-setup"

#define USER_CMD_ESP_UPDATE_CHECK_STRING "esp-update-check"
#define USER_CMD_ESP_UPDATE_PULL_STRING "esp-update-pull"
#define USER_CMD_ESP_UPDATE_FORCE_STRING "esp-update-force"
#define USER_CMD_ESP_UPDATE_SET_SERVER_STRING "esp-update-set-server"
#define USER_CMD_ESP_UPDATE_SET_PATH_STRING "esp-update-set-path"
//#define USER_CMD_ESP_UPDATE_PUSH_ENABLE_STRING "esp-update-push-enable"
//#define USER_CMD_ESP_UPDATE_PUSH_DISABLE_STRING "esp-update-push-disable"

#define USER_CMD_ESP_CLEAR_ALL_STRING "esp-clear-all"
#define USER_CMD_ESP_SAVE_STRING "esp-save"
#define USER_CMD_RESET_ALL_STRING "rst-all"
#define USER_CMD_RESET_ESP_STRING "rst-esp"
#define USER_CMD_RESET_PIC_STRING "rst-pic"
#define USER_CMD_HELP_STRING "help"

// ANSI things
// Reset
#define ANSI_RESET "\x1b[0m"
#define ANSI_BOLD "\x1b[1m"
#define ANSI_ITALIC "\x1b[3m"
#define ANSI_UNDERLINE "\x1b[4m"
#define ANSI_STRIKETHROUGH "\x1b[9m"

// White Text on Colored Backgrounds
#define ANSI_WHITE_BLACK   "\x1b[37;40m"
#define ANSI_WHITE_RED     "\x1b[37;41m"
#define ANSI_WHITE_GREEN   "\x1b[37;42m"
#define ANSI_WHITE_YELLOW  "\x1b[37;43m"
#define ANSI_WHITE_BLUE    "\x1b[37;44m"
#define ANSI_WHITE_MAGENTA "\x1b[37;45m"
#define ANSI_WHITE_CYAN    "\x1b[37;46m"
#define ANSI_WHITE_WHITE   "\x1b[37;47m"

// Black Text on Colored Backgrounds
#define ANSI_BLACK_BLACK   "\x1b[30;40m"
#define ANSI_BLACK_RED     "\x1b[30;41m"
#define ANSI_BLACK_GREEN   "\x1b[30;42m"
#define ANSI_BLACK_YELLOW  "\x1b[30;43m"
#define ANSI_BLACK_BLUE    "\x1b[30;44m"
#define ANSI_BLACK_MAGENTA "\x1b[30;45m"
#define ANSI_BLACK_CYAN    "\x1b[30;46m"
#define ANSI_BLACK_WHITE   "\x1b[30;47m"


USER_CMD_TYPE serial_console_check_1(const char *cmd_buf);
USER_CMD serial_console_check_2_esp(const char *cmd_buf);
USER_CMD serial_console_check_2_pic(const char *cmd_buf);
USER_CMD serial_console_check_2_rst(const char *cmd_buf);
USER_CMD serial_console_check_2_help(const char *cmd_buf);
void serial_console_help(Stream *output);
void serial_console_print_help_all(Stream *output);

void serial_console_exec(Stream *output, USER_CMD cmd, const char *arg_buf);
void sercon_print_wifi(Stream *output);
void sercon_print_ssids(Stream *output);
void serial_console_print_info(Stream *output);

void serial_console_second_changed(uint32_t millis);
bool serial_console_print_local_available(void);
void serial_console_print_local(void);

bool serial_console_validate_uint32(const char* input, uint32_t* output);
bool serial_console_validate_int32(const char* input, int32_t* output);

#endif	/* SERIAL_CONSOLE_H */
