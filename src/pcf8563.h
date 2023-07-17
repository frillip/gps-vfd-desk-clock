/* 
 * File:   pcf8563.h
 * Author: Frillip
 *
 * Created on October 17, 2021, 11:26 PM
 */

#ifndef PCF8563_H
#define	PCF8563_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include "mcc_generated_files/i2c1.h"

#define PCF8563_ADDRESS 0x51

#define PCF8563_CONTROL_STATUS_1 0x00
#define PCF8563_CONTROL_STATUS_2 0x01
#define PCF8563_TIME 0x02
#define PCF8563_VL_SECONDS 0x02
#define PCF8563_MINUTES 0x03
#define PCF8563_HOURS 0x04
#define PCF8563_DATE 0x05
#define PCF8563_DAYS 0x05
#define PCF8563_WEEKDAYS 0x06
#define PCF8563_CENTURY_MONTH 0x07
#define PCF8563_YEARS 0x08
#define PCF8563_MINUTE_ALARM 0x09
#define PCF8563_HOUR_ALARM 0x0A
#define PCF8563_DAY_ALARM 0x0B
#define PCF8563_WEEKDAY_ALARM 0x0C
#define PCF8563_CLKOUT_CONTROL 0x0D
#define PCF8563_TIMER_CONTROL 0x0E
#define PCF8563_TIMER 0x0F

time_t PCF8563_read(void);
bool PCF8563_write(time_t rtc);
uint8_t bin2bcd(uint8_t val);
uint8_t bcd2bin(uint8_t val);

#ifdef	__cplusplus
}
#endif

#endif	/* PCF8563_H */

