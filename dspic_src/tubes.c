#include "tubes.h"

uint64_t display_last_driver_buffer = 0;
uint32_t characters[] = {DIGIT_0, DIGIT_1, DIGIT_2, DIGIT_3, DIGIT_4, DIGIT_5, DIGIT_6, DIGIT_7, DIGIT_8, DIGIT_9, DIGIT_A, DIGIT_B, DIGIT_C, DIGIT_D, DIGIT_E, DIGIT_F};
bool dash_display = 0;
bool display_blinking = 0;
bool display_update_pending = 0;
bool display_brightness_manual = 0;
uint16_t display_brightness = DISPLAY_BRIGHTNESS_DEFAULT;
uint16_t display_brightness_target = DISPLAY_BRIGHTNESS_DEFAULT;
bool display_brightness_oc_running = 0;

extern uint16_t esp_brightness;
extern bool esp_detected;

extern bool veml6040_detected;
extern uint16_t veml_ambient_light;
extern uint16_t veml_brightness;

extern bool pps_sync;

extern UI_DISPLAY_STATE ui_state_current;

extern UI_MENU_STATE ui_menu_current;

time_t display;
time_t previous_display;
int32_t tz_offset = TZ_OFFSET_DEFAULT;
int32_t dst_offset = DST_OFFSET_DEFAULT;
int32_t alarm_offset = UI_ALARM_DEFAULT;
bool dst_active = DST_ACTIVE_DEFAULT;

extern time_t utc;

extern int32_t bme280_temperature;

extern uint16_t t10ms_display;

#define SPI2_DMA_BUFFER_LENGTH 4
uint16_t spi2_dma_buffer[SPI2_DMA_BUFFER_LENGTH];

extern EEPROM_DATA_STRUCT running_data;

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
    display_brightness_target = 0;
    display_brightness_oc_running = 0;
}

void display_brightness_min(void)
{
    display_brightness_set(DISPLAY_BRIGHTNESS_MIN);
    display_brightness_set_target(DISPLAY_BRIGHTNESS_MIN);
}

void display_brightness_max(void)
{
    display_brightness_set(DISPLAY_BRIGHTNESS_MAX);
    display_brightness_set_target(DISPLAY_BRIGHTNESS_MAX);
}

void display_brightness_set(uint16_t brightness)
{
    if(brightness>DISPLAY_BRIGHTNESS_MAX) brightness = DISPLAY_BRIGHTNESS_MAX;
    if(brightness<DISPLAY_BRIGHTNESS_MIN) brightness = DISPLAY_BRIGHTNESS_MIN;
    OC3R = brightness;
    display_brightness = brightness;
    if(!display_brightness_oc_running) OC3_Initialize();
}

void display_brightness_set_manual(void)
{
    display_brightness_manual = 1;
    display_brightness = display_brightness+(DISPLAY_BRIGHTNESS_STEP/2);
    display_brightness /= DISPLAY_BRIGHTNESS_STEP;
    display_brightness *= DISPLAY_BRIGHTNESS_STEP;
    display_brightness_set(display_brightness);
}

void display_brightness_set_auto(void)
{
    display_brightness_manual = 0;
    if(veml6040_detected)
    {
        display_brightness_set_target(veml_brightness);
    }
    else display_brightness_set_target(DISPLAY_BRIGHTNESS_DEFAULT);
}

void display_brightness_set_target(uint16_t target)
{
    display_brightness_target = target+(DISPLAY_BRIGHTNESS_TARGET_STEP/2);
    display_brightness_target /= DISPLAY_BRIGHTNESS_TARGET_STEP;
    display_brightness_target *= DISPLAY_BRIGHTNESS_TARGET_STEP;
    if(display_brightness_target>DISPLAY_BRIGHTNESS_MAX) display_brightness_target = DISPLAY_BRIGHTNESS_MAX;
    if(display_brightness_target<DISPLAY_BRIGHTNESS_MIN) display_brightness_target = DISPLAY_BRIGHTNESS_MIN;
}

void display_brightness_up_step(void)
{
    display_brightness_set(display_brightness + DISPLAY_BRIGHTNESS_TARGET_STEP);
}

void display_brightness_down_step(void)
{
    display_brightness_set(display_brightness - DISPLAY_BRIGHTNESS_TARGET_STEP);
}

void display_brightness_up(uint16_t brightness_up)
{
    display_brightness_set(display_brightness + brightness_up);
}

void display_brightness_down(uint16_t brightness_down)
{
    display_brightness_set(display_brightness - brightness_down);
}

void display_brightness_on(void)
{
    OC3CON1bits.OCM = 0b000;
    BLANK_SetLow();
    display_brightness = 4000;
    display_brightness_target = 4000;
    display_brightness_oc_running = 0;
}

void display_brightness_update(void)
{
    if(!display_brightness_manual)
    {
        if(!veml6040_detected && !esp_detected)
        {
            display_brightness_set_target(DISPLAY_BRIGHTNESS_DEFAULT);
        }
        if(display_brightness_target > display_brightness)
        {
            display_brightness_up_step();
        }
        else if(display_brightness_target < display_brightness)
        {
            display_brightness_down_step();
        }
    }
}

void display_temp(int32_t temp)
{
    uint16_t display_digits = 0;
    bool temp_negative = 0;
    
    if(temp<0)
    {
        temp_negative =  1;
        temp = (temp ^ 0xFFFF) +1; // Make positive
    }
    
    // Construct the counter in BCD
    display_digits |= (temp / 1000)<<12;
    display_digits |= ((temp%1000) / 100)<<8;
    display_digits |= ((temp%100) / 10)<<4;
    display_digits |= (temp%10); // Break the counter down into individual digits
    
    // Generate the buffer content
    uint64_t driver_buffer = display_generate_buffer(display_digits);
    
    // Add decimal point
    driver_buffer |= MIDDLE_SEPARATOR_DOT;
    // Add minus if negative
    if(temp_negative) driver_buffer |= START_SEPARATOR_LINE;
    
    // Show the left hand dot if switch is closed
    if(ui_switch_state()) driver_buffer |= START_SEPARATOR_DOT;
    
    display_send_buffer(driver_buffer); // Load buffer into the driver
}

// Show a counter on the display
void display_count(int16_t count)
{
    uint16_t display_digits = 0;
    bool count_negative = 0;
    
    if(count<0)
    {
        count_negative =  1;
        count = (count ^ 0xFFFF) +1; // Make positive
    }
    
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
        driver_buffer |= MIDDLE_SEPARATOR_DOT;
    }
    else
    {
        driver_buffer |= MIDDLE_SEPARATOR_LINE;
    }
    
    if(count_negative) driver_buffer |= START_SEPARATOR_LINE;
    
    // Show the left hand dot if switch is closed
    if(ui_switch_state()) driver_buffer |= START_SEPARATOR_DOT;
    
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
    // otherwise separator on permanently
    if(pps_sync)
    {
        if(!(disp_time->tm_sec%2))
        {
            driver_buffer |= MIDDLE_SEPARATOR_BOTH;
        }
    }
    else
    {
        driver_buffer |= MIDDLE_SEPARATOR_BOTH;
    }

    // Show the left hand dot if switch is closed
    if(ui_switch_state()) driver_buffer |= START_SEPARATOR_DOT;
    
    // Show the left hand dot if button is pressed
    if(ui_button_state()) driver_buffer |= START_SEPARATOR_LINE;

    display_send_buffer(driver_buffer); // Load buffer into the driver
}

void display_mmss(const time_t *time)
{
    uint16_t display_digits = 0;
    struct tm *disp_time;
    
    // Convert our time_t into a time struct
    disp_time = gmtime(time);
    
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

void display_ssmm(const time_t *time)
{
    uint16_t display_digits = 0;
    struct tm *disp_time;
    uint8_t display_subseconds = t10ms_display;
    
    // Convert our time_t into a time struct
    disp_time = gmtime(time);
    
    // Sometimes we end up with 100 counts due to various uncertainties
    if(display_subseconds>=100)
    {
        // So bodge it
        disp_time->tm_sec++;
        display_subseconds = display_subseconds - 100;
    }
    
    // Construct our BCD time
    display_digits |= (disp_time->tm_sec / 10)<<12;
    display_digits |= (disp_time->tm_sec % 10)<<8;
    display_digits |= (display_subseconds / 10)<<4;
    display_digits |= (display_subseconds % 10);

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

void display_yyyy(const time_t *time)
{
    uint16_t display_digits = 0;
    struct tm *disp_time;
    
    // Convert our time_t into a time struct
    disp_time = gmtime(time);
    
    uint16_t year = 1900 + disp_time->tm_year; // tm_year indexed from 1900
    
    // Construct our BCD time
    display_digits |= (year / 1000)<<12;
    display_digits |= ((year / 100) % 10)<<8;
    display_digits |= ((year / 10) % 10)<<4;
    display_digits |= year % 10;

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

void display_mmdd(const time_t *time)
{
    uint16_t display_digits = 0;
    struct tm *disp_time;
    
    // Convert our time_t into a time struct
    disp_time = gmtime(time);
    
    uint16_t month = disp_time->tm_mon + 1; // tm_mon is zero indexed for no reason
    
    // Construct our BCD time
    display_digits |= (month / 10)<<12;
    display_digits |= (month % 10)<<8;
    display_digits |= (disp_time->tm_mday / 10)<<4;
    display_digits |= (disp_time->tm_mday % 10);

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

// Show a counter on the display
void display_offset(int32_t offset)
{
    uint16_t display_digits = 0;
    bool offset_negative = 0;
    
    if(offset<0)
    {
        offset_negative =  1;
        offset = (offset ^ 0xFFFFFFFF) +1; // Make positive
    }
    
    uint16_t hours = offset / 3600;
    uint16_t minutes = (offset - (3600L * hours)) / 60;
    // Construct the counter in BCD
    display_digits |= (hours / 10)<<12;
    display_digits |= (hours%10)<<8;
    display_digits |= (minutes / 10)<<4;
    display_digits |= (minutes%10); // Break the counter down into individual digits
    
    // Generate the buffer content
    uint64_t driver_buffer = display_generate_buffer(display_digits);
    
    // Enable the middle colon
    driver_buffer |= MIDDLE_SEPARATOR_DOT;
    driver_buffer |= MIDDLE_SEPARATOR_LINE;
    
    // Enable the minus if negative
    if(offset_negative) driver_buffer |= START_SEPARATOR_LINE;
    
    // Show the left hand dot if switch is closed
    // Don't do this actually
    //if(ui_switch_state()) driver_buffer |= START_SEPARATOR_DOT;
    
    display_send_buffer(driver_buffer); // Load buffer into the driver
}

void display_mask_hh(void)
{
    uint64_t driver_buffer = display_last_driver_buffer;
    driver_buffer &= DISPLAY_MASK_TUBE_3;
    driver_buffer &= DISPLAY_MASK_TUBE_4;
    display_send_buffer(driver_buffer);
}

void display_mask_mm(void)
{
    uint64_t driver_buffer = display_last_driver_buffer;
    driver_buffer &= DISPLAY_MASK_TUBE_1;
    driver_buffer &= DISPLAY_MASK_TUBE_2;
    display_send_buffer(driver_buffer);
}

extern bool ui_menu_flash_off;

void display_menu(void)
{
    switch(ui_menu_current)
    {
        case UI_MENU_STATE_TZ_SET:
            display_offset(tz_offset);
            break;
            
        case UI_MENU_STATE_TZ_SET_HH:
            display_offset(tz_offset);
            if(ui_menu_flash_off) display_mask_hh();
            break;
            
        case UI_MENU_STATE_TZ_SET_MM:
            display_offset(tz_offset);
            if(ui_menu_flash_off) display_mask_mm();
            break;

        case UI_MENU_STATE_DST_SET_OFFSET:
            if(ui_menu_flash_off) display_blank();
            else display_offset(tz_offset);
            break;

        case UI_MENU_STATE_ALARM_SET_HH:
            if(ui_menu_flash_off) display_blank();
            else display_offset(tz_offset);
            break;
            
        case UI_MENU_STATE_ALARM_SET_MM:
            if(ui_menu_flash_off) display_blank();
            else display_offset(tz_offset);
            break;

        default:
            if(ui_menu_flash_off) display_blank();
            else display_menu_text();
            break;
    }
}

void display_menu_text(void)
{
    uint64_t driver_buffer = 0ULL;
    bool no_update_req = 0;

    switch (ui_menu_current)
    {
        case UI_MENU_STATE_ROOT:
            driver_buffer = (DIGIT_M << TUBE_4_OFFSET);
            driver_buffer |= (DIGIT_E << TUBE_3_OFFSET);
            driver_buffer |= (DIGIT_N << TUBE_2_OFFSET);
            driver_buffer |= (DIGIT_U << TUBE_1_OFFSET);
            break;
            
        case UI_MENU_STATE_TZ:
            driver_buffer |= (DIGIT_NONE << TUBE_4_OFFSET);
            driver_buffer |= (DIGIT_NONE << TUBE_3_OFFSET);
            driver_buffer |= (DIGIT_T << TUBE_2_OFFSET);
            driver_buffer |= (DIGIT_Z << TUBE_1_OFFSET);
            break;

        case UI_MENU_STATE_TZ_AUTO:
        case UI_MENU_STATE_TZ_AUTO_SEL:
            if(running_data.fields.tz_data.tz_auto)
            {
                driver_buffer |= (DIGIT_A << TUBE_4_OFFSET);
                driver_buffer |= (DIGIT_U << TUBE_3_OFFSET);
                driver_buffer |= (DIGIT_T << TUBE_2_OFFSET);
                driver_buffer |= (DIGIT_O << TUBE_1_OFFSET);
            }
            else
            {
                driver_buffer |= (DIGIT_NONE << TUBE_4_OFFSET);
                driver_buffer |= (DIGIT_M << TUBE_3_OFFSET);
                driver_buffer |= (DIGIT_A << TUBE_2_OFFSET);
                driver_buffer |= (DIGIT_N << TUBE_1_OFFSET);
            }
            break;

        case UI_MENU_STATE_TZ_SET:
            no_update_req = 1;
            break;
            
        case UI_MENU_STATE_TZ_SET_HH:
            no_update_req = 1;
            break;

        case UI_MENU_STATE_TZ_SET_MM:
            no_update_req = 1;
            break;
            
        case UI_MENU_STATE_TZ_BACK:
            driver_buffer |= (DIGIT_B << TUBE_4_OFFSET);
            driver_buffer |= (DIGIT_A << TUBE_3_OFFSET);
            driver_buffer |= (DIGIT_C << TUBE_2_OFFSET);
            driver_buffer |= (DIGIT_K << TUBE_1_OFFSET);
            break;

        case UI_MENU_STATE_DST:
            driver_buffer |= (DIGIT_NONE << TUBE_4_OFFSET);
            driver_buffer |= (DIGIT_D << TUBE_3_OFFSET);
            driver_buffer |= (DIGIT_S << TUBE_2_OFFSET);
            driver_buffer |= (DIGIT_T << TUBE_1_OFFSET);
            break;

        case UI_MENU_STATE_DST_AUTO:
            if(running_data.fields.dst_data.dst_auto)
            {
                driver_buffer |= (DIGIT_A << TUBE_4_OFFSET);
                driver_buffer |= (DIGIT_U << TUBE_3_OFFSET);
                driver_buffer |= (DIGIT_T << TUBE_2_OFFSET);
                driver_buffer |= (DIGIT_O << TUBE_1_OFFSET);
            }
            else
            {
                driver_buffer |= (DIGIT_NONE << TUBE_4_OFFSET);
                driver_buffer |= (DIGIT_M << TUBE_3_OFFSET);
                driver_buffer |= (DIGIT_A << TUBE_2_OFFSET);
                driver_buffer |= (DIGIT_N << TUBE_1_OFFSET);
            }
            break;
            
        case UI_MENU_STATE_DST_SET:
            driver_buffer |= (DIGIT_NONE << TUBE_4_OFFSET);
            driver_buffer |= (DIGIT_S << TUBE_3_OFFSET);
            driver_buffer |= (DIGIT_E << TUBE_2_OFFSET);
            driver_buffer |= (DIGIT_T << TUBE_1_OFFSET);
            break;

        case UI_MENU_STATE_DST_SET_STATE:
            if(running_data.fields.dst_data.dst_active)
            {
                driver_buffer |= (DIGIT_NONE << TUBE_4_OFFSET);
                driver_buffer |= (DIGIT_NONE << TUBE_3_OFFSET);
                driver_buffer |= (DIGIT_O << TUBE_2_OFFSET);
                driver_buffer |= (DIGIT_N << TUBE_1_OFFSET);
            }
            else
            {
                driver_buffer |= (DIGIT_NONE << TUBE_4_OFFSET);
                driver_buffer |= (DIGIT_O << TUBE_3_OFFSET);
                driver_buffer |= (DIGIT_F << TUBE_2_OFFSET);
                driver_buffer |= (DIGIT_F << TUBE_1_OFFSET);
            }
            break;
            
        case UI_MENU_STATE_DST_SET_OFFSET:
            no_update_req = 1;
            break;

        case UI_MENU_STATE_DST_BACK:
            driver_buffer |= (DIGIT_B << TUBE_4_OFFSET);
            driver_buffer |= (DIGIT_A << TUBE_3_OFFSET);
            driver_buffer |= (DIGIT_C << TUBE_2_OFFSET);
            driver_buffer |= (DIGIT_K << TUBE_1_OFFSET);
            break;

        case UI_MENU_STATE_ALARM:
            driver_buffer |= (DIGIT_A << TUBE_4_OFFSET);
            driver_buffer |= (DIGIT_L << TUBE_3_OFFSET);
            driver_buffer |= (DIGIT_R << TUBE_2_OFFSET);
            driver_buffer |= (DIGIT_M << TUBE_1_OFFSET);
            break;

        case UI_MENU_STATE_ALARM_ENABLED:
            if(running_data.fields.alarm_data.alarm_enabled)
            {
                driver_buffer |= (DIGIT_NONE << TUBE_4_OFFSET);
                driver_buffer |= (DIGIT_NONE << TUBE_3_OFFSET);
                driver_buffer |= (DIGIT_O << TUBE_2_OFFSET);
                driver_buffer |= (DIGIT_N << TUBE_1_OFFSET);
            }
            else
            {
                driver_buffer |= (DIGIT_NONE << TUBE_4_OFFSET);
                driver_buffer |= (DIGIT_O << TUBE_3_OFFSET);
                driver_buffer |= (DIGIT_F << TUBE_2_OFFSET);
                driver_buffer |= (DIGIT_F << TUBE_1_OFFSET);
            }
            break;
            
        case UI_MENU_STATE_ALARM_SET:
            driver_buffer |= (DIGIT_NONE << TUBE_4_OFFSET);
            driver_buffer |= (DIGIT_S << TUBE_3_OFFSET);
            driver_buffer |= (DIGIT_E << TUBE_2_OFFSET);
            driver_buffer |= (DIGIT_T << TUBE_1_OFFSET);
            break;

        case UI_MENU_STATE_ALARM_SET_HH:
            no_update_req = 1;
            break;
            
        case UI_MENU_STATE_ALARM_SET_MM:
            no_update_req = 1;
            break;

        case UI_MENU_STATE_ALARM_BACK:
            driver_buffer |= (DIGIT_B << TUBE_4_OFFSET);
            driver_buffer |= (DIGIT_A << TUBE_3_OFFSET);
            driver_buffer |= (DIGIT_C << TUBE_2_OFFSET);
            driver_buffer |= (DIGIT_K << TUBE_1_OFFSET);
            break;

        case UI_MENU_STATE_BEEP:
            driver_buffer |= (DIGIT_B << TUBE_4_OFFSET);
            driver_buffer |= (DIGIT_E << TUBE_3_OFFSET);
            driver_buffer |= (DIGIT_E << TUBE_2_OFFSET);
            driver_buffer |= (DIGIT_P << TUBE_1_OFFSET);
            break;

        case UI_MENU_STATE_BEEP_ENABLE:
        case UI_MENU_STATE_BEEP_ENABLE_SEL:
            if(running_data.fields.beep_data.beep_enabled)
            {
                driver_buffer |= (DIGIT_NONE << TUBE_4_OFFSET);
                driver_buffer |= (DIGIT_NONE << TUBE_3_OFFSET);
                driver_buffer |= (DIGIT_O << TUBE_2_OFFSET);
                driver_buffer |= (DIGIT_N << TUBE_1_OFFSET);
            }
            else
            {
                driver_buffer |= (DIGIT_NONE << TUBE_4_OFFSET);
                driver_buffer |= (DIGIT_O << TUBE_3_OFFSET);
                driver_buffer |= (DIGIT_F << TUBE_2_OFFSET);
                driver_buffer |= (DIGIT_F << TUBE_1_OFFSET);
            }
            break;
            
        case UI_MENU_STATE_BEEP_BACK:
            driver_buffer |= (DIGIT_B << TUBE_4_OFFSET);
            driver_buffer |= (DIGIT_A << TUBE_3_OFFSET);
            driver_buffer |= (DIGIT_C << TUBE_2_OFFSET);
            driver_buffer |= (DIGIT_K << TUBE_1_OFFSET);
            break;
            
        case UI_MENU_STATE_DISPLAY:
            driver_buffer |= (DIGIT_D << TUBE_4_OFFSET);
            driver_buffer |= (DIGIT_I << TUBE_3_OFFSET);
            driver_buffer |= (DIGIT_S << TUBE_2_OFFSET);
            driver_buffer |= (DIGIT_P << TUBE_1_OFFSET);
            break;

        case UI_MENU_STATE_DISPLAY_SET:
        case UI_MENU_STATE_DISPLAY_SET_SEL:
            switch(running_data.fields.display_data.selected)
            {
                case UI_DISPLAY_STATE_CLOCK_HHMM:
                    driver_buffer |= (DIGIT_H << TUBE_4_OFFSET);
                    driver_buffer |= (DIGIT_H << TUBE_3_OFFSET);
                    driver_buffer |= (DIGIT_M << TUBE_2_OFFSET);
                    driver_buffer |= (DIGIT_M << TUBE_1_OFFSET);
                    break;

                case UI_DISPLAY_STATE_CLOCK_MMSS:
                    driver_buffer |= (DIGIT_M << TUBE_4_OFFSET);
                    driver_buffer |= (DIGIT_M << TUBE_3_OFFSET);
                    driver_buffer |= (DIGIT_S << TUBE_2_OFFSET);
                    driver_buffer |= (DIGIT_S << TUBE_1_OFFSET);
                    break;

                case UI_DISPLAY_STATE_CLOCK_SSMM:
                    driver_buffer |= (DIGIT_S << TUBE_4_OFFSET);
                    driver_buffer |= (DIGIT_S << TUBE_3_OFFSET);
                    driver_buffer |= (DIGIT_N << TUBE_2_OFFSET);
                    driver_buffer |= (DIGIT_N << TUBE_1_OFFSET);
                    break;

                case UI_DISPLAY_STATE_CLOCK_YYYY:
                    driver_buffer |= (DIGIT_Y << TUBE_4_OFFSET);
                    driver_buffer |= (DIGIT_Y << TUBE_3_OFFSET);
                    driver_buffer |= (DIGIT_Y << TUBE_2_OFFSET);
                    driver_buffer |= (DIGIT_Y << TUBE_1_OFFSET);
                    break;

                case UI_DISPLAY_STATE_CLOCK_MMDD:
                    driver_buffer |= (DIGIT_M << TUBE_4_OFFSET);
                    driver_buffer |= (DIGIT_M << TUBE_3_OFFSET);
                    driver_buffer |= (DIGIT_D << TUBE_2_OFFSET);
                    driver_buffer |= (DIGIT_D << TUBE_1_OFFSET);
                    break;
            }
            break;

        case UI_MENU_STATE_DISPLAY_BACK:
            driver_buffer |= (DIGIT_B << TUBE_4_OFFSET);
            driver_buffer |= (DIGIT_A << TUBE_3_OFFSET);
            driver_buffer |= (DIGIT_C << TUBE_2_OFFSET);
            driver_buffer |= (DIGIT_K << TUBE_1_OFFSET);
            break;

        case UI_MENU_STATE_RESET:
            driver_buffer |= (DIGIT_NONE << TUBE_4_OFFSET);
            driver_buffer |= (DIGIT_R << TUBE_3_OFFSET);
            driver_buffer |= (DIGIT_S << TUBE_2_OFFSET);
            driver_buffer |= (DIGIT_T << TUBE_1_OFFSET);
            break;
            
        case UI_MENU_STATE_RESET_CONFIRM:
            driver_buffer |= (DIGIT_C << TUBE_4_OFFSET);
            driver_buffer |= (DIGIT_O << TUBE_3_OFFSET);
            driver_buffer |= (DIGIT_N << TUBE_2_OFFSET);
            driver_buffer |= (DIGIT_F << TUBE_1_OFFSET);
            break;

        case UI_MENU_STATE_RESET_BACK:
            driver_buffer |= (DIGIT_B << TUBE_4_OFFSET);
            driver_buffer |= (DIGIT_A << TUBE_3_OFFSET);
            driver_buffer |= (DIGIT_C << TUBE_2_OFFSET);
            driver_buffer |= (DIGIT_K << TUBE_1_OFFSET);
            break;

        case UI_MENU_STATE_EXIT:
            driver_buffer |= (DIGIT_E << TUBE_4_OFFSET);
            driver_buffer |= (DIGIT_X << TUBE_3_OFFSET);
            driver_buffer |= (DIGIT_I << TUBE_2_OFFSET);
            driver_buffer |= (DIGIT_T << TUBE_1_OFFSET);
            break;

        default:
            no_update_req = 1;
            break;
    }
    
    if(!no_update_req) display_send_buffer(driver_buffer);
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

void display_all(void)
{
    uint64_t driver_buffer = 0ULL; // Start with an empty buffer
    driver_buffer |= (DIGIT_ALL << TUBE_4_OFFSET);
    driver_buffer |= (DIGIT_ALL << TUBE_3_OFFSET);
    driver_buffer |= (DIGIT_ALL << TUBE_2_OFFSET);
    driver_buffer |= (DIGIT_ALL << TUBE_1_OFFSET);
    driver_buffer |= START_SEPARATOR_BOTH;
    driver_buffer |= MIDDLE_SEPARATOR_BOTH;
    display_send_buffer(driver_buffer); // Load buffer into the driver
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
    display_last_driver_buffer = buffer;
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
    DELAY_microseconds(1);
    LATCH_GPIO_SetLow();
    display_update_pending = 0;
}

void display_local_time(time_t time)
{
    display = time;
    display = display + tz_offset;
    if(isDST(&time)) display = display+dst_offset;
    previous_display = display;

    switch(ui_state_current)
    {
        case UI_DISPLAY_STATE_CLOCK_HHMM:
            display_time(&display);
            break;
            
        case UI_DISPLAY_STATE_CLOCK_MMSS:
            display_mmss(&display);
            break;
            
        case UI_DISPLAY_STATE_CLOCK_SSMM:
            display_ssmm(&utc);
            break;
            
        case UI_DISPLAY_STATE_CLOCK_YYYY:
            display_yyyy(&display);
            break;
            
        case UI_DISPLAY_STATE_CLOCK_MMDD:
            display_mmdd(&display);
            break;
            
        case UI_DISPLAY_STATE_TEMP:
            display_temp(bme280_temperature);
            break;
            
        case UI_DISPLAY_STATE_DASHES:
            display_dashes();
            break;
            
        case UI_DISPLAY_STATE_INIT:
            display_blank();
            break;
            
        default:
            break;
    }
}

// DST is stupid
// Valid for UK and EU and other places where
// DST starts on the last Sunday of March
// and ends on the last Sunday of October
// at 0100 UTC
bool isDST(const time_t *time)
{
    struct tm *disp_time;
    
    // Convert our time_t into a time struct
    disp_time = gmtime(time);
    uint8_t next_sunday = 0;
    uint8_t last_sunday = 0;
    bool dst = 0;

    // Are we in the DST danger zone?
    if(disp_time->tm_mon >= 2 && disp_time->tm_mon <= 9)
    {
        // If we're in April to September is always DST
        if(disp_time->tm_mon > 2 && disp_time->tm_mon < 9)
        {
            dst = 1;
        }
        
        // Are we in either March or October?
        else if(disp_time->tm_mon == 2 || disp_time->tm_mon == 9)
        {
            // Calculate next Sunday
            next_sunday = disp_time->tm_mday + (7 - disp_time->tm_wday);
            
            // And then the last Sunday of the month
            last_sunday = next_sunday;
            if(last_sunday > 31) 
            {
                last_sunday -= 7;
            }
            else
            {
                while((last_sunday + 7) <= 31)
                {
                    last_sunday += 7;
                }
            }
            
            // If we are in March
            if(disp_time->tm_mon == 2)
            {
                // Are we past the last Sunday?
                if(disp_time->tm_mday > last_sunday)
                {
                    dst = 1;
                }
                // Is this the last Sunday in the month?
                else if(disp_time->tm_mday == last_sunday)
                {
                    // Apply DST if it's after 0100 UTC
                    if(disp_time->tm_hour >= 1)
                    {
                        dst = 1;
                    }
                }
            }
            //If we are in October
            else if(disp_time->tm_mon == 9)
            {
                // Are we still before the last Sunday?
                if(disp_time->tm_mday < last_sunday)
                {
                    dst = 1;
                }
                // Is this the last Sunday in the month?
                if(disp_time->tm_mday == last_sunday)
                {
                    // Apply DST if it's before 0100 UTC
                    if(disp_time->tm_hour < 1)
                    {
                        dst = 1;
                    }
                }
            }
        }
    }
    
    dst_active = dst;
    // Return DST status
    return dst;
}

void display_timezone_incr(void)
{
    tz_offset = tz_offset + TZ_OFFSET_STEP_SIZE;
    if(tz_offset > TZ_OFFSET_MAX)
    {
        tz_offset = TZ_OFFSET_MIN;
    }
}

void display_timezone_incr_hh(void)
{
    int32_t tz_hour = tz_offset / 3600;
    int32_t tz_minute = (tz_offset - (tz_hour * 3600)) / 60;
    tz_hour++;
    if(tz_hour > 14)
    {
        tz_hour = -12;
        tz_minute = tz_minute * -1;
    }
    
    tz_offset = tz_hour * 3600;
    tz_offset += tz_minute * 60;
}

void display_timezone_incr_mm(void)
{
    int32_t tz_hour = tz_offset / 3600;
    int32_t tz_minute = (tz_offset - (tz_hour * 3600)) / 60;
    if(tz_offset >= 0)
    {
        tz_minute = tz_minute + 15;
        if(tz_minute >= 60)
        {
            tz_minute = 0;
        }
    }
    else 
    {
        tz_minute = tz_minute - 15;
        if(tz_minute <= -60)
        {
            tz_minute = 0;
        }
    }
    tz_offset = (tz_hour * 3600) + (tz_minute * 60);
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
