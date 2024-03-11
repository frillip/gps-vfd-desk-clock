#include "veml6040.h"

bool veml6040_detected = 0;
double veml_ambient_light = 0;
uint16_t veml_brightness = 0;

bool VEML6040_init(void)
{
    I2C1_MESSAGE_STATUS status;
    uint16_t i2c_timeout = 0;
    uint8_t sensor_conf[3] = {0};
    
    // CMD register
    sensor_conf[0] = VEML6040_CMD_CONF;
    // Enable sensor
    sensor_conf[1] |= VEML6040_CONF_SD_ENABLE;
    // Auto mode
    sensor_conf[1] |= VEML6040_CONF_AF_AUTO;
    // In auto mode we don't need the trigger
    sensor_conf[1] |= VEML6040_CONF_TRIG_DISABLE;
    // Use 320ms integration time
    sensor_conf[1] |= VEML6040_CONF_IT_320MS;
    // second byte of 16-bit write should be empty
    //sensor_conf[2] = 0;
    
    I2C1_MasterWrite(&sensor_conf, 2, VEML6040_ADDR, &status);
    // at this point, your status will probably be I2C2_MESSAGE_PENDING
    while (status == I2C1_MESSAGE_PENDING)
    {
        i2c_timeout++;
        if(i2c_timeout>40000)
        {
            status = I2C1_MESSAGE_FAIL;
            break;
        }
    }// wait for status to to change
    if (status == I2C1_MESSAGE_COMPLETE)
    {
        return 1;
    }
    return 0;
}

uint16_t VEML6040_get_red(void)
{
    return VEML_get_data(VEML6040_CMD_R_DATA);
}

uint16_t VEML6040_get_green(void)
{
    return VEML_get_data(VEML6040_CMD_G_DATA);
}

uint16_t VEML6040_get_blue(void)
{
    return VEML_get_data(VEML6040_CMD_B_DATA);
}

uint16_t VEML6040_get_white(void)
{
    return VEML_get_data(VEML6040_CMD_W_DATA);
}

uint16_t VEML_get_data(uint8_t reg)
{
    I2C1_MESSAGE_STATUS status;
    uint16_t i2c_timeout = 0;
    uint8_t pdata_write = reg; // the register we want
    uint8_t pdata_read[2];     // return 16 bits of data
    I2C1_MasterWrite(&pdata_write, 1, VEML6040_ADDR, &status);
    // at this point, your status will probably be I2C2_MESSAGE_PENDING
    while (status == I2C1_MESSAGE_PENDING); // wait for status to to change
    if (status == I2C1_MESSAGE_COMPLETE) {
        I2C1_MasterRead(&pdata_read, 2, VEML6040_ADDR, &status);
        while (status == I2C1_MESSAGE_PENDING)
        {
            i2c_timeout++;
            if(i2c_timeout>40000)
            {
                status = I2C1_MESSAGE_FAIL;
                break;
            }
        }
        if (status == I2C1_MESSAGE_COMPLETE) {
            // pdata_read should now be the number of seconds (in binary-coded decimal)
        }
    }
    
    uint16_t res = pdata_read[0];
    res |= (uint16_t)pdata_read[1]<<8;
    
    return res;
}

double VEML6040_get_lux(void)
{
    return VEML6040_get_green() * VEML6040_SENS_320MS;
}

uint16_t VEML_calc_brightness(double lux)
{
    uint16_t brightness = VEML_DISPLAY_BRIGHTNESS_CONST;
    brightness += (VEML_DISPLAY_BRIGHTNESS_FIRST * lux);
    brightness += (VEML_DISPLAY_BRIGHTNESS_SECOND * (lux * lux));
    return brightness;
}

void print_veml_data(void)
{
    if(veml6040_detected)
    {
        printf("\r\n=== VEML6040 LUX DATA ===\r\n");
        printf("LUX: %6.1f", veml_ambient_light);
        printf(" BRI: %4u/4000\r\n", veml_brightness);
    }
    else
    {
        printf("\r\n=== NO VEML6040 DETECTED ===\r\n");
    }
}
