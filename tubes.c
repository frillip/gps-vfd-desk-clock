#include "tubes.h"

uint32_t characters[] = {DIGIT_0, DIGIT_1, DIGIT_2, DIGIT_3, DIGIT_4, DIGIT_5, DIGIT_6, DIGIT_7, DIGIT_8, DIGIT_9, DIGIT_A, DIGIT_B, DIGIT_C, DIGIT_D, DIGIT_E, DIGIT_F};
uint32_t digits[] = {0,0,0,0};
uint32_t segments[] = {0,0,0,0};

void display_init(void)
{
    uint32_t driver_buffer = 0x00000000; // Set a blank buffer
    BLANK_SetHigh(); // Disable the blanking function
    POL_SetHigh(); // Set normal polarity
    display_buffer(driver_buffer); // Load buffer into the driver
}

// Show a counter on the display
void display_count(uint16_t count)
{
    uint32_t driver_buffer = 0x00000000; // Start with an empty buffer
    digits[3] = count / 1000;
    digits[2] = (count%1000) / 100;
    digits[1] = (count%100) / 10;
    digits[0] = count%10; // Break the counter down into individual digits
    segments[3] = characters[digits[3]];
    segments[2] = characters[digits[2]];
    segments[1] = characters[digits[1]];
    segments[0] = characters[digits[0]]; // Determine the segments needed for each digit
    // OR the segments into the buffer at the required offsets
    driver_buffer = (segments[0]<<21) | (segments[1]<<14) | (segments[2]<<7) | (segments[3]);
    // Toggle both the dots/dashes based on if the counter is even or odd
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
    display_buffer(driver_buffer); // Load buffer into the driver
}

void display_time(const time_t *tod)
{
    uint32_t driver_buffer = 0x00000000; // Start with an empty buffer
    struct tm *disp_time;
    extern bool pps_sync;
    
    // Convert our time_t into a time struct
    disp_time = gmtime(tod);
    
    digits[3] = disp_time->tm_hour / 10;
    digits[2] = disp_time->tm_hour % 10;
    digits[1] = disp_time->tm_min / 10;
    digits[0] = disp_time->tm_min % 10; // Display hours and minutes on the digits
    segments[3] = characters[digits[3]];
    segments[2] = characters[digits[2]];
    segments[1] = characters[digits[1]];
    segments[0] = characters[digits[0]]; // Determine the segments needed for each digit
    // OR the segments into the buffer at the required offsets
    driver_buffer = (segments[0]<<21) | (segments[1]<<14) | (segments[2]<<7) | (segments[3]);
    // Toggle the middle dots/dashes based on if the seconds are even or odd
    if(disp_time->tm_sec%2)
    {
        driver_buffer |= 0x10000000;
    }
    else
    {
        driver_buffer |= 0x20000000;
    }
    // Show the left hand dot if we have PPS sync
    if(pps_sync) driver_buffer |= 0x80000000;
    // Placeholder for left hand dash condition
    //if(somethingselse) driver_buffer |= 0x40000000;
    display_buffer(driver_buffer); // Load buffer into the driver
}

void display_mmss(const time_t *tod)
{
    uint32_t driver_buffer = 0x00000000; // Start with an empty buffer
    struct tm *disp_time;
    extern bool pps_sync;
    
    // Convert our time_t into a time struct
    disp_time = gmtime(tod);
    
    digits[3] = disp_time->tm_min / 10;
    digits[2] = disp_time->tm_min % 10;
    digits[1] = disp_time->tm_sec / 10;
    digits[0] = disp_time->tm_sec % 10; // Display minutes and seconds on the digits
    segments[3] = characters[digits[3]];
    segments[2] = characters[digits[2]];
    segments[1] = characters[digits[1]];
    segments[0] = characters[digits[0]]; // Determine the segments needed for each digit
    // OR the segments into the buffer at the required offsets
    driver_buffer = (segments[0]<<21) | (segments[1]<<14) | (segments[2]<<7) | (segments[3]);
    // Toggle the middle dots/dashes based on if the seconds are even or odd
    if(disp_time->tm_sec%2)
    {
        driver_buffer |= 0x10000000;
    }
    else
    {
        driver_buffer |= 0x20000000;
    }
    // Show the left hand dot if we have PPS sync
    if(pps_sync) driver_buffer |= 0x80000000;
    // Placeholder for left hand dash condition
    //if(somethingselse) driver_buffer |= 0x40000000;
    display_buffer(driver_buffer); // Load buffer into the driver
}

// Send our 32bit buffer over SPI as 2x 16bits
void display_buffer(uint32_t buffer)
{
    SPI2_Exchange16bit((uint16_t)((buffer>>16)&0xFFFF));
    SPI2_Exchange16bit((uint16_t)(buffer&0xFFFF));
}

// Manual display latch, not required with OC running
void display_latch(void)
{
    LATCH_SetHigh();
    DELAY_microseconds(5);
    LATCH_SetLow();
}