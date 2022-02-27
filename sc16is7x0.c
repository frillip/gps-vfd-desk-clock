#include "sc16is7x0.h"


void sc16is7x0_init(uint32_t freq, uint32_t baud)
{
    sc16is7x0_write_reg(SC16IS7X0_REG_IOCONTROL, sc16is7x0_read_reg(SC16IS7X0_REG_IOCONTROL) | 0x08); //UART software reset
    sc16is7x0_write_reg(SC16IS7X0_REG_FCR, sc16is7x0_read_reg(SC16IS7X0_REG_FCR) | 0x07); // FIFO enable and clear

    uint16_t divisor = freq / (baud * 16); // prescaler = 1
    uint8_t lcr = sc16is7x0_read_reg(SC16IS7X0_REG_LCR);
    sc16is7x0_write_reg(SC16IS7X0_REG_LCR, lcr | 0x80); // activate divisor registers
    sc16is7x0_write_reg(SC16IS7X0_REG_DLL, (uint8_t) divisor);
    sc16is7x0_write_reg(SC16IS7X0_REG_DLH, (uint8_t) (divisor >> 8));
    sc16is7x0_write_reg(SC16IS7X0_REG_LCR, (lcr & 0xC0) | 0x03); //length 8, no parity, stop bit 1
}

bool sc16is7x0_available(void)
{
    if(sc16is7x0_read_reg(SC16IS7X0_REG_RXLVL)) return 1;
    else return 0;
}

uint8_t sc16is7x0_rx_lvl(void)
{
    uint8_t rx_lvl = sc16is7x0_read_reg(SC16IS7X0_REG_RXLVL);
    return rx_lvl;
}

uint8_t sc16is7x0_tx_lvl(void)
{
    uint8_t tx_lvl = sc16is7x0_read_reg(SC16IS7X0_REG_TXLVL);
    return tx_lvl;
}

char sc16is7x0_read(void)
{
    char c = sc16is7x0_read_reg(SC16IS7X0_REG_RHR);
    return c;
}

void sc16is7x0_write(char c)
{
    sc16is7x0_write_reg(SC16IS7X0_REG_THR, c);
}



uint8_t sc16is7x0_read_reg(uint8_t reg)
{
    I2C1_MESSAGE_STATUS status;
    uint16_t i2c_timeout = 0;
    uint8_t pdata_write = reg << 3;
    uint8_t pdata_read[1];

    I2C1_MasterWrite(&pdata_write, 1, SC16IS7X0_ADDRESS, &status);
    // at this point, your status will probably be I2C2_MESSAGE_PENDING
    while (status == I2C1_MESSAGE_PENDING); // wait for status to to change
    if (status == I2C1_MESSAGE_COMPLETE) {
        I2C1_MasterRead(&pdata_read, 1, SC16IS7X0_ADDRESS, &status);
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
            return pdata_read[0];
        }
    }
    return 0xFF;
}

bool sc16is7x0_write_reg(uint8_t reg, uint8_t val)
{
    I2C1_MESSAGE_STATUS status;
    uint16_t i2c_timeout = 0;
    uint8_t pdata_write[2];
    pdata_write[0] = reg << 3;
    pdata_write[1] = val;

    I2C1_MasterWrite(&pdata_write, 2, SC16IS7X0_ADDRESS, &status);
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

bool sc16is7x0_nread(char *buf, uint8_t n)
{
    I2C1_MESSAGE_STATUS status;
    uint16_t i2c_timeout = 0;
    uint8_t pdata_write = SC16IS7X0_REG_RHR << 3;

    I2C1_MasterWrite(&pdata_write, 1, SC16IS7X0_ADDRESS, &status);
    // at this point, your status will probably be I2C2_MESSAGE_PENDING
    while (status == I2C1_MESSAGE_PENDING); // wait for status to to change
    if (status == I2C1_MESSAGE_COMPLETE) {
        I2C1_MasterRead(buf, n, SC16IS7X0_ADDRESS, &status);
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
            return 1;
        }
    }
    return 0;
}

bool sc16is7x0_nwrite(char *buf, uint8_t n)
{
    I2C1_MESSAGE_STATUS status;
    uint16_t i2c_timeout = 0;
    uint8_t pdata_write[65];
    pdata_write[0] = SC16IS7X0_REG_THR << 3;
    memcpy(pdata_write+1, buf, n);
    printf("%u",n);

    I2C1_MasterWrite(&pdata_write, n+1, SC16IS7X0_ADDRESS, &status);
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