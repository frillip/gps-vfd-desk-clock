/* 
 * File:   veml6040.h
 * Author: Frillip
 *
 * Created on 11 March 2024, 22:02
 */

#ifndef VEML6040_H
#define	VEML6040_H

#include <stdbool.h>
#include <stdint.h>

#include "i2c1.h"
    
#define VEML6040_ADDRESS            0x10

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
#define VEML6040_SENS_160MS          6292UL
#define VEML6040_SENS_320MS          3146UL
#define VEML6040_SENS_640MS          1573UL
#define VEML6040_SENS_1280MS          787UL // 0.007865 in datasheet
#define VEML6040_SENS_SCALAR_X10    10000UL
    
// Retry a chip that did not answer this often (V2 gave up after 30 tries).
#define VEML6040_DETECT_INTERVAL_MS 1000UL
// Read a present chip this often, so what is printed is recent.
#define VEML6040_READ_INTERVAL_MS 500UL
    
typedef enum
{
    VEML6040_NONE = 0, // no request yet
    VEML6040_READING,
    VEML6040_WRITING,
    VEML6040_DONE,            // the last request worked
    VEML6040_FAILED,          // the last request failed: see last_result
} Veml6040Request;

typedef struct
{
    bool present;
    bool lux_valid;
    bool saturated;
    bool too_dark;
    uint8_t chip_shutdown;
    uint8_t trigger_mode;
    uint8_t manual_mode;
    uint8_t integration_time;
    uint32_t read_ms;     // the millisecond clock when that read finished
    uint32_t read_stamp;  // ... and when its I2C start completed (i2c1 stamp clock)
    uint8_t registers[8]; // control/status 1 to years, as last read
    Veml6040Request request;
    I2cResult last_result;
    uint16_t r_data;
    uint16_t g_data;
    uint16_t b_data;
    uint16_t w_data;
    uint16_t lux_x10;
    uint32_t reads;
    uint32_t writes;
    uint32_t failures;
} Veml6040Status;

bool veml6040_init(void);
bool veml6040_busy(void);
bool veml6040_read(void);
bool veml6040_write(void);
bool veml6040_service(uint32_t now_ms);

void veml6040_set_auto_read(bool enabled);
const Veml6040Status *veml6040_status(void);
const uint16_t veml6040_lux_x10(void);

void veml6040_print(uint32_t now_ms);

#endif	/* VEML6040_H */
