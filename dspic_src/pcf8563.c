#include "pcf8563.h"

time_t PCF8563_read(void)
{
    time_t rtc;
    struct tm rtc_time;
    rtc_time.tm_isdst = 0;
    I2C1_MESSAGE_STATUS status;
    uint16_t i2c_timeout = 0;
    uint8_t pdata_write = 0x02; // 0 to 'seconds' register
    uint8_t pdata_read[7]; // will hold 'seconds'

    I2C1_MasterWrite(&pdata_write, 1, PCF8563_ADDRESS, &status);
    // at this point, your status will probably be I2C2_MESSAGE_PENDING
    while (status == I2C1_MESSAGE_PENDING); // wait for status to to change
    if (status == I2C1_MESSAGE_COMPLETE) {
        I2C1_MasterRead(&pdata_read, 7, PCF8563_ADDRESS, &status);
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
    
    rtc_time.tm_sec = PCF8563_bcd2bin(pdata_read[0])&0x7F;
    rtc_time.tm_min = PCF8563_bcd2bin(pdata_read[1]);
    rtc_time.tm_hour = PCF8563_bcd2bin(pdata_read[2]);
    rtc_time.tm_mday = PCF8563_bcd2bin(pdata_read[3]);
    rtc_time.tm_mon = PCF8563_bcd2bin(pdata_read[5]) - 1;
    rtc_time.tm_year = PCF8563_bcd2bin(pdata_read[6]) + 100;
    rtc = mktime(&rtc_time);
    return rtc;
}

bool PCF8563_write(time_t rtc)
{
    struct tm *rtc_time;
    rtc_time = gmtime(&rtc);
    I2C1_MESSAGE_STATUS status;
    uint16_t i2c_timeout = 0;
    uint8_t pdata_write[8];
    pdata_write[0] = 0x02;
    pdata_write[1] = PCF8563_bin2bcd(rtc_time->tm_sec);
    pdata_write[2] = PCF8563_bin2bcd(rtc_time->tm_min);
    pdata_write[3] = PCF8563_bin2bcd(rtc_time->tm_hour);
    pdata_write[4] = PCF8563_bin2bcd(rtc_time->tm_mday);
    pdata_write[5] = PCF8563_bin2bcd(rtc_time->tm_wday);
    pdata_write[6] = PCF8563_bin2bcd(rtc_time->tm_mon + 1);
    pdata_write[7] = PCF8563_bin2bcd(rtc_time->tm_year - 100);

    I2C1_MasterWrite(&pdata_write, 8, PCF8563_ADDRESS, &status);
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

uint8_t PCF8563_bcd2bin(uint8_t val) { return val - 6 * (val >> 4); };
uint8_t PCF8563_bin2bcd(uint8_t val) { return val + 6 * (val / 10); };
