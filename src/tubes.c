#include "tubes.h"

uint32_t characters[] = {DIGIT_0, DIGIT_1, DIGIT_2, DIGIT_3, DIGIT_4, DIGIT_5, DIGIT_6, DIGIT_7, DIGIT_8, DIGIT_9, DIGIT_A, DIGIT_B, DIGIT_C, DIGIT_D, DIGIT_E, DIGIT_F};
uint32_t digits[] = {0,0,0,0};
uint64_t segments[] = {0,0,0,0};
bool dash_display = 0;
bool display_blinking = 0;

#define SPI2_DMA_BUFFER_LENGTH 4
uint16_t spi2_dma_buffer[SPI2_DMA_BUFFER_LENGTH];

void spi2_dma_init(void)
{
    // MSTEN Master; DISSDO disabled; PPRE 4:1; SPRE 2:1; MODE16 enabled; SMP Middle; DISSCK disabled; CKP Idle:High, Active:Low; CKE Idle to Active; SSEN disabled;
    SPI2CON1 = 0x047A;
    // SPIFSD disabled; SPIBEN disabled; FRMPOL disabled; FRMDLY disabled; FRMEN disabled; 
    SPI2CON2 = 0x0000;
    // SISEL SPI_INT_SPIRBF; SPIROV disabled; SPIEN enabled; SPISIDL disabled; 
    SPI2STAT = 0x800C;
    
    IFS2bits.SPI2IF = 0; 
    IEC2bits.SPI2IE = 0;
    
    DMA0CONbits.CHEN = 0;           // For now, disable it
    DMA0CONbits.SIZE = 0;           // Word transfers
    DMA0CONbits.DIR = 1;            // DMA -> SPI
    DMA0CONbits.HALF = 0;           // Interrupt only on completion. (Interrupt is ignored.)
    DMA0CONbits.NULLW = 0;          // Null write disabled
    DMA0CONbits.AMODE = 0b00;       // Peripheral indirect with post increment by 2 (very important)
    DMA0CONbits.MODE = 0b01;        // One-shot, no ping-pong
    DMA0REQbits.IRQSEL = 0b0100001; // SPI2 IRQ
    DMA0STAL = __builtin_dmaoffset(spi2_dma_buffer);
    DMA0PAD = (volatile unsigned int) &SPI2BUF;
    DMA0CNT = SPI2_DMA_BUFFER_LENGTH - 1;
    DMA0CONbits.CHEN = 1;
    
    DMA1CONbits.CHEN = 0;
    DMA1CONbits.SIZE=0;
    DMA1CONbits.DIR=0;
    DMA1CONbits.HALF = 0;
    DMA1CONbits.NULLW = 0;
    DMA1CONbits.AMODE=0b01;
    DMA1CONbits.MODE=0b00;
    DMA1REQbits.IRQSEL = 0b0100001; // SPI2 IRQ

    DMA1STAL = __builtin_dmaoffset(&spi2_dma_buffer);
    DMA1PAD = (volatile unsigned int) &SPI2BUF; 
    DMA1CNT = SPI2_DMA_BUFFER_LENGTH - 1;   
    DMA1CONbits.CHEN=1;
    
    //*/
    
    IFS0bits.DMA0IF = 0;
    IEC0bits.DMA0IE = 0;
    IFS0bits.DMA1IF = 0;
    IEC0bits.DMA1IE = 0;
}

void display_init(void)
{
    uint64_t driver_buffer = 0x00000000; // Set a blank buffer
    BLANK_SetLow(); // Disable the blanking function
    //spi2_dma_init();
    display_buffer(driver_buffer); // Load buffer into the driver
}

// Show a counter on the display
void display_count(uint16_t count)
{
    uint64_t driver_buffer = 0ULL; // Start with an empty buffer
    digits[3] = count / 1000;
    digits[2] = (count%1000) / 100;
    digits[1] = (count%100) / 10;
    digits[0] = count%10; // Break the counter down into individual digits
    segments[3] = characters[digits[3]];
    segments[2] = characters[digits[2]];
    segments[1] = characters[digits[1]];
    segments[0] = characters[digits[0]]; // Determine the segments needed for each digit
    // OR the segments into the buffer at the required offsets
    driver_buffer = (segments[3]<<33) | (segments[2]<<20) | (segments[1]<<13) | (segments[0]);
    /*
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
    */
    display_buffer(driver_buffer); // Load buffer into the driver
}

void display_time(const time_t *tod)
{
    uint64_t driver_buffer = 0ULL; // Start with an empty buffer
    struct tm *disp_time;
    //extern bool pps_sync;
    
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
    driver_buffer = (segments[3]<<33) | (segments[2]<<20) | (segments[1]<<13) | (segments[0]);
    /*
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
    */
    display_buffer(driver_buffer); // Load buffer into the driver
}

void display_mmss(const time_t *tod)
{
    uint64_t driver_buffer = 0ULL; // Start with an empty buffer
    struct tm *disp_time;
    //extern bool pps_sync;
    
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
    driver_buffer = (segments[3]<<33) | (segments[2]<<20) | (segments[1]<<13) | (segments[0]);
    /*
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
    */
    display_buffer(driver_buffer); // Load buffer into the driver
}

void display_dashes(void)
{
    uint64_t driver_buffer = 0x800408004; // Buffer containing only dashes
    display_buffer(driver_buffer);
}

void display_blank(void)
{
    uint64_t driver_buffer = 0x00000000; // Start with an empty buffer
    display_buffer(driver_buffer); // Load buffer into the driver
}

void display_buffer(uint64_t buffer)
{
    SPI2_Exchange16bit((uint16_t)((buffer>>24)&0xFFFF));
    SPI2_Exchange16bit((uint16_t)((buffer>>32)&0xFFFF));
    SPI2_Exchange16bit((uint16_t)((buffer>>16)&0xFFFF));
    SPI2_Exchange16bit((uint16_t)(buffer&0xFFFF));
}

/*
void display_buffer(uint64_t buffer)
{
    spi2_dma_buffer[SPI2_DMA_BUFFER_LENGTH-4] = (buffer>>48)&0xFFFF;
    spi2_dma_buffer[SPI2_DMA_BUFFER_LENGTH-3] = (buffer>>32)&0xFFFF;
    spi2_dma_buffer[SPI2_DMA_BUFFER_LENGTH-2] = (buffer>>16)&0xFFFF;
    spi2_dma_buffer[SPI2_DMA_BUFFER_LENGTH-1] = buffer&0xFFFF;
    DMA0CONbits.CHEN = 1;
    DMA0REQbits.FORCE = 1;
}
*/

// Manual display latch, not required with OC running
void display_latch(void)
{
    LATCH_GPIO_SetHigh();
    DELAY_microseconds(5);
    LATCH_GPIO_SetLow();
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
            while((last_sunday + 7) < 31) last_sunday += 7;
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
    // Return DST status
    return dst;
}

// Set up timer 3 to blink the display every 0.5s
void display_blink_start(uint32_t fcy)
{
    // TMR3 starts at 0
    TMR3 = 0x00;
    // Period ~0.5 s, calculated from fcy / 4
    PR3 = (uint16_t)(fcy >> 11)-1;
    // TCKPS 1:256; T32 16 Bit; TON enabled; TSIDL disabled; TCS FOSC/2; TGATE disabled
    T3CON = 0x8030;
    // Set the display blinking flag
    display_blinking = 1;
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
    // Set the display blinking flag
    display_blinking = 0;
}

uint8_t t3_counter = 0;

void __attribute__ ( ( interrupt, no_auto_psv ) ) _T3Interrupt (  )
{
    t3_counter++;
    if(t3_counter==4)
    {
        t3_counter=0;
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
    }
    // Clear the interrupt flag
    IFS0bits.T3IF = false;
}

void __attribute__((__interrupt__,no_auto_psv)) _SPI2Interrupt(void)
{
    IFS2bits.SPI2IF = 0;
}

void __attribute__((__interrupt__,no_auto_psv)) _DMA0Interrupt(void)
{
    IFS0bits.DMA0IF = 0;
}

void __attribute__((__interrupt__,no_auto_psv)) _DMA1Interrupt(void)
{
    IFS0bits.DMA1IF = 0;
}