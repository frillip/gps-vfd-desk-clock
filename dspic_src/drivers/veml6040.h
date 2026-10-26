/*
 * Vishay VEML6040 colour light sensor on I2C1 (address 0x10), as fitted to
 * V2: the ambient light for automatic brightness (ui/brightness.h). Ported
 * from V2's veml6040.c onto drivers/i2c1.h, as the PCF8563 was: nothing
 * waits.
 *
 * The sensor measures continuously once configured (auto mode, the
 * integration time VEML6040_INTEGRATION). Only the green channel is read:
 * the data sheet gives lux from it directly, at VEML6040_SENSITIVITY_*
 * lux per count for each integration time (in 1/100000 lux, so lux_x10 =
 * count x sensitivity / 10000).
 *
 *   - Configured when first found, and the configuration read back (the
 *     register is readable). It is read back again every
 *     VEML6040_CHECK_INTERVAL_MS and rewritten if it differs, and after
 *     the chip has not answered: a reset puts it back to its power-on
 *     default (40 ms), which would read 8x low. Counts read while it is not
 *     known to be configured are not used.
 *   - Read every VEML6040_READ_INTERVAL_MS; a missing chip is tried every
 *     VEML6040_DETECT_INTERVAL_MS.
 *   - saturated: the count is at full scale, so the light is at least
 *     what that reads as; too_dark: a count of 0, below one count's worth.
 *     lux_valid is set for neither.
 *
 * One request at a time, finished by veml6040_service() from the main loop.
 */
#ifndef VEML6040_H
#define VEML6040_H

#include <stdbool.h>
#include <stdint.h>

#include "i2c1.h"

#define VEML6040_ADDRESS 0x10U

// Command codes (registers): the configuration, then the four channels.
#define VEML6040_CMD_CONF   0x00U
#define VEML6040_CMD_R_DATA 0x08U
#define VEML6040_CMD_G_DATA 0x09U
#define VEML6040_CMD_B_DATA 0x0AU
#define VEML6040_CMD_W_DATA 0x0BU

// Configuration register, low byte.
#define VEML6040_CONF_SD_ENABLE    0x00U // measuring (SD: shut down)
#define VEML6040_CONF_SD_DISABLE   0x01U
#define VEML6040_CONF_AF_AUTO      0x00U // measures continuously
#define VEML6040_CONF_AF_FORCE     0x02U // one measurement per trigger
#define VEML6040_CONF_TRIG_DISABLE 0x00U
#define VEML6040_CONF_TRIG_ENABLE  0x04U
#define VEML6040_CONF_IT_40MS      0x00U
#define VEML6040_CONF_IT_80MS      0x10U
#define VEML6040_CONF_IT_160MS     0x20U
#define VEML6040_CONF_IT_320MS     0x30U
#define VEML6040_CONF_IT_640MS     0x40U
#define VEML6040_CONF_IT_1280MS    0x50U
#define VEML6040_CONF_IT_MASK      0x70U

// Green channel sensitivity, 1/100000 lux per count, for each integration
// time (data sheet; 0.007865 at 1280 ms rounds to 787).
#define VEML6040_SENSITIVITY_40MS   25168UL
#define VEML6040_SENSITIVITY_80MS   12584UL
#define VEML6040_SENSITIVITY_160MS  6292UL
#define VEML6040_SENSITIVITY_320MS  3146UL
#define VEML6040_SENSITIVITY_640MS  1573UL
#define VEML6040_SENSITIVITY_1280MS 787UL

// 320 ms: 0.03 lux a count, full scale 2062 lux.
#define VEML6040_INTEGRATION VEML6040_CONF_IT_320MS

// Retry a chip that did not answer this often.
#define VEML6040_DETECT_INTERVAL_MS 1000UL
// Read a present chip this often: once per measurement at most, so each
// read is a fresh one (the data sheet: wait an integration time between
// reads). 320 ms is 16 cycles of 50 Hz mains, so lamp flicker averages out.
#define VEML6040_READ_INTERVAL_MS 500UL
// Read the configuration back this often.
#define VEML6040_CHECK_INTERVAL_MS 60000UL

typedef enum
{
    VEML6040_NONE = 0, // no request yet
    VEML6040_READING,
    VEML6040_WRITING,
    VEML6040_CHECKING, // reading the configuration back
    VEML6040_DONE,     // the last request worked
    VEML6040_FAILED,   // the last request failed: see last_result
} Veml6040Request;

typedef struct
{
    bool present;
    bool configured; // the configuration was read back as written
    bool lux_valid;
    bool saturated;
    bool too_dark;
    uint8_t configuration; // as written: VEML6040_CONF_*
    uint16_t green;        // count, as last read
    uint16_t lux_x10;      // 0.1 lux (lux_valid), or full scale when saturated
    uint32_t read_ms;      // the millisecond clock when that read finished
    Veml6040Request request;
    I2cResult last_result;
    uint32_t reads;
    uint32_t writes;
    uint32_t mismatches; // configuration read back different (reset?)
    uint32_t failures;
} Veml6040Status;

// Configures the sensor (if it answers). After i2c1_init().
void veml6040_init(void);

// Every pass of the main loop: finishes requests and starts the next.
// Returns true when a read has just finished.
bool veml6040_service(uint32_t now_ms);

// Periodic reads on or off (on by default): off while another user of the
// bus needs it to itself (the RTC source).
void veml6040_set_auto_read(bool enabled);

const Veml6040Status *veml6040_status(void);
// One line: the light, the raw count, and the counts of requests.
void veml6040_print(uint32_t now_ms);

#endif // VEML6040_H
