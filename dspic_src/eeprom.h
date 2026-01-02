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
#include "freq.h"
#include "tubes.h"
#include "ui.h"
    
#include "mcc_generated_files/memory/flash.h"
    
#define EEPROM_HEADER_VALUE 0xC0FFEEEE
#define EEPROM_MINIMUM_FOSC_WRITE_INTERVAL 86400UL
    
typedef union
{
  struct __attribute__ ((packed)) _eeprom_data_struct
  {
    uint32_t header;
    struct __attribute__ ((packed))
    {
        struct __attribute__ ((packed))
        {
            bool automatic : 1;
            uint16_t padding : 15;
        } flags;
        int32_t offset;
    } tz;
    struct __attribute__ ((packed))
    {
        struct __attribute__ ((packed))
        {
            bool automatic : 1;
            bool active : 1;
            uint16_t padding : 14;
        } flags;
        int32_t offset;
        // Store the next 4 auto DST transitions
        // TO DO: Add terminal command to set these manually
        // TO DO: Once the DST transition has passed, move DST transitions
        // to next element, so index 0 is always the incoming transition
		time_t next[4];
    } dst;
    struct __attribute__ ((packed))
    {
		uint32_t fosc_freq;
    } pps;
    struct __attribute__ ((packed))
    {
        struct __attribute__ ((packed))
        {
            bool enabled : 1;
            uint16_t padding : 15;
        } flags;
        int32_t offset;
    } alarm;
    struct __attribute__ ((packed))
    {
        struct __attribute__ ((packed))
        {
            uint16_t padding : 16;
        } flags;
        time_t epoch;
    } delta;
    struct __attribute__ ((packed))
    {
        struct __attribute__ ((packed))
        {
            bool enabled : 1;
            uint16_t padding : 15;
        } flags;
    } beep;
    struct __attribute__ ((packed))
    {
        struct __attribute__ ((packed))
        {
            bool hour_12_format : 1;
            bool display_brightness_manual : 1;
            uint16_t padding: 14;
        } flags;
        struct __attribute__ ((packed))
        {
            bool manual : 1;
            uint16_t level: 15;
        } brightness;
        UI_DISPLAY_STATE current : 8;
        UI_DISPLAY_STATE selected : 8;
    } display;
    
    struct __attribute__ ((packed))
    {
        struct __attribute__ ((packed))
        {
            bool wifi : 1;
            bool settings : 1;
            bool all : 1;
            uint16_t padding: 13;
        } flags;
    } reset;
    uint32_t checksum; // unused
  } fields;
  uint16_t raw[sizeof(struct _eeprom_data_struct)];
} EEPROM_DATA_STRUCT;


void eeprom_init(void);
void eeprom_read(void);
void eeprom_write(void);
bool eeprom_check_settings(void);
void eeprom_reset_settings(void);
void eeprom_clear_pending_changes(void);
void eeprom_print_settings(void);
void eeprom_print_stored_settings(void);
void eeprom_write_fosc(void);


#ifdef	__cplusplus
}
#endif

#endif	/* EEPROM_H */

