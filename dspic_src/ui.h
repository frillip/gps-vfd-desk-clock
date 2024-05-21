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
#include "../common/enums.h"
#include "eeprom.h"

#define UI_BUTTON_SHORT_PRESS_COUNT 2
#define UI_BUTTON_LONG_PRESS_COUNT 50
#define UI_DISPLAY_TIMEOUT_COUNT 1000
    
#define UI_MENU_FLASH_ON_PERIOD 30 // * 20ms
#define UI_MENU_FLASH_PERIOD 50 // * 20ms
#define UI_MENU_FLASH_INITIAL (-50)
#define UI_MENU_FLASH_RESET 0

#define UI_ALARM_DEFAULT 0L

void ui_init(void);
void ui_tasks(void);

void ui_button_task(void);
bool ui_button_state(void);
bool ui_switch_state(void);

void ui_buzzer_task(void);
void ui_buzzer_interval_beep(void);
void ui_buzzer_sounder(void);

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
void ui_menu_long_press(void);
void ui_menu_short_press(void);

void ui_uart1_input(char c);
void ui_print_iso8601_string(time_t iso);
void ui_print_iso8601_string_local(time_t local);
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
#define ui_button_input_state()   (!_RA7)

// Switch pulls RA0 to ground
#define ui_switch_input_state()   (!_RA0)

#ifdef	__cplusplus
}
#endif

#endif	/* UI_H */

