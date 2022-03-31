#include "tubes.h"
#include <stdio.h>

uint32_t characters[] = {DIGIT_0, DIGIT_1, DIGIT_2, DIGIT_3, DIGIT_4, DIGIT_5, DIGIT_6, DIGIT_7, DIGIT_8, DIGIT_9, DIGIT_A, DIGIT_B, DIGIT_C, DIGIT_D, DIGIT_E, DIGIT_F};
uint32_t digits[] = {0,0,0,0};
uint32_t segments[] = {0,0,0,0};
bool dash_display = 0;

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

void display_dashes(void)
{
    uint32_t driver_buffer = 0x50810204; // Buffer containing only dashes
    display_buffer(driver_buffer);
}

void display_blank(void)
{
    uint32_t driver_buffer = 0x00000000; // Start with an empty buffer
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

// Set up timer 3 to blink the display every 0.5s
void display_blink_start(uint32_t fcy)
{
    // TMR3 starts at 0
    TMR3 = 0x00;
    // Period ~0.5 s, calculated from fcy
    PR3 = (uint16_t)(fcy >> 9)-1;
    // TCKPS 1:256; T32 16 Bit; TON enabled; TSIDL disabled; TCS FOSC/2; TGATE disabled
    T3CON = 0x8030;
    // Clear T3 interrupt flag
    IFS0bits.T3IF = false;
    // Enable T3 interrupts
    IEC0bits.T3IE = true;
    // Set interrupt priority
    IPC2bits.T3IP = 1;
}

void display_blink_stop(void)
{
    // Turn the timer off
    T3CONbits.TON = 0;
}

// DST is stupid
// Valid for UK and EU and other places where
// DST starts on the last Sunday of March
// and ends on the last Sunday of October
// at 0100 UTC
bool isDST(const time_t *tod)
{
    struct tm *disp_time;
    
    // Convert our time_t into a time struct
    disp_time = gmtime(tod);
    uint8_t month = disp_time->tm_mon;
    uint8_t mday = disp_time->tm_mday;
    uint8_t wday = disp_time->tm_wday;
    uint8_t hour = disp_time->tm_hour;
    uint8_t last_sunday = 0;
    bool dst = 0;

    // April to October is DST
    if(disp_time->tm_mon > 2 && disp_time->tm_mon <9)
    {
        dst = 1;
    }
    // If we're in March/October
    if(disp_time->tm_mon == 2 || disp_time->tm_mon == 9)
    {
        // Calculate when the last Sunday is
        last_sunday = disp_time->tm_mday + disp_time->tm_wday;
        // Are we already past the last sunday
        if(last_sunday > 31)
        {
            dst = 1;
        }
        else
        {
            if((last_sunday + 7) < 31) last_sunday += 7;
            // If we're past that mday, we're in DST
            if(disp_time->tm_mday > last_sunday)
            {
                dst = 1;
            }
            // If today is the day
            if(disp_time->tm_mday == last_sunday)
            {
                // Apply DST after 0100 UTC
                if(disp_time->tm_hour >= 1)
                {
                    dst = 1;
                }
            }
        }
    }
    
    printf("%h %h %h %h\r\n", month, mday, wday, hour);
    // Return DST status
    if(dst) return 1;
    else return 0;
}

void __attribute__ ( ( interrupt, no_auto_psv ) ) _T3Interrupt (  )
{
    printf(".");
    if(dash_display)
    {
        display_dashes();
        display_latch();
        dash_display = 0;
    }
    else
    {
        display_blank();
        display_latch();
        dash_display = 1;
    }
    // Clear the interrupt flag
    IFS0bits.T3IF = false;
}