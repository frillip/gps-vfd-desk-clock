/*
 * File:   bme280.h
 * Author: Frillip
 *
 * Created on 14 March 2024, 22:30
 */

#ifndef BME280_H
#define	BME280_H

#ifdef	__cplusplus
extern "C" {
#endif

    
#include "mcc_generated_files/system.h"
#include "mcc_generated_files/pin_manager.h"    
#include "mcc_generated_files/i2c1.h"
#include "mcc_generated_files/delay.h"
#include <time.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#define BME280_TEMP_OFFSET      (-1280L)    // How much warming does the board add in 0.01C

#define BME280_ADDR             (0x76)
#define BME280_CHIP_ID          (0x60)
    
#define BME280_REG_CAL_00_25    (0x88)
#define BME280_REG_ID           (0xD0)
#define BME280_REG_RESET        (0xE0)
#define BME280_REG_CAL_26_41    (0xE1)
#define BME280_REG_CTRL_HUM     (0xF2)
#define BME280_REG_STATUS       (0xF3)
#define BME280_REG_CTRL_MEAS    (0xF4)
#define BME280_REG_CONFIG       (0xF5)
#define BME280_REG_PRES         (0xF7)
#define BME280_REG_TEMP         (0xFA)
#define BME280_REG_HUM          (0xFD)
    
#define BME280_LEN_CMD          1
#define BME280_LEN_CAL_00_25    26
#define BME280_LEN_ID           1
#define BME280_LEN_RESET        1
#define BME280_LEN_CAL_26_41    7
#define BME280_LEN_CTRL_HUM     1
#define BME280_LEN_STATUS       1
#define BME280_LEN_CTRL_MEAS    1
#define BME280_LEN_CONFIG       1
#define BME280_LEN_ALL_CONFIG   4
#define BME280_LEN_P_T_H_DATA   8
#define BME280_LEN_P_DATA       3
#define BME280_LEN_T_DATA       3
#define BME280_LEN_H_DATA       2
    
#define BME280_CMD_RESET        (0xB6)

#define BME280_MODE_SLEEP       (0x00)
#define BME280_MODE_FORCE       (0x01)
#define BME280_MODE_NORMAL      (0x03)
    
#define BME280_OVERSAMPLE_OFF   (0x00)
#define BME280_OVERSAMPLE_X1    (0x01)
#define BME280_OVERSAMPLE_X2    (0x02)
#define BME280_OVERSAMPLE_X4    (0x03)
#define BME280_OVERSAMPLE_X8    (0x04)
#define BME280_OVERSAMPLE_X16   (0x05)

#define BME280_FILTER_OFF       (0x00)
#define BME280_FILTER_X2        (0x01)
#define BME280_FILTER_X4        (0x02)
#define BME280_FILTER_X8        (0x03)
#define BME280_FILTER_X16       (0x04)

#define BME280_STANDBY_0MS5     (0x00)
#define BME280_STANDBY_10MS     (0x06)
#define BME280_STANDBY_20MS     (0x07)
#define BME280_STANDBY_62MS5    (0x01)
#define BME280_STANDBY_125MS    (0x02)
#define BME280_STANDBY_250MS    (0x03)
#define BME280_STANDBY_500MS    (0x04)
#define BME280_STANDBY_1000MS   (0x05)
    
struct bme280_cal_data
{
    uint16_t dig_t1;
    int16_t dig_t2;
    int16_t dig_t3;
    uint16_t dig_p1;
    int16_t dig_p2;
    int16_t dig_p3;
    int16_t dig_p4;
    int16_t dig_p5;
    int16_t dig_p6;
    int16_t dig_p7;
    int16_t dig_p8;
    int16_t dig_p9;
    uint8_t dig_h1;
    int16_t dig_h2;
    uint8_t dig_h3;
    int16_t dig_h4;
    int16_t dig_h5;
    int8_t dig_h6;
    int32_t t_fine;
};

struct bme280_data
{
    uint32_t pressure;
    int32_t temperature;
    uint32_t humidity;
};

struct bme280_uncomp_data
{
    uint32_t pressure;
    uint32_t temperature;
    uint32_t humidity;
};

struct bme280_settings
{
    uint8_t osr_p;
    uint8_t osr_t;
    uint8_t osr_h;
    uint8_t filter;
    uint8_t standby_time;
};
    
bool BME280_init(void);
uint8_t BME280_read_id(void);
void BME280_read_temp(void);
int32_t BME280_comp_temp(uint32_t uncomp_temp);
void BME280_read_pres(void);
uint32_t BME280_comp_pres(uint32_t uncomp_pres);
void BME280_read_hum(void);
uint32_t BME280_comp_hum(uint32_t uncomp_hum);
void BME280_read_all(void);
void BME280_read_settings(void);
bool BME280_read_cal(void);
bool BME280_read_cal_00(void);
bool BME280_read_cal_26(void);
void BME280_write_settings(void);
bool BME280_reset(void);
#define BME280_make16le(msb, lsb)         (((uint16_t)msb << 8) | (uint16_t)lsb)

#ifdef	__cplusplus
}
#endif

#endif	/* BME280_H */

