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
#include "tubes.h"
#include "sync_state.h"
    
typedef enum
{
    UI_DISPLAY_STATE_INIT,
    UI_DISPLAY_STATE_CLOCK_HHMM,
    UI_DISPLAY_STATE_CLOCK_MMSS,
    UI_DISPLAY_STATE_CLOCK_SSMM,
    UI_DISPLAY_STATE_MENU,
} UI_DISPLAY_STATE;

typedef enum
{
    UI_MENU_STATE_ROOT,
    UI_MENU_STATE_TZ,
    UI_MENU_STATE_TZ_AUTO,
    UI_MENU_STATE_TZ_MANUAL,
    UI_MENU_STATE_TZ_SET,
    UI_MENU_STATE_TZ_SET_HH,
    UI_MENU_STATE_TZ_SET_MM,
    UI_MENU_STATE_TZ_BACK,
    UI_MENU_STATE_DST,
    UI_MENU_STATE_DST_AUTO,
    UI_MENU_STATE_DST_MANUAL,
    UI_MENU_STATE_DST_SET,
    UI_MENU_STATE_DST_SET_ON,
    UI_MENU_STATE_DST_SET_OFF,
    UI_MENU_STATE_DST_BACK,
    UI_MENU_STATE_ALARM,
    UI_MENU_STATE_ALARM_ON,
    UI_MENU_STATE_ALARM_OFF,
    UI_MENU_STATE_ALARM_SET,
    UI_MENU_STATE_ALARM_SET_HH,
    UI_MENU_STATE_ALARM_SET_MM,
    UI_MENU_STATE_ALARM_BACK,
    UI_MENU_STATE_BEEP,
    UI_MENU_STATE_BEEP_ON,
    UI_MENU_STATE_BEEP_OFF,
    UI_MENU_STATE_EXIT,
} UI_MENU_STATE;

typedef enum
{
    UI_BUTTON_STATE_NO_PRESS,
    UI_BUTTON_STATE_SHORT_PRESS,
    UI_BUTTON_STATE_LONG_PRESS,
    UI_BUTTON_STATE_HOLD,
} UI_BUTTON_STATE;

#define UI_BUTTON_SHORT_PRESS_COUNT 2
#define UI_BUTTON_LONG_PRESS_COUNT 50
#define UI_DISPLAY_TIMEOUT_COUNT 500

void ui_init(void);
void ui_tasks(void);

void ui_button_task(void);
bool ui_button_state(void);
bool ui_switch_state(void);

void ui_buzzer_task(void);
void ui_buzzer_interval_beep(void);
void ui_buzzer_sounder(void);

void ui_display_task(void);

void ui_uart1_input(char c);
void ui_print_iso8601_string(time_t iso);
void ui_print_clear_window(void);

#define BUZZER_BUFFER_LENGTH 64

// Defined in multiples of 20ms

#define BEEP_LENGTH         5
#define BEEP_GAP            5
#define BEEP_PAUSE_GAP      15
#define BEEP_GROUP_SIZE     4

#define BEEP_MINOR_LENGTH   5
#define BEEP_MINOR_INTERVAL 15

#define ui_buzzer_on()      (_LATB7 = 1)
#define ui_buzzer_off()     (_LATB7 = 0)
#define ui_buzzer_state()   _RB7

// Button pulls RA7 to ground
#define ui_button_input_state()   !_RA7

// Switch pulls RA0 to ground
#define ui_switch_input_state()   !_RA0

#ifdef	__cplusplus
}
#endif

#endif	/* UI_H */

