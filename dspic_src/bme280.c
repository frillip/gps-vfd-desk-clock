#include "bme280.h"

bool bme280_detected = 0;
struct bme280_cal_data calibration_data;
struct bme280_data bme280_data;
struct bme280_uncomp_data bme280_uncomp_data;
int32_t bme280_temperature = 0;
uint32_t bme280_pressure = 0;
uint32_t bme280_humidity = 0;

bool BME280_init(void)
{
    if(BME280_read_id() == BME280_CHIP_ID)
    {
        return BME280_read_cal();
    }
    return 0;
}

uint8_t BME280_read_id(void)
{
    uint8_t id = 0;
    
    I2C1_MESSAGE_STATUS status;
    uint16_t i2c_timeout = 0;
    uint8_t pdata_write[BME280_LEN_CMD] = {BME280_REG_ID};    // the register we want
    uint8_t pdata_read[BME280_LEN_CMD] = {0};                 // return 1 byte of data
    I2C1_MasterWrite(pdata_write, BME280_LEN_CMD, BME280_ADDR, &status);
    // at this point, your status will probably be I2C2_MESSAGE_PENDING
    while (status == I2C1_MESSAGE_PENDING); // wait for status to to change
    if (status == I2C1_MESSAGE_COMPLETE) {
        I2C1_MasterRead(pdata_read, 1, BME280_ADDR, &status);
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
    
    id = pdata_read[0];
    
    return id;
}

void BME280_read_temp(void)
{
    
}

int32_t BME280_comp_temp(uint32_t uncomp_temp)
{
    int32_t var1;
    int32_t var2;
    int32_t temperature;
    int32_t temperature_min = -4000;
    int32_t temperature_max = 8500;

    var1 = (int32_t)((uncomp_temp / 8) - ((int32_t)calibration_data.dig_t1 * 2));
    var1 = (var1 * ((int32_t)calibration_data.dig_t2)) / 2048;
    var2 = (int32_t)((uncomp_temp / 16) - ((int32_t)calibration_data.dig_t1));
    var2 = (((var2 * var2) / 4096) * ((int32_t)calibration_data.dig_t3)) / 16384;
    calibration_data.t_fine = var1 + var2;
    temperature = (calibration_data.t_fine * 5 + 128) / 256;

    if (temperature < temperature_min)
    {
        temperature = temperature_min;
    }
    else if (temperature > temperature_max)
    {
        temperature = temperature_max;
    }

    return temperature;
}

void BME280_read_pres(void)
{
    
}

uint32_t BME280_comp_pres(uint32_t uncomp_pres)
{
    int64_t var1;
    int64_t var2;
    int64_t var3;
    int64_t var4;
    uint32_t pressure;
    uint32_t pressure_min = 3000000;
    uint32_t pressure_max = 11000000;

    var1 = ((int64_t)calibration_data.t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)calibration_data.dig_p6;
    var2 = var2 + ((var1 * (int64_t)calibration_data.dig_p5) * 131072);
    var2 = var2 + (((int64_t)calibration_data.dig_p4) * 34359738368);
    var1 = ((var1 * var1 * (int64_t)calibration_data.dig_p3) / 256) + ((var1 * ((int64_t)calibration_data.dig_p2) * 4096));
    var3 = ((int64_t)1) * 140737488355328;
    var1 = (var3 + var1) * ((int64_t)calibration_data.dig_p1) / 8589934592;

    /* To avoid divide by zero exception */
    if (var1 != 0)
    {
        var4 = 1048576 - uncomp_pres;
        var4 = (((var4 * (2147483648LL)) - var2) * 3125) / var1;
        var1 = (((int64_t)calibration_data.dig_p9) * (var4 / 8192) * (var4 / 8192)) / 33554432;
        var2 = (((int64_t)calibration_data.dig_p8) * var4) / 524288;
        var4 = ((var4 + var1 + var2) / 256) + (((int64_t)calibration_data.dig_p7) * 16);
        pressure = (uint32_t)(((var4 / 2) * 100) / 128);

        if (pressure < pressure_min)
        {
            pressure = pressure_min;
        }
        else if (pressure > pressure_max)
        {
            pressure = pressure_max;
        }
    }
    else
    {
        pressure = pressure_min;
    }

    return pressure;
}

void BME280_read_hum(void)
{
    
}

uint32_t BME280_comp_hum(uint32_t uncomp_hum)
{
    int32_t var1;
    int32_t var2;
    int32_t var3;
    int32_t var4;
    int32_t var5;
    uint32_t humidity;
    uint32_t humidity_max = 102400;

    var1 = calibration_data.t_fine - ((int32_t)76800);
    var2 = (int32_t)(uncomp_hum * 16384);
    var3 = (int32_t)(((int32_t)calibration_data.dig_h4) * 1048576);
    var4 = ((int32_t)calibration_data.dig_h5) * var1;
    var5 = (((var2 - var3) - var4) + (int32_t)16384) / 32768;
    var2 = (var1 * ((int32_t)calibration_data.dig_h6)) / 1024;
    var3 = (var1 * ((int32_t)calibration_data.dig_h3)) / 2048;
    var4 = ((var2 * (var3 + (int32_t)32768)) / 1024) + (int32_t)2097152;
    var2 = ((var4 * ((int32_t)calibration_data.dig_h2)) + 8192) / 16384;
    var3 = var5 * var2;
    var4 = ((var3 / 32768) * (var3 / 32768)) / 128;
    var5 = var3 - ((var4 * ((int32_t)calibration_data.dig_h1)) / 16);
    var5 = (var5 < 0 ? 0 : var5);
    var5 = (var5 > 419430400 ? 419430400 : var5);
    humidity = (uint32_t)(var5 / 4096);

    if (humidity > humidity_max)
    {
        humidity = humidity_max;
    }

    return humidity;
}

void BME280_read_all(void)
{
    I2C1_MESSAGE_STATUS status;
    uint16_t i2c_timeout = 0;
    uint8_t pdata_write[BME280_LEN_CMD] = {BME280_REG_PRES};  // the register we want
    uint8_t pdata_read[BME280_LEN_P_T_H_DATA];
    I2C1_MasterWrite(pdata_write, BME280_LEN_CMD, BME280_ADDR, &status);
    // at this point, your status will probably be I2C2_MESSAGE_PENDING
    while (status == I2C1_MESSAGE_PENDING);         // wait for status to to change
    if (status == I2C1_MESSAGE_COMPLETE) {
        I2C1_MasterRead(pdata_read, BME280_LEN_P_T_H_DATA, BME280_ADDR, &status);
        while (status == I2C1_MESSAGE_PENDING)
        {
            i2c_timeout++;
            if(i2c_timeout>40000)
            {
                status = I2C1_MESSAGE_FAIL;
                break;
            }
        }
        if (status == I2C1_MESSAGE_COMPLETE)
        {
            uint32_t data_xlsb;
            uint32_t data_lsb;
            uint32_t data_msb;

            /* Store the parsed register values for temperature data */
            data_msb = (uint32_t)pdata_read[3] << 12;
            data_lsb = (uint32_t)pdata_read[4] << 4;
            data_xlsb = (uint32_t)pdata_read[5] >> 4;
            bme280_uncomp_data.temperature = data_msb | data_lsb | data_xlsb;
            bme280_data.temperature = BME280_comp_temp(bme280_uncomp_data.temperature);
            bme280_temperature = bme280_data.temperature + BME280_TEMP_OFFSET;
            printf("%li.%lidegC\r\n", bme280_temperature/100, bme280_temperature%100);
            
            /* Store the parsed register values for pressure data */
            data_msb = (uint32_t)pdata_read[0] << 12;
            data_lsb = (uint32_t)pdata_read[1] << 4;
            data_xlsb = (uint32_t)pdata_read[2] >> 4;
            bme280_uncomp_data.pressure = data_msb | data_lsb | data_xlsb;
            bme280_data.pressure = BME280_comp_pres(bme280_uncomp_data.pressure);
            bme280_pressure = bme280_data.pressure / 100;
            printf("%lu.%lumBar\r\n", bme280_pressure/100, bme280_pressure%100);

            /* Store the parsed register values for humidity data */
            data_msb = (uint32_t)pdata_read[6] << 8;
            data_lsb = (uint32_t)pdata_read[7];
            bme280_uncomp_data.humidity = data_msb | data_lsb;
            bme280_data.humidity = BME280_comp_hum(bme280_uncomp_data.humidity);
            bme280_humidity = (bme280_data.humidity>>10)*100;
            bme280_humidity += ((bme280_data.humidity&0x3FF)*100)>>10;
            printf("%lu.%lu%%\r\n", bme280_humidity/100, bme280_humidity%100);
        }
    }
}

void BME280_read_settings(void)
{
    I2C1_MESSAGE_STATUS status;
    uint16_t i2c_timeout = 0;
    uint8_t pdata_write[BME280_LEN_CMD] = {BME280_REG_CTRL_HUM};  // the register we want
    uint8_t pdata_read[BME280_LEN_ALL_CONFIG];                          // return 4 bytes of data
    I2C1_MasterWrite(pdata_write, BME280_LEN_CMD, BME280_ADDR, &status);
    // at this point, your status will probably be I2C2_MESSAGE_PENDING
    while (status == I2C1_MESSAGE_PENDING);         // wait for status to to change
    if (status == I2C1_MESSAGE_COMPLETE) {
        I2C1_MasterRead(pdata_read, BME280_LEN_ALL_CONFIG, BME280_ADDR, &status);
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
}

bool BME280_read_cal(void)
{
    bool res = 0;
    res = BME280_read_cal_00();
    if(res) res = BME280_read_cal_26();
    if(res)
    {
        printf("read BME cal data\r\n");
        return 1;
    }
    return 0;
}

bool BME280_read_cal_00(void)
{
    I2C1_MESSAGE_STATUS status;
    uint16_t i2c_timeout = 0;
    uint8_t pdata_write[BME280_LEN_CMD] = {BME280_REG_CAL_00_25};   // the register we want
    uint8_t pdata_read[BME280_LEN_CAL_00_25];                       // data length

    I2C1_MasterWrite(pdata_write, BME280_LEN_CMD, BME280_ADDR, &status);
    // at this point, your status will probably be I2C2_MESSAGE_PENDING
    while (status == I2C1_MESSAGE_PENDING);                         // wait for status to to change
    if (status == I2C1_MESSAGE_COMPLETE) {
        I2C1_MasterRead(pdata_read, BME280_LEN_CAL_00_25, BME280_ADDR, &status);
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
            calibration_data.dig_t1 = BME280_make16le(pdata_read[1], pdata_read[0]);
            calibration_data.dig_t2 = (int16_t)BME280_make16le(pdata_read[3], pdata_read[2]);
            calibration_data.dig_t3 = (int16_t)BME280_make16le(pdata_read[5], pdata_read[4]);
            calibration_data.dig_p1 = BME280_make16le(pdata_read[7], pdata_read[6]);
            calibration_data.dig_p2 = (int16_t)BME280_make16le(pdata_read[9], pdata_read[8]);
            calibration_data.dig_p3 = (int16_t)BME280_make16le(pdata_read[11], pdata_read[10]);
            calibration_data.dig_p4 = (int16_t)BME280_make16le(pdata_read[13], pdata_read[12]);
            calibration_data.dig_p5 = (int16_t)BME280_make16le(pdata_read[15], pdata_read[14]);
            calibration_data.dig_p6 = (int16_t)BME280_make16le(pdata_read[17], pdata_read[16]);
            calibration_data.dig_p7 = (int16_t)BME280_make16le(pdata_read[19], pdata_read[18]);
            calibration_data.dig_p8 = (int16_t)BME280_make16le(pdata_read[21], pdata_read[20]);
            calibration_data.dig_p9 = (int16_t)BME280_make16le(pdata_read[23], pdata_read[22]);
            calibration_data.dig_h1 = pdata_read[25];
            return 1;
        }
    }
    return 0;
}

bool BME280_read_cal_26(void)
{
    I2C1_MESSAGE_STATUS status;
    uint16_t i2c_timeout = 0;
    uint8_t pdata_write[BME280_LEN_CMD] = {BME280_REG_CAL_26_41};   // the register we want
    uint8_t pdata_read[BME280_LEN_CAL_26_41];                       // data length

    I2C1_MasterWrite(pdata_write, BME280_LEN_CMD, BME280_ADDR, &status);
    // at this point, your status will probably be I2C2_MESSAGE_PENDING
    while (status == I2C1_MESSAGE_PENDING);                         // wait for status to to change
    if (status == I2C1_MESSAGE_COMPLETE) {
        I2C1_MasterRead(pdata_read, BME280_LEN_CAL_26_41, BME280_ADDR, &status);
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
            int16_t dig_h4_lsb;
            int16_t dig_h4_msb;
            int16_t dig_h5_lsb;
            int16_t dig_h5_msb;

            calibration_data.dig_h2 = (int16_t)BME280_make16le(pdata_read[1], pdata_read[0]);
            calibration_data.dig_h3 = pdata_read[2];
            dig_h4_msb = (int16_t)(int8_t)pdata_read[3] * 16;
            dig_h4_lsb = (int16_t)(pdata_read[4] & 0x0F);
            calibration_data.dig_h4 = dig_h4_msb | dig_h4_lsb;
            dig_h5_msb = (int16_t)(int8_t)pdata_read[5] * 16;
            dig_h5_lsb = (int16_t)(pdata_read[4] >> 4);
            calibration_data.dig_h5 = dig_h5_msb | dig_h5_lsb;
            calibration_data.dig_h6 = (int8_t)pdata_read[6];
            return 1;
        }
    }
    return 0;
}

void BME280_write_settings(void)
{
    I2C1_MESSAGE_STATUS status;
    uint16_t i2c_timeout = 0;
    uint8_t setting_data[2] = {0};
    
    setting_data[0] = 0xF5;
    setting_data[1] = (BME280_STANDBY_62MS5<<5) | (BME280_FILTER_X16<<2);
    I2C1_MasterWrite(setting_data, 2, BME280_ADDR, &status);
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
    }
    
    DELAY_microseconds(100);
    
    i2c_timeout = 0;
    setting_data[0] = 0xF2;
    setting_data[1] = (BME280_OVERSAMPLE_X16);
    I2C1_MasterWrite(setting_data, 2, BME280_ADDR, &status);
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
    }
    
    DELAY_microseconds(100);
    
    i2c_timeout = 0;
    setting_data[0] = 0xF4;
    setting_data[1] = (BME280_OVERSAMPLE_X16<<5) | (BME280_OVERSAMPLE_X16<<2) | BME280_MODE_NORMAL;
    I2C1_MasterWrite(setting_data, 2, BME280_ADDR, &status);
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
    }
}

bool BME280_reset(void)
{
    I2C1_MESSAGE_STATUS status;
    uint16_t i2c_timeout = 0;
    uint8_t reset_data[BME280_LEN_CMD + BME280_LEN_RESET] = {BME280_REG_RESET, BME280_CMD_RESET};
    
    I2C1_MasterWrite(reset_data, BME280_LEN_CMD + BME280_LEN_RESET, BME280_ADDR, &status);
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
