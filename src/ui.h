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
#include "tubes.h"

void ui_init(void);
void ui_tasks(void);

void ui_button_task(void);
bool ui_button_state(void);
bool ui_switch_state(void);

void ui_buzzer_task(void);
void ui_buzzer_interval_beep(void);
void ui_buzzer_sounder(void);

void ui_display_task(void);

void print_iso8601_string(time_t iso);

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
