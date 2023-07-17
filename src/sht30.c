#include "sht30.h"

float temperature = 0;
float humidity = 0;

void sht30_start_meas(void)
{
    I2C1_MESSAGE_STATUS status;
    uint16_t i2c_timeout = 0;
    uint8_t pdata_write[2] = {0x24, 0x00}; // no clock stretching, high repeatability

    I2C1_MasterWrite(&pdata_write, 2, SHT30_ADDRESS, &status);
    // at this point, your status will probably be I2C2_MESSAGE_PENDING
    while (status == I2C1_MESSAGE_PENDING) // wait for status to to change
    {
        i2c_timeout++;
        if(i2c_timeout>40000)
        {
            status = I2C1_MESSAGE_FAIL;
            break;
        }
    }
    printf("1 %u %u",status,i2c_timeout);
}

void sht30_read_data(void)
{
    I2C1_MESSAGE_STATUS status;
    uint16_t i2c_timeout = 0;
    uint8_t pdata_read[6] = {0};// will hold 'seconds'
    I2C1_MasterRead(&pdata_read, 6, SHT30_ADDRESS, &status);
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

    temperature = sht30_convert_temp(((uint16_t) pdata_read[0] << 8) | pdata_read[1]);
    humidity = sht30_convert_humidity(((uint16_t) pdata_read[3] << 8) | pdata_read[4]);
    printf("2 %u %u",status,i2c_timeout);
}

void sht30_start_periodic_meas(void)
{
    I2C1_MESSAGE_STATUS status;
    uint16_t i2c_timeout = 0;
    uint8_t pdata_write[2] = {0x22, 0x36}; // 2mps, high repeatability

    I2C1_MasterWrite(&pdata_write, 2, SHT30_ADDRESS, &status);
    // at this point, your status will probably be I2C2_MESSAGE_PENDING
    while (status == I2C1_MESSAGE_PENDING) // wait for status to to change
    {
        i2c_timeout++;
        if(i2c_timeout>40000)
        {
            status = I2C1_MESSAGE_FAIL;
            break;
        }
    }
    printf("1 %u %u",status,i2c_timeout);
}

void sht30_read_periodic_data(void)
{
    I2C1_MESSAGE_STATUS status;
    uint16_t i2c_timeout = 0;
    uint8_t pdata_write[2] = {0xE0, 0x00}; // 0 to 'seconds' register
    uint8_t pdata_read[6] = {0}; // will hold 'seconds'

    I2C1_MasterWrite(&pdata_write, 2, SHT30_ADDRESS, &status);
    // at this point, your status will probably be I2C2_MESSAGE_PENDING
    while (status == I2C1_MESSAGE_PENDING); // wait for status to to change
    printf("2 %u %u",status,i2c_timeout);
    if (status == I2C1_MESSAGE_COMPLETE) {
        I2C1_MasterRead(&pdata_read, 6, SHT30_ADDRESS, &status);
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
    
    temperature = sht30_convert_temp(((uint16_t) pdata_read[0] << 8) | pdata_read[1]);
    humidity = sht30_convert_humidity(((uint16_t) pdata_read[3] << 8) | pdata_read[4]);
    printf("2 %u %u",status,i2c_timeout);
}

float sht30_convert_temp(uint16_t val)
{
    // Borrowed from Adafruit
    int32_t stemp = val;
    // simplified (65536 instead of 65535) integer version of:
    // temp = (stemp * 175.0f) / 65535.0f - 45.0f;
    stemp = ((4375 * stemp) >> 14) - 4500;
    return (float)stemp / 100.0f;
}

float sht30_convert_humidity(uint16_t val)
{
    uint32_t shum = val;
    // simplified (65536 instead of 65535) integer version of:
    // humidity = (shum * 100.0f) / 65535.0f;
    shum = (625 * shum) >> 12;
    return (float)shum / 100.0f;
}

void print_sht30_data(void)
{
    printf("\r\n=== SHT30 ===\r\n");
    printf("%3.2fC, %3.2f%%", temperature, humidity);
}