// Version: 2026-10-19T15:15:00+01:00
/*
 * File:   enums.h
 * Author: Frillip
 *
 * Created on 13 April 2024, 19:33
 *
 * Application enums shared by name between the dsPIC and ESP32 sources.
 * None of these values goes on the wire: SML (sml_proto.h) carries fixed
 * SML_* codes, and each side maps its enums onto them with explicit tables.
 *
 * 2026-10-19, SML 1.1: removed the enums the protocol now defines as wire
 * codes, and those that duplicated u-blox definitions:
 *   CLOCK_SOURCE        -> SML_SOURCE_* (values differ: INIT 0, NONE 1, ...)
 *   CLOCK_SYNC_STATUS   -> SML_SYNC_*, SML_CAUSE_*, SML_LOCK_* (V2+ sync/)
 *   TZINFO_SOURCE       -> SML_TZ_SOURCE_*
 *   PIC_RTC_TYPE        -> SML_RTC_*
 *   ESP_/PIC_MESSAGE_TYPE, BOOTLOADER_CMD -> SML classes and IDs
 *   GNSS_MESSAGE_TYPE, UBX_NAV_STATUS_GPSFIX, UBX_NAV_TIMEUTC_VALID_UTCSTANDARD,
 *   UBX_TIM_TM2_FLAGS_TIMEBASE -> ubx_proto.h (UBX_FIX_*, UBX_TIMEUTC_*, UBX_TM2_*)
 * esp_src/ links to this file, and to sml.c, sml.h and sml_proto.h, from
 * esp_src/src/.
 *
 * 2026-10-27: USER_CMD, USER_CMD_TYPE and USER_CMD_SRC removed: the ESP's
 * console commands are a table of names in esp_src/src/console/commands.cpp.
 */

#ifndef ENUMS_H
#define	ENUMS_H

#ifdef	__cplusplus
extern "C" {
#endif

// Wire values are SML_DISPLAY_*, numerically equal by design.
typedef enum
{
    UI_DISPLAY_STATE_INIT = 0,
    UI_DISPLAY_STATE_CLOCK_HHMM,
    UI_DISPLAY_STATE_CLOCK_MMSS,
    UI_DISPLAY_STATE_CLOCK_SSMM,
    UI_DISPLAY_STATE_CLOCK_YYYY,
    UI_DISPLAY_STATE_CLOCK_MMDD,
    UI_DISPLAY_STATE_DELTA,
    UI_DISPLAY_STATE_DASHES, // Here and below considered invalid EEPROM settings
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

#ifdef	__cplusplus
}
#endif

#endif	/* ENUMS_H */

