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

#define SEG_G       0x00000004
#define SEG_F       0x00000002
#define SEG_E       0x00000001
#define SEG_D       0x00000040
#define SEG_C       0x00000020
#define SEG_B       0x00000010
#define SEG_A       0x00000008
#define SEG_NONE    0x00000000

#define DIGIT_0    SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F
#define DIGIT_1    SEG_B | SEG_C
#define DIGIT_2    SEG_A | SEG_B | SEG_D | SEG_E | SEG_G
#define DIGIT_3    SEG_A | SEG_B | SEG_C | SEG_D | SEG_G
#define DIGIT_4    SEG_B | SEG_C | SEG_F | SEG_G
#define DIGIT_5    SEG_A | SEG_C | SEG_D | SEG_F | SEG_G
#define DIGIT_6    SEG_A | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G
#define DIGIT_7    SEG_A | SEG_B | SEG_C
#define DIGIT_8    SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G
#define DIGIT_9    SEG_A | SEG_B | SEG_C | SEG_D | SEG_F | SEG_G
#define DIGIT_A    SEG_A | SEG_B | SEG_C | SEG_E | SEG_F | SEG_G
#define DIGIT_B    SEG_C | SEG_D | SEG_E | SEG_F | SEG_G
#define DIGIT_C    SEG_A | SEG_D | SEG_E | SEG_F
#define DIGIT_D    SEG_B | SEG_C | SEG_D | SEG_E | SEG_G
#define DIGIT_E    SEG_A | SEG_D | SEG_E | SEG_F | SEG_G
#define DIGIT_F    SEG_A | SEG_E | SEG_F | SEG_G
#define DIGIT_ALL  SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G
#define DIGIT_DASH SEG_G
#define DIGIT_NONE SEG_NONE

void display_init(void);
void display_count(uint16_t count);
void display_buffer(uint32_t buffer);
void display_time(const time_t *tod);
void display_mmss(const time_t *tod);
void display_latch(void);

#ifdef	__cplusplus
}
#endif

#endif	/* TUBES_H */

