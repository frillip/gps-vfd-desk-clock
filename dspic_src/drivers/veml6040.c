#include "veml6040.h"

#include <stdio.h>

#define REGISTER_COUNT 2U

uint32_t veml6040_int_sens[6] = {VEML6040_SENS_40MS, VEML6040_SENS_80MS, VEML6040_SENS_160MS, VEML6040_SENS_320MS, VEML6040_SENS_640MS, VEML6040_SENS_1280MS};

static Veml6040Status status;
static uint32_t last_attempt_ms;
static bool print_next; // report the next completed read on the console
static bool auto_read = true;

static uint8_t read_address = VEML6040_CMD_G_DATA;
static uint8_t write_buffer[3];
static I2cTransfer read_transfer =
    I2C_TRANSFER(VEML6040_ADDRESS, &read_address, 1U, status.registers, REGISTER_COUNT);
static I2cTransfer write_transfer =
    I2C_TRANSFER(VEML6040_ADDRESS, write_buffer, sizeof(write_buffer), 0, 0U);

static void decode(void)
{
    const uint8_t *r = status.registers;
    //status.r_data = (r[VEML6040_CMD_R_DATA - read_address] & 0xFF);
    //status.r_data += (uint16_t)(r[(VEML6040_CMD_R_DATA + 1) - read_address] & 0xFF) <<8;
    
    status.g_data = (r[VEML6040_CMD_G_DATA - read_address] & 0xFF);
    status.g_data += (uint16_t)(r[(VEML6040_CMD_G_DATA + 1) - read_address] & 0xFF) <<8;
    
    //status.b_data = (r[VEML6040_CMD_B_DATA - read_address] & 0xFF);
    //status.b_data += (uint16_t)(r[(VEML6040_CMD_B_DATA + 1) - read_address] & 0xFF) <<8;
    
    //status.w_data = (r[VEML6040_CMD_W_DATA - read_address] & 0xFF);
    //status.w_data += (uint16_t)(r[(VEML6040_CMD_W_DATA + 1) - read_address] & 0xFF) <<8;

    uint32_t lux = 0;
    status.saturated = 0;
    status.too_dark = 0;
    status.lux_valid = 0;
    
    uint32_t sensitivity = veml6040_int_sens[status.integration_time >> 4];
    lux = (((uint32_t)status.g_data * sensitivity) + (VEML6040_SENS_SCALAR_X10 / 2)) / VEML6040_SENS_SCALAR_X10;
    if(lux > 65535)
    {
        lux = 65535;
        status.saturated = 1;
    }
    else if(lux < 1)
    {
        status.too_dark = 1;
    }
    else status.lux_valid = 1;
    status.lux_x10 = lux;
}

static void encode(void)
{
    uint8_t *r = &write_buffer[1];
    
    write_buffer[0] = VEML6040_CMD_CONF;
    r[VEML6040_CMD_CONF] = status.chip_shutdown |
                            status.manual_mode  |
                            status.trigger_mode |
                            status.integration_time;
    r[VEML6040_CMD_CONF + 1U] = 0x00;
}

static bool start(I2cTransfer *transfer, Veml6040Request request)
{
    if (!i2c1_submit(transfer))
    {
        return false;
    }
    status.request = request;
    return true;
}

bool veml6040_init(void)
{
    status.chip_shutdown = VEML6040_CONF_SD_ENABLE;
    status.manual_mode = VEML6040_CONF_AF_AUTO;
    status.trigger_mode = VEML6040_CONF_TRIG_DISABLE;
    status.integration_time = VEML6040_CONF_IT_320MS;
    
    encode();
    if (!start(&write_transfer, VEML6040_WRITING))
    {
        return false;
    }
    print_next = true;
    return true;
}

bool veml6040_busy(void)
{
    return status.request == VEML6040_READING || status.request == VEML6040_WRITING;
}


bool veml6040_read(void)
{
    if (veml6040_busy())
    {
        return false;
    }
    return start(&read_transfer, VEML6040_READING);
}

bool veml6040_write(void)
{
    if (veml6040_busy())
    {
        return false;
    }
    encode();
    if (!start(&write_transfer, VEML6040_WRITING))
    {
        return false;
    }
    print_next = true;
    return true;
}

// A transfer ended: true if it was the read.
static bool finish(const I2cTransfer *transfer, uint32_t now_ms)
{
    I2cResult result = transfer->result;

    status.last_result = result;
    last_attempt_ms = now_ms;
    if (result == I2C_NACK_ADDRESS)
    {
        status.present = false;
        status.lux_valid = false;
    }
    else if (result == I2C_OK || result == I2C_NACK_DATA)
    {
        status.present = true;
    }
    if (result != I2C_OK)
    {
        status.failures++;
        status.request = VEML6040_FAILED;
        return transfer == &read_transfer;
    }
    if (transfer == &write_transfer)
    {
        status.writes++;
        // Read back what the chip now holds.
        status.request = VEML6040_DONE;
        if (!start(&read_transfer, VEML6040_READING))
        {
            status.request = VEML6040_FAILED;
        }
        return false;
    }
    status.reads++;
    status.read_ms = now_ms;
    status.read_stamp = read_transfer.start_stamp;
    status.request = VEML6040_DONE;
    decode();
    return true;
}

bool veml6040_service(uint32_t now_ms)
{
    bool read_done = false;

    if (status.request == VEML6040_READING && i2c1_done(&read_transfer))
    {
        read_done = finish(&read_transfer, now_ms);
    }
    else if (status.request == VEML6040_WRITING &&
             i2c1_done(&write_transfer))
    {
        read_done = finish(&write_transfer, now_ms);
    }
    if (read_done && print_next)
    {
        print_next = false;
        veml6040_print(now_ms);
    }
    if (auto_read && !veml6040_busy() &&
        now_ms - last_attempt_ms >=
            (status.present ? VEML6040_READ_INTERVAL_MS : VEML6040_DETECT_INTERVAL_MS))
    {
        last_attempt_ms = now_ms;
        veml6040_read();
    }
    return read_done;
}


void veml6040_set_auto_read(bool enabled)
{
    auto_read = enabled;
}


const Veml6040Status *veml6040_status(void)
{
    return &status;
}

const uint16_t veml6040_lux_x10(void)
{
    return status.lux_x10;
}

void veml6040_print(uint32_t now_ms)
{
    printf("ALS: VEML6040 ");
    if (!status.present)
    {
        printf("%s\n", status.request == VEML6040_NONE ? "not read yet" : "not found");
        return;
    }
    if (status.request == VEML6040_FAILED)
    {
        printf("%s, ", i2c1_result_name(status.last_result));
    }
    if (status.reads == 0U)
    {
        printf("not read yet\n");
        return;
    }
    printf("%s%s%s, ", status.lux_valid ? "" : " NOT VALID",
           status.saturated ? " (sensor saturated)" : "",
           status.too_dark ? " (too dark)" : "");
    printf("%4u.%01ulux", status.lux_x10/10, status.lux_x10%10);
    printf(" (read 0x%04X", status.g_data);
    printf(" %lu s ago)", (unsigned long)((now_ms - status.read_ms) / 1000UL));
    printf(", %lu reads, %lu writes, %lu failures\n", (unsigned long)status.reads,
           (unsigned long)status.writes, (unsigned long)status.failures);
}