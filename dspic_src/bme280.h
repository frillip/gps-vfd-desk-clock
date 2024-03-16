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

#define BME280_ADDR             (0x76)
    
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
    
struct bme280_calib_data
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
    
void bme280_init(void);
void bme280_read_temp(void);
void bme280_read_pres(void);
void bme280_read_hum(void);
void bme280_read_settings(void);
void bme280_read_cal(void);
void bme280_write_settings(void);
void bme280_reset(void);

#ifdef	__cplusplus
}
#endif

#endif	/* BME280_H */

