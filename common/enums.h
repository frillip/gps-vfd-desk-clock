/* 
 * File:   enums.h
 * Author: Frillip
 *
 * Created on 13 April 2024, 19:33
 */

#ifndef ENUMS_H
#define	ENUMS_H

#ifdef	__cplusplus
extern "C" {
#endif

typedef enum
{
    GNSS_NONE = 0,
    GNSS_UBX_TIM_TM2,
    GNSS_UBX_NAV_TIMEUTC,
    GNSS_UBX_NAV_CLOCK,
    GNSS_UBX_NAV_STATUS,
    GNSS_UBX_NAV_POSLLH,
    GNSS_GNRMC,
} GNSS_MESSAGE_TYPE;

typedef enum
{
    GPSFIX_NO_FIX = 0,
    GPSFIX_DR_ONLY = 1,
    GPSFIX_2D = 2,
    GPSFIX_3D = 3,
    GPSFIX_GPS_DR = 4,
    GPSFIX_TIME_ONLY = 5,
} UBX_NAV_STATUS_GPSFIX;

typedef enum
{
    UTC_INA = 0,
    UTC_CRL = 1,
    UTC_NIST = 2,
    UTC_USNO = 3,
    UTC_BIPM = 4,
    UTC_EL = 5,
    UTC_SU = 6,
    UTC_NTSC = 7,
    UTC_NPLI = 8,
    UTC_UNKNOWN = 15,
} UBX_NAV_TIMEUTC_VALID_UTCSTANDARD;

typedef enum
{
    TIMEBASE_RECV = 0,
    TIMEBASE_GNSS = 1,
    TIMEBASE_UTC = 2,
} UBX_TIM_TM2_FLAGS_TIMEBASE;

typedef enum
{
    SYNC_POWER_ON = 0,
    SYNC_RTC_DETECT,
    SYNC_NTP_DETECT,
    SYNC_GNSS_DETECT,
    SYNC_GNSS_WAIT_FOR_FIX,
    SYNC_STARTUP,
    SYNC_NOSYNC,
    SYNC_ADJUST_STAGE_1,
    SYNC_ADJUST_STAGE_2,
    SYNC_PPS_SYNC,
    SYNC_SCHED_SYNC,
    SYNC_MIN_INTERVAL,
    SYNC_INTERVAL,
    SYNC_SYNC,
    SYNC_NOSYNC_MINOR,
    SYNC_NOSYNC_MINOR_OC,
    SYNC_NOSYNC_MAJOR,
    SYNC_NOSYNC_MAJOR_OC,
    SYNC_NOSYNC_FREQ,
    SYNC_NOSYNC_FREQ_ONLY,
    SYNC_NOSYNC_GNSS,
    SYNC_NOSYNC_MANUAL,
    SYNC_NO_CLOCK,
    SYNC_RTC,
    SYNC_NTP,
    SYNC_NTP_ADJUST,
    SYNC_NTP_NO_NETWORK,
} CLOCK_SYNC_STATUS;

typedef enum
{
    CLOCK_SOURCE_NONE = 0,
    CLOCK_SOURCE_RTC,
    CLOCK_SOURCE_ESP,
    CLOCK_SOURCE_NTP,
    CLOCK_SOURCE_GNSS,
} CLOCK_SOURCE;

typedef enum
{
    UI_DISPLAY_STATE_INIT = 0,
    UI_DISPLAY_STATE_CLOCK_HHMM,
    UI_DISPLAY_STATE_CLOCK_MMSS,
    UI_DISPLAY_STATE_CLOCK_SSMM,
    UI_DISPLAY_STATE_CLOCK_YYYY,
    UI_DISPLAY_STATE_CLOCK_MMDD,
    UI_DISPLAY_STATE_DASHES,
    UI_DISPLAY_STATE_TEMP,
    UI_DISPLAY_STATE_MENU,
} UI_DISPLAY_STATE;

typedef enum
{
    UI_MENU_STATE_ROOT = 0,
    UI_MENU_STATE_TIME,
    UI_MENU_STATE_TIME_AUTO,
    UI_MENU_STATE_TIME_AUTO_SEL,
    UI_MENU_STATE_TIME_SET,
    UI_MENU_STATE_TIME_SET_YEAR,
    UI_MENU_STATE_TIME_SET_MON,
    UI_MENU_STATE_TIME_SET_DAY,
    UI_MENU_STATE_TIME_SET_HOUR,
    UI_MENU_STATE_TIME_SET_MIN,
    UI_MENU_STATE_TIME_BACK,
    UI_MENU_STATE_TZ,
    UI_MENU_STATE_TZ_AUTO,
    UI_MENU_STATE_TZ_AUTO_SEL,
    UI_MENU_STATE_TZ_SET,
    UI_MENU_STATE_TZ_SET_HH,
    UI_MENU_STATE_TZ_SET_MM,
    UI_MENU_STATE_TZ_BACK,
    UI_MENU_STATE_DST,
    UI_MENU_STATE_DST_AUTO,
    UI_MENU_STATE_DST_AUTO_SEL,
    UI_MENU_STATE_DST_STATE,
    UI_MENU_STATE_DST_STATE_SEL,
    UI_MENU_STATE_DST_OFFSET,
    UI_MENU_STATE_DST_OFFSET_SEL,
    UI_MENU_STATE_DST_BACK,
    UI_MENU_STATE_ALARM,
    UI_MENU_STATE_ALARM_ENABLED,
    UI_MENU_STATE_ALARM_ENABLED_SEL,
    UI_MENU_STATE_ALARM_SET,
    UI_MENU_STATE_ALARM_SET_HH,
    UI_MENU_STATE_ALARM_SET_MM,
    UI_MENU_STATE_ALARM_BACK,
    UI_MENU_STATE_BEEP,
    UI_MENU_STATE_BEEP_ENABLE,
    UI_MENU_STATE_BEEP_ENABLE_SEL,
    UI_MENU_STATE_BEEP_BACK,
    UI_MENU_STATE_DISPLAY,
    UI_MENU_STATE_DISPLAY_FORMAT,
    UI_MENU_STATE_DISPLAY_FORMAT_SEL,
    UI_MENU_STATE_DISPLAY_SET,
    UI_MENU_STATE_DISPLAY_SET_SEL,
    UI_MENU_STATE_DISPLAY_BACK,
    UI_MENU_STATE_RESET,
    UI_MENU_STATE_RESET_WIFI,
    UI_MENU_STATE_RESET_WIFI_YN,
    UI_MENU_STATE_RESET_SETTINGS,
    UI_MENU_STATE_RESET_SETTINGS_YN,
    UI_MENU_STATE_RESET_ALL,
    UI_MENU_STATE_RESET_ALL_YN,
    UI_MENU_STATE_RESET_BACK,
    UI_MENU_STATE_EXIT,
} UI_MENU_STATE;

typedef enum
{
    UI_BUTTON_STATE_NO_PRESS = 0,
    UI_BUTTON_STATE_SHORT_PRESS,
    UI_BUTTON_STATE_LONG_PRESS,
    UI_BUTTON_STATE_HOLD,
} UI_BUTTON_STATE;

typedef enum
{
    ESP_NONE = 0,
    ESP_TIME,
    ESP_GNSS,
    ESP_OFFSET,
    ESP_NET,
    ESP_RTC,
    ESP_SENSOR,
    ESP_DISPLAY,
    ESP_USER,
} ESP_MESSAGE_TYPE;

typedef enum
{
    PIC_NONE = 0,
    PIC_TIME,
    PIC_GNSS,
    PIC_OFFSET,
    PIC_NET,
    PIC_RTC,
    PIC_SENSOR,
    PIC_DISPLAY,
    PIC_USER,
} PIC_MESSAGE_TYPE;

typedef enum
{
    USER_CMD_NONE = 0,
    USER_CMD_ESP_RESET,
    USER_CMD_ESP_SET_INTERVAL,
    USER_CMD_ESP_SET_SERVER,
    USER_CMD_ESP_RESYNC,
    USER_CMD_ESP_WIFI_INFO,
    USER_CMD_ESP_WIFI_SHOW,
    USER_CMD_ESP_WIFI_CONNECT,
    USER_CMD_ESP_WIFI_DISCONNECT,
    USER_CMD_ESP_WIFI_SSID,
    USER_CMD_ESP_WIFI_PASS,
    USER_CMD_ESP_WIFI_DHCP,
    USER_CMD_ESP_WIFI_IP,
    USER_CMD_ESP_WIFI_MASK,
    USER_CMD_ESP_WIFI_GATEWAY,
    USER_CMD_ESP_WIFI_CLEAR,
    USER_CMD_ESP_WIFI_SETUP,
    USER_CMD_ESP_CLEAR_ALL,
    USER_CMD_ESP_SAVE,
    USER_CMD_PIC_INFO,
    USER_CMD_PIC_RESYNC,
    USER_CMD_PIC_RESET,
    USER_CMD_PIC_SET_RTC,
    USER_CMD_PIC_SET_TZ_OFFSET,
    USER_CMD_PIC_SET_DST_OFFSET,
    USER_CMD_PIC_SET_DST_AUTO,
    USER_CMD_PIC_SET_DST_ACTIVE,
    USER_CMD_PIC_SET_ALARM_ENABLED,
    USER_CMD_PIC_SET_ALARM,
    USER_CMD_PIC_SET_BEEPS,
    USER_CMD_PIC_SET_DISPLAY,
    USER_CMD_PIC_SET_BRIGHTNESS_AUTO,
    USER_CMD_PIC_SET_BRIGHTNESS,
    USER_CMD_PIC_EEPROM_SHOW,
    USER_CMD_PIC_CLEAR_ALL,
    USER_CMD_PIC_SAVE,
    USER_CMD_RESET_ALL,
    USER_CMD_HELP,
} USER_CMD;

typedef enum
{
    USER_CMD_TYPE_NONE = 0,
    USER_CMD_TYPE_ESP,
    USER_CMD_TYPE_PIC,
    USER_CMD_TYPE_RST,
    USER_CMD_TYPE_HELP,
} USER_CMD_TYPE;


#ifdef	__cplusplus
}
#endif

#endif	/* ENUMS_H */

