#include "tubes.h"

uint32_t characters[] = {DIGIT_0, DIGIT_1, DIGIT_2, DIGIT_3, DIGIT_4, DIGIT_5, DIGIT_6, DIGIT_7, DIGIT_8, DIGIT_9, DIGIT_A, DIGIT_B, DIGIT_C, DIGIT_D, DIGIT_E, DIGIT_F};
uint32_t digits[] = {0,0,0,0};
uint32_t segments[] = {0,0,0,0};

void display_init(void)
{
    uint32_t driver_buffer = 0x00000000;
    BLANK_SetHigh();
    POL_SetHigh();
    display_buffer(driver_buffer);
    //display_latch();
}

void display_count(uint16_t count)
{
    uint32_t driver_buffer = 0x00000000;
    digits[3] = count / 1000;
    digits[2] = (count%1000) / 100;
    digits[1] = (count%100) / 10;
    digits[0] = count%10;
    segments[3] = characters[digits[3]];
    segments[2] = characters[digits[2]];
    segments[1] = characters[digits[1]];
    segments[0] = characters[digits[0]];
    driver_buffer = (segments[0]<<21) | (segments[1]<<14) | (segments[2]<<7) | (segments[3]);
    if(!(count&0x01))
    {
        driver_buffer |= 0x10000000;
        driver_buffer |= 0x40000000;
    }
    else
    {
        driver_buffer |= 0x20000000;
        driver_buffer |= 0x80000000;
    }
    display_buffer(driver_buffer);
}

void display_time(const time_t *tod)
{
    uint32_t driver_buffer = 0x00000000;
    struct tm *disp_time;
    extern bool pps_sync;
    
    disp_time = gmtime(tod);
    
    digits[3] = disp_time->tm_hour / 10;
    digits[2] = disp_time->tm_hour % 10;
    digits[1] = disp_time->tm_min / 10;
    digits[0] = disp_time->tm_min % 10;
    segments[3] = characters[digits[3]];
    segments[2] = characters[digits[2]];
    segments[1] = characters[digits[1]];
    segments[0] = characters[digits[0]];
    driver_buffer = (segments[0]<<21) | (segments[1]<<14) | (segments[2]<<7) | (segments[3]);
    if(disp_time->tm_sec%2)
    {
        driver_buffer |= 0x10000000;
    }
    else
    {
        driver_buffer |= 0x20000000;
    }
    if(pps_sync) driver_buffer |= 0x80000000;
    //if(somethingselse) driver_buffer |= 0x40000000;
    display_buffer(driver_buffer);
}

void display_mmss(const time_t *tod)
{
    uint32_t driver_buffer = 0x00000000;
    struct tm *disp_time;
    extern bool pps_sync;
    
    disp_time = gmtime(tod);
    
    digits[3] = disp_time->tm_min / 10;
    digits[2] = disp_time->tm_min % 10;
    digits[1] = disp_time->tm_sec / 10;
    digits[0] = disp_time->tm_sec % 10;
    segments[3] = characters[digits[3]];
    segments[2] = characters[digits[2]];
    segments[1] = characters[digits[1]];
    segments[0] = characters[digits[0]];
    driver_buffer = (segments[0]<<21) | (segments[1]<<14) | (segments[2]<<7) | (segments[3]);
    if(disp_time->tm_sec%2)
    {
        driver_buffer |= 0x10000000;
    }
    else
    {
        driver_buffer |= 0x20000000;
    }
    if(pps_sync) driver_buffer |= 0x80000000;
    display_buffer(driver_buffer);
}

void display_buffer(uint32_t buffer)
{
    SPI2_Exchange16bit((uint16_t)((buffer>>16)&0xFFFF));
    SPI2_Exchange16bit((uint16_t)(buffer&0xFFFF));
}

void display_latch(void)
{
    LATCH_SetHigh();
    DELAY_microseconds(5);
    LATCH_SetLow();
}