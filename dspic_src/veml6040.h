/* 
 * File:   veml6040.h
 * Author: Frillip
 *
 * Created on 11 March 2024, 22:02
 */

#ifndef VEML6040_H
#define	VEML6040_H

#ifdef	__cplusplus
extern "C" {
#endif


#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "mcc_generated_files/i2c1.h"
#include "tubes.h"
#include "ui.h"
    
#define VEML6040_ADDR               0x10

#define VEML6040_CMD_CONF           0x00
#define VEML6040_CMD_R_DATA         0x08
#define VEML6040_CMD_G_DATA         0x09
#define VEML6040_CMD_B_DATA         0x0A
#define VEML6040_CMD_W_DATA         0x0B
    
#define VEML6040_CONF_SD_ENABLE     0x00
#define VEML6040_CONF_SD_DISABLE    0x01
    
#define VEML6040_CONF_AF_AUTO       0x00
#define VEML6040_CONF_AF_FORCE      0x02

#define VEML6040_CONF_TRIG_DISABLE  0x00
#define VEML6040_CONF_TRIG_ENABLE   0x04

#define VEML6040_CONF_IT_40MS       0x00
#define VEML6040_CONF_IT_80MS       0x10
#define VEML6040_CONF_IT_160MS      0x20
#define VEML6040_CONF_IT_320MS      0x30
#define VEML6040_CONF_IT_640MS      0x40
#define VEML6040_CONF_IT_1280MS     0x50
    
#define VEML6040_SENS_40MS          25168UL
#define VEML6040_SENS_80MS          12584UL
#define VEML6040_SENS_160MS         06292UL
#define VEML6040_SENS_320MS         03146UL
#define VEML6040_SENS_640MS         01573UL
#define VEML6040_SENS_1280MS        00787UL // 0.007865 in datasheet
#define VEML6040_SENS_SCALAR       100000UL
    
#define VEML_DISPLAY_BRIGHTNESS_CONST       (0)
#define VEML_DISPLAY_BRIGHTNESS_FIRST       (3)
#define VEML_DISPLAY_BRIGHTNESS_SECOND      (0)
    
bool VEML6040_init(void);
uint16_t VEML6040_get_red(void);
uint16_t VEML6040_get_green(void);
uint16_t VEML6040_get_blue(void);
uint16_t VEML6040_get_white(void);
uint16_t VEML_get_data(uint8_t reg);
uint16_t VEML6040_get_lux(void);
uint16_t VEML_calc_brightness(uint16_t lux);
void print_veml_data(void);

#ifdef	__cplusplus
}
#endif

#endif	/* VEML6040_H */

