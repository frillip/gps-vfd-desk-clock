/* 
 * File:   eeprom.h
 * Author: Frillip
 *
 * Created on 19 May 2024, 22:22
 */

#ifndef EEPROM_H
#define	EEPROM_H

#ifdef	__cplusplus
extern "C" {
#endif
    
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "../common/enums.h"

typedef union
{
  struct __attribute__ ((packed)) _eeprom_data_struct
  {
    struct __attribute__ ((packed))
    {
        uint8_t tz_auto : 1;
        int8_t tz_offset: 7;
    } tz_data;
    struct __attribute__ ((packed))
    {
        uint8_t dst_auto : 1;
        uint8_t dst_active : 1;
        int8_t dst_offset: 4;
    } dst_data;
    struct __attribute__ ((packed))
    {
        uint8_t alarm_enabled : 1;
        uint16_t alarm_offset: 15;
    } alarm_data;
    struct __attribute__ ((packed))
    {
        uint8_t beep_enabled : 1;
    } beep_data;
    struct __attribute__ ((packed))
    {
        uint8_t hour_format : 1;
        uint8_t padding: 7;
        UI_DISPLAY_STATE current : 8;
        UI_DISPLAY_STATE selected : 8;
    } display_data;
  } fields;
  uint8_t raw[sizeof(struct _eeprom_data_struct)];
} EEPROM_DATA_STRUCT;


#ifdef	__cplusplus
}
#endif

#endif	/* EEPROM_H */

