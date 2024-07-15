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

#define LOCAL_TIME_PRINT_DELAY 50
#define SERIAL_CONSOLE_BUFFER_LENGTH 100

#define USER_CMD_STAGE_1_ESP_STRING "esp-"
#define USER_CMD_STAGE_1_PIC_STRING "pic-"
#define USER_CMD_STAGE_1_RST_STRING "rst-"
#define USER_CMD_STAGE_1_HELP_STRING "help"
#define USER_CMD_STAGE_1_LENGTH 4

#define USER_CMD_ESP_WIFI_PREFIX_STRING "wifi-"
#define USER_CMD_ESP_WIFI_PREFIX_LENGTH 5

#define USER_CMD_ESP_RESET_STRING "esp-reset"
#define USER_CMD_ESP_SET_INTERVAL_STRING "esp-set-interval"
#define USER_CMD_ESP_SET_SERVER_STRING "esp-set-server"
#define USER_CMD_ESP_RESYNC_STRING "esp-resync"
#define USER_CMD_ESP_WIFI_INFO_STRING "esp-wifi-info"
#define USER_CMD_ESP_WIFI_SHOW_STRING "esp-wifi-show"
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
#define USER_CMD_ESP_CLEAR_ALL_STRING "esp-clear-all"
#define USER_CMD_ESP_SAVE_STRING "esp-save"
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
#define USER_CMD_PIC_SET_BEEPS_STRING "pic-set-beeps"
#define USER_CMD_PIC_SET_DISPLAY_STRING "pic-set-display"
#define USER_CMD_PIC_SET_BRIGHTNESS_AUTO_STRING "pic-set-brightness-auto"
#define USER_CMD_PIC_SET_BRIGHTNESS_STRING "pic-set-brightness"
#define USER_CMD_PIC_EEPROM_SHOW_STRING "pic-eeprom-show"
#define USER_CMD_PIC_CLEAR_ALL_STRING "pic-clear-all"
#define USER_CMD_PIC_SAVE_STRING "pic-save"
#define USER_CMD_RESET_ALL_STRING "rst-all"
#define USER_CMD_RESET_ESP_STRING "rst-esp"
#define USER_CMD_RESET_PIC_STRING "rst-pic"
#define USER_CMD_HELP_STRING "help"

#define DEBUG_UART  0
#define DEBUG_BAUD  115200

void serial_console_init();
bool serial_console_char_available(void);
void serial_console_task(void);
USER_CMD_TYPE serial_console_check_1(void);
USER_CMD serial_console_check_2_esp(void);
USER_CMD serial_console_check_2_pic(void);
USER_CMD serial_console_check_2_rst(void);
USER_CMD serial_console_check_2_help(void);
void serial_console_help(void);
void serial_console_print_help_all(void);
void serial_console_exec(USER_CMD cmd);
void serial_console_print_info(void);

void serial_console_second_changed(uint32_t millis);
bool serial_console_print_local_available(void);
void serial_console_print_local(void);