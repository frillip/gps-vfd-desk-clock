/* 
 * File:   tubes.h
 * Author: Frillip
 *
 * Created on October 2, 2021, 12:33 AM
 */

#ifndef TUBES_H
#define	TUBES_H

#ifdef	__cplusplus
extern "C" {
#endif

#include "mcc_generated_files/system.h"
#include "mcc_generated_files/pin_manager.h"    
#include "mcc_generated_files/spi2.h"
#include "mcc_generated_files/delay.h"
#include <time.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
    
#include "ui.h"
#include "../common/enums.h"
#include "eeprom.h"

// Digit offsets
#define TUBE_4_OFFSET   33
#define TUBE_3_OFFSET   20
#define TUBE_2_OFFSET   13
#define TUBE_1_OFFSET   0    

// Segment mapping on the driver
#define SEG_G       0x00000004ULL
#define SEG_F       0x00000002ULL
#define SEG_E       0x00000001ULL
#define SEG_D       0x00000040ULL
#define SEG_C       0x00000020ULL
#define SEG_B       0x00000010ULL
#define SEG_A       0x00000008ULL
#define SEG_NONE    0x00000000ULL

// Define the segments required for each digit
#define DIGIT_0    ( SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F )
#define DIGIT_1    ( SEG_B | SEG_C )
#define DIGIT_2    ( SEG_A | SEG_B | SEG_D | SEG_E | SEG_G )
#define DIGIT_3    ( SEG_A | SEG_B | SEG_C | SEG_D | SEG_G )
#define DIGIT_4    ( SEG_B | SEG_C | SEG_F | SEG_G )
#define DIGIT_5    ( SEG_A | SEG_C | SEG_D | SEG_F | SEG_G )
#define DIGIT_6    ( SEG_A | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G )
#define DIGIT_7    ( SEG_A | SEG_B | SEG_C )
#define DIGIT_8    ( SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G )
#define DIGIT_9    ( SEG_A | SEG_B | SEG_C | SEG_D | SEG_F | SEG_G )
#define DIGIT_A    ( SEG_A | SEG_B | SEG_C | SEG_E | SEG_F | SEG_G )
#define DIGIT_B    ( SEG_C | SEG_D | SEG_E | SEG_F | SEG_G )
#define DIGIT_C    ( SEG_A | SEG_D | SEG_E | SEG_F )
#define DIGIT_D    ( SEG_B | SEG_C | SEG_D | SEG_E | SEG_G )
#define DIGIT_E    ( SEG_A | SEG_D | SEG_E | SEG_F | SEG_G )
#define DIGIT_F    ( SEG_A | SEG_E | SEG_F | SEG_G )
#define DIGIT_G    ( SEG_A | SEG_C | SEG_D | SEG_E | SEG_F )
#define DIGIT_H    ( SEG_B | SEG_C | SEG_E | SEG_F | SEG_G )
#define DIGIT_I    ( SEG_E | SEG_F )
#define DIGIT_J    ( SEG_B | SEG_C | SEG_D | SEG_E )
#define DIGIT_K    ( SEG_A | SEG_C | SEG_E | SEG_F | SEG_G )
#define DIGIT_L    ( SEG_D | SEG_E | SEG_F )
#define DIGIT_M    ( SEG_A | SEG_C | SEG_E )
#define DIGIT_N    ( SEG_A | SEG_B | SEG_C | SEG_E | SEG_F )
#define DIGIT_O    ( SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F )
#define DIGIT_P    ( SEG_A | SEG_B | SEG_E | SEG_F | SEG_G )
#define DIGIT_Q    ( SEG_A | SEG_B | SEG_C | SEG_F | SEG_G )
#define DIGIT_R    ( SEG_A | SEG_B | SEG_E | SEG_F )
#define DIGIT_S    ( SEG_A | SEG_C | SEG_D | SEG_F | SEG_G )
#define DIGIT_T    ( SEG_D | SEG_E | SEG_F | SEG_G )
#define DIGIT_U    ( SEG_B | SEG_C | SEG_D | SEG_E | SEG_F )
#define DIGIT_V    ( SEG_B | SEG_C | SEG_D | SEG_F )
#define DIGIT_W    ( SEG_B | SEG_D | SEG_F )
#define DIGIT_X    ( SEG_B | SEG_C | SEG_E | SEG_F | SEG_G )
#define DIGIT_Y    ( SEG_B | SEG_C | SEG_D | SEG_F | SEG_G )
#define DIGIT_Z    ( SEG_A | SEG_B | SEG_D | SEG_E | SEG_G )
#define DIGIT_ALL  ( SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G )
#define DIGIT_DASH ( SEG_G )
#define DIGIT_NONE ( SEG_NONE )
    
#define START_SEPARATOR_DOT         0x080000000
#define START_SEPARATOR_LINE        0x100000000
#define START_SEPARATOR_BOTH        ( START_SEPARATOR_DOT | START_SEPARATOR_LINE )
#define MIDDLE_SEPARATOR_DOT        0x0800
#define MIDDLE_SEPARATOR_LINE       0x1000
#define MIDDLE_SEPARATOR_BOTH       ( MIDDLE_SEPARATOR_DOT | MIDDLE_SEPARATOR_LINE )
    
#define DISPLAY_MASK_TUBE_1         0xFFFFFFFFFFFFFF80
#define DISPLAY_MASK_TUBE_2         0xFFFFFFFFFFF01FFF
#define DISPLAY_MASK_TUBE_3         0xFFFFFFFFF80FFFFF
#define DISPLAY_MASK_TUBE_4         0xFFFFFF01FFFFFFFF
    
#define DISPLAY_BRIGHTNESS_HZ           10000
#define DISPLAY_BRIGHTNESS_PR           3999    // OC3RS = (40000000/10000) - 1
#define DISPLAY_BRIGHTNESS_MAX          3800    // OC3R must be less than OC3RS
#define DISPLAY_BRIGHTNESS_DEFAULT      2000
#define DISPLAY_BRIGHTNESS_MIN          200
#define DISPLAY_BRIGHTNESS_STEP         200
#define DISPLAY_BRIGHTNESS_TARGET_STEP  20

void spi2_dma_init(void);
void OC3_Initialize(void);
void display_init(void);
void display_brightness_off(void);
void display_brightness_min(void);
void display_brightness_max(void);
void display_brightness_set(uint16_t brightness);
void display_brightness_set_manual(void);
void display_brightness_set_auto(void);
void display_brightness_set_target(uint16_t target);
void display_brightness_up_step(void);
void display_brightness_down_step(void);
void display_brightness_up(uint16_t brightness_up);
void display_brightness_down(uint16_t brightness_down);
void display_brightness_on(void);
void display_brightness_update(void);
void display_count(int16_t count);
void display_temp(int32_t temp);
uint64_t display_generate_buffer(uint16_t digits);
void display_send_buffer(uint64_t buffer);
void display_hhmm(const time_t *time);
void display_hhmm_24(const time_t *time);
void display_hhmm_12(const time_t *time);
void display_mmss(const time_t *time);
void display_ssmm(const time_t *time);
void display_yyyy(const time_t *time);
void display_mmdd(const time_t *time);
void display_offset(int32_t offset);
void display_mask_12h(void);
void display_mask_hh(void);
void display_mask_mm(void);
void display_menu(void);
void display_menu_text(void);
void display_dashes(void);
void display_all(void);
void display_blank(void);
void display_latch(void);
void display_local_time(time_t time);
bool isDST(const time_t *tod);
void display_timezone_incr(void);
void display_timezone_incr_hh(void);
void display_timezone_incr_mm(void);
void display_timezone_decr(void);

#ifdef	__cplusplus
}
#endif

#endif	/* TUBES_H */
