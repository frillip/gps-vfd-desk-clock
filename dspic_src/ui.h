/* 
 * File:   ui.h
 * Author: Frillip
 *
 * Created on 26 July 2023, 21:49
 */

#ifndef UI_H
#define	UI_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "mcc_generated_files/pin_manager.h"
#include "mcc_generated_files/uart1.h"
#include "gnss_pps.h"
#include "pic_pps.h"
#include "rtc.h"
#include "tubes.h"
#include "sync_state.h"
#include "../common/enums.h"
#include "eeprom.h"

#define UI_BUTTON_SHORT_PRESS_COUNT 2
#define UI_BUTTON_LONG_PRESS_COUNT 50
#define UI_DISPLAY_TIMEOUT_COUNT 1000
    
#define UI_MENU_FLASH_ON_PERIOD 30 // * 20ms
#define UI_MENU_FLASH_PERIOD 50 // * 20ms
#define UI_MENU_FLASH_INITIAL (10)
#define UI_MENU_FLASH_RESET 0

#define UI_ALARM_ENABLED_DEFAULT 0
#define UI_ALARM_OFFSET_DEFAULT 0L
#define UI_ALARM_MINUTE_INCREMENT_STEP_SIZE 300
#define UI_ALARM_OFFSET_MAX 86400UL
#define UI_ALARM_OFFSET_MIN 0UL
    
#define UI_ALARM_BEEP_LENGTH    5
#define UI_ALARM_BEEP_GAP       5
#define UI_ALARM_PAUSE_GAP     25
#define UI_ALARM_GROUP_SIZE     4
#define UI_ALARM_DURATION     120
#define UI_ALARM_MUTE_LENGTH    5
    
#define UI_DELTA_EPOCH_DEFAULT 1725030000L
    
#define UI_BEEP_ENABLED_DEFAULT 0
    
#define UI_DISPLAY_HOUR_FORMAT_DEFAULT 0
#define UI_DISPLAY_SELECTED_DEFAULT UI_DISPLAY_STATE_CLOCK_HHMM
    
#define UI_RESET_WIFI_DEFAULT 0
#define UI_RESET_SETTINGS_DEFAULT 0
#define UI_RESET_ALL_DEFAULT 0
    
#define UI_TZ_AUTOMATIC_DEFAULT 0
#define UI_TZ_OFFSET_DEFAULT 0L
#define UI_TZ_OFFSET_STEP_SIZE 900L // 15 minutes
#define UI_TZ_OFFSET_MAX 50400L // +14:00
#define UI_TZ_OFFSET_MIN -43200L // -12:00
#define UI_TZ_OFFSET_FUDGE 3600L

#define UI_DST_OFFSET_DEFAULT 3600L
#define UI_DST_OFFSET_STEP_SIZE 900L // 15 minutes
#define UI_DST_OFFSET_MAX 7200L // 2 hours
#define UI_DST_OFFSET_MIN 0L // 0 hours
#define UI_DST_AUTOMATIC_DEFAULT 1
#define UI_DST_ACTIVE_DEFAULT 0

void ui_init(void);
void ui_tasks(void);

void ui_button_task(void);
bool ui_button_state(void);
bool ui_switch_state(void);

void ui_buzzer_task(void);
void ui_buzzer_interval_beep(void);
uint8_t ui_buzzer_get_beep_count(uint8_t hour, uint8_t minute);
void ui_buzzer_interval_generate_buffer(uint8_t beep_count);
void ui_buzzer_button_beep(uint8_t beep_count);
void ui_buzzer_button_generate_buffer(uint8_t beep_count);
void ui_buzzer_mute(uint8_t length);
void ui_buzzer_sounder(void);

typedef enum
{
    ALARM_DISABLED = 0,
    ALARM_OFF = 1,
    ALARM_ARMED = 2,
    ALARM_START = 3,
    ALARM_FINISH = 4,
} UI_ALARM_STATE;

void ui_alarm_task(void);
void print_alarm_state(UI_ALARM_STATE state);
void ui_alarm_arm(void);
void ui_alarm_disarm(void);
void ui_alarm_stop(void);
void ui_alarm_reset(void);
bool ui_alarm_check_state(void);
void ui_alarm_sound(void);
void ui_alarm_generate_buffer(void);
void ui_alarm_mute(void);

void ui_display_task(void);
void ui_update_display(void);
void ui_set_display_off(void);
void ui_set_display_hhmm(void);
void ui_set_display_mmss(void);
void ui_set_display_ssmm(void);
void ui_set_display_dashes(void);
void ui_display_cycle(void);

void ui_set_display_menu(void);
void ui_menu_change_state(UI_MENU_STATE new_state);
void ui_menu_start_flash(void);
void ui_menu_stop_flash(void);
void ui_menu_reset_flash(void);
bool ui_in_menu(void);
void ui_menu_long_press(void);
void ui_menu_short_press(void);

void ui_tz_offset_incr(void);
void ui_tz_offset_incr_hh(void);
void ui_tz_offset_incr_mm(void);
void ui_dst_offset_incr(void);
void ui_alarm_offset_incr_hh(void);
void ui_alarm_offset_incr_mm(void);

void pic_reset(void);
void ui_user_cmd(USER_CMD cmd, uint32_t arg);
void ui_print_iso8601_string(time_t iso);
void ui_print_iso8601_string_local(time_t utc);
void ui_print_local_offset(int32_t total_offset);
void ui_print_clear_window(void);

#define BUZZER_BUFFER_LENGTH 100

typedef enum
{
    BUZZER_OFF = 0,
    BUZZER_ON = 1,
} UI_BUZZER_STATE;

struct _buzzer_buffer_element {
    UI_BUZZER_STATE state : 1;
    uint8_t length : 7;
};

// Defined in multiples of 20ms

#define UI_BEEP_LENGTH         5
#define UI_BEEP_GAP            5

#define UI_BEEP_LENGTH_BUTTON  3
#define UI_BEEP_GAP_BUTTON     3
#define UI_BEEP_COUNT_BUTTON_SHORT   1
#define UI_BEEP_COUNT_BUTTON_LONG    1

#define UI_BEEP_MUTE_LENGTH    1

#define UI_BEEP_PAUSE_GAP      15
#define UI_BEEP_GROUP_SIZE     4

#define UI_BEEP_MINOR_LENGTH   5
#define UI_BEEP_MINOR_INTERVAL 15

#define UI_BEEP_MIDNIGHT_HOUR           0
#define UI_BEEP_MIDNIGHT_BEEP_COUNT     12
#define UI_BEEP_AM_PM_HOUR_THRESHOLD    12

#define ui_buzzer_on()      (_LATB7 = 1)
#define ui_buzzer_off()     (_LATB7 = 0)
#define ui_buzzer_state()   _RB7

// Button pulls RA7 to ground
#define ui_button_input_state()   (!_RA7)

// Switch pulls RA0 to ground
#define ui_switch_input_state()   (!_RA0)

#ifdef	__cplusplus
}
#endif

#endif	/* UI_H */

