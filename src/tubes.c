#include "tubes.h"

uint32_t characters[] = {DIGIT_0, DIGIT_1, DIGIT_2, DIGIT_3, DIGIT_4, DIGIT_5, DIGIT_6, DIGIT_7, DIGIT_8, DIGIT_9, DIGIT_A, DIGIT_B, DIGIT_C, DIGIT_D, DIGIT_E, DIGIT_F};
bool dash_display = 0;
bool display_blinking = 0;
bool display_update_pending = 0;
uint16_t display_brightness = DISPLAY_BRIGHTNESS_DEFAULT;
bool display_brightness_oc_running = 0;

extern bool pps_sync;

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

void OC3_Initialize(void)
{
    OC3R = display_brightness;
    OC3RS = DISPLAY_BRIGHTNESS_PR;
    // Peripheral clock, Double compare continuous
    OC3CON1 = 0x1C0D;
    // 16-bit, inverted, self-sync
    //OC3CON2 = 0x101F;
    OC3CON2 = 0x1F;
    display_brightness_oc_running = 1;
}

void display_init(void)
{
    uint64_t driver_buffer = 0x00000000; // Set a blank buffer
    display_brightness_set(DISPLAY_BRIGHTNESS_DEFAULT);
    OC3_Initialize();
    //BLANK_SetLow(); // Disable the blanking function
    //spi2_dma_init();
    display_send_buffer(driver_buffer); // Load buffer into the driver
    display_latch();
}

void display_brightness_off(void)
{
    OC3CON1bits.OCM = 0b000;
    BLANK_SetHigh();
    display_brightness = 0;
    display_brightness_oc_running = 0;
}

void display_brightness_min(void)
{
    display_brightness_set(DISPLAY_BRIGHTNESS_MIN);
}

void display_brightness_max(void)
{
    display_brightness_set(DISPLAY_BRIGHTNESS_MAX);
}

void display_brightness_set(uint16_t brightness)
{
    if(brightness>DISPLAY_BRIGHTNESS_MAX) brightness = DISPLAY_BRIGHTNESS_MAX;
    if(brightness<DISPLAY_BRIGHTNESS_MIN) brightness = DISPLAY_BRIGHTNESS_MIN;
    OC3R = brightness;
    display_brightness = brightness;
    if(!display_brightness_oc_running) OC3_Initialize();
}

void display_brightness_on(void)
{
    OC3CON1bits.OCM = 0b000;
    BLANK_SetLow();
    display_brightness = 4000;
    display_brightness_oc_running = 0;
}

// Show a counter on the display
void display_count(uint16_t count)
{
    uint16_t display_digits = 0;
    
    // Construct the counter in BCD
    display_digits |= (count / 1000)<<12;
    display_digits |= ((count%1000) / 100)<<8;
    display_digits |= ((count%100) / 10)<<4;
    display_digits |= (count%10); // Break the counter down into individual digits
    
    // Generate the buffer content
    uint64_t driver_buffer = display_generate_buffer(display_digits);
    
    // Toggle both the dots/dashes based on if the counter is even or odd
    if(!(count&0x01))
    {
        driver_buffer |= START_SEPARATOR_DOT;
        driver_buffer |= MIDDLE_SEPARATOR_DOT;
    }
    else
    {
        driver_buffer |= START_SEPARATOR_LINE;
        driver_buffer |= MIDDLE_SEPARATOR_LINE;
    }
    
    display_send_buffer(driver_buffer); // Load buffer into the driver
}

void display_time(const time_t *time)
{
    uint16_t display_digits = 0;
    struct tm *disp_time;
    
    // Convert our time_t into a time struct
    disp_time = gmtime(time);
    
    // Construct our BCD time
    display_digits |= (disp_time->tm_hour / 10)<<12;
    display_digits |= (disp_time->tm_hour % 10)<<8;
    display_digits |= (disp_time->tm_min / 10)<<4;
    display_digits |= (disp_time->tm_min % 10);
    
    // Generate the buffer content
    uint64_t driver_buffer = display_generate_buffer(display_digits);

    // Toggle the middle dots/dashes based on if the seconds are even or odd
    // but only if we have PPS sync
    if(!(disp_time->tm_sec%2) && pps_sync)
    {
        driver_buffer |= MIDDLE_SEPARATOR_BOTH;
    }

    // Show the left hand dot if switch is closed
    if(ui_switch_state()) driver_buffer |= START_SEPARATOR_DOT;
    
    // Show the left hand dot if button is pressed
    if(ui_button_state()) driver_buffer |= START_SEPARATOR_LINE;

    display_send_buffer(driver_buffer); // Load buffer into the driver
}

void display_mmss(const time_t *mmss)
{
    uint16_t display_digits = 0;
    struct tm *disp_time;
    
    // Convert our time_t into a time struct
    disp_time = gmtime(mmss);
    
    // Construct our BCD time
    display_digits |= (disp_time->tm_min / 10)<<12;
    display_digits |= (disp_time->tm_min % 10)<<8;
    display_digits |= (disp_time->tm_sec / 10)<<4;
    display_digits |= (disp_time->tm_sec % 10);

    // Generate the buffer content
    uint64_t driver_buffer = display_generate_buffer(display_digits);
    
    // Toggle the middle dots/dashes based on if the seconds are even or odd
    // but only if we have PPS sync
    if(!(disp_time->tm_sec%2) && pps_sync)
    {
        driver_buffer |= MIDDLE_SEPARATOR_BOTH;
    }
    // Show the left hand dot if switch is closed
    if(ui_switch_state()) driver_buffer |= START_SEPARATOR_DOT;
    
    // Show the left hand dot if button is pressed
    if(ui_button_state()) driver_buffer |= START_SEPARATOR_LINE;

    display_send_buffer(driver_buffer); // Load buffer into the driver
}

void display_dashes(void)
{
    // Create buffer with only dashes
    uint64_t driver_buffer = 0ULL;
    driver_buffer |= (DIGIT_DASH << TUBE_4_OFFSET);
    driver_buffer |= (DIGIT_DASH << TUBE_3_OFFSET);
    driver_buffer |= (DIGIT_DASH << TUBE_2_OFFSET);
    driver_buffer |= (DIGIT_DASH << TUBE_1_OFFSET);
    driver_buffer |= START_SEPARATOR_LINE;
    driver_buffer |= MIDDLE_SEPARATOR_LINE;
    display_send_buffer(driver_buffer);
}

void display_blank(void)
{
    uint64_t driver_buffer = 0x00000000; // Start with an empty buffer
    display_send_buffer(driver_buffer); // Load buffer into the driver
}

uint64_t display_generate_buffer(uint16_t digits)
{
    uint64_t driver_buffer = 0ULL; // Start with an empty buffer
    uint64_t segments[] = {0,0,0,0};
    
    segments[3] = characters[(digits & 0xF000)>>12];
    segments[2] = characters[(digits & 0x0F00)>>8];
    segments[1] = characters[(digits & 0x00F0)>>4];
    segments[0] = characters[(digits & 0x000F)]; // Determine the segments needed for each digit
    
    // OR the segments into the buffer at the required offsets
    driver_buffer |= (segments[3] << TUBE_4_OFFSET);
    driver_buffer |= (segments[2] << TUBE_3_OFFSET);
    driver_buffer |= (segments[1] << TUBE_2_OFFSET);
    driver_buffer |= (segments[0] << TUBE_1_OFFSET);
    
    return driver_buffer;
}

void display_send_buffer(uint64_t buffer)
{
    SPI2_Exchange16bit((uint16_t)((buffer>>24)&0xFFFF));
    SPI2_Exchange16bit((uint16_t)((buffer>>32)&0xFFFF));
    SPI2_Exchange16bit((uint16_t)((buffer>>16)&0xFFFF));
    SPI2_Exchange16bit((uint16_t)(buffer&0xFFFF));
    display_update_pending = 1;
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
    display_update_pending = 0;
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
