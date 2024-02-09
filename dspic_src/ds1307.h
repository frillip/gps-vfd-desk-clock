/* 
 * File:   ds1307.h
 * Author: Frillip
 *
 * Created on October 17, 2021, 11:26 PM
 */

#ifndef DS1307_H
#define	DS1307_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include "mcc_generated_files/i2c1.h"

#define DS1307_ADDRESS 0x68
#define DS1307_TIME 0x00
#define DS1307_SECONDS 0x00
#define DS1307_MINUTES 0x01
#define DS1307_HOURS 0x02
#define DS1307_DAY 0x03
#define DS1307_DATE 0x04
#define DS1307_MONTH 0x05
#define DS1307_YEAR 0x06
#define DS1307_CONTROL 0x07
#define DS1307_NVRAM 0x08

time_t DS1307_read(void);
bool DS1307_write(time_t rtc);
uint8_t DS1307_bin2bcd(uint8_t val);
uint8_t DS1307_bcd2bin(uint8_t val);

#ifdef	__cplusplus
}
#endif

#endif	/* DS1307_H */

