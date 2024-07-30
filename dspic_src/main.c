#include"mcc_generated_files/system.h"
#include"mcc_generated_files/clock.h"
#include"mcc_generated_files/pin_manager.h"
#include"mcc_generated_files/interrupt_manager.h"
#include"mcc_generated_files/i2c1.h"
#include"mcc_generated_files/spi2.h"
#include"mcc_generated_files/uart1.h"
#include"mcc_generated_files/uart2.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <xc.h>

#include "bme280.h"
#include "eeprom.h"
#include "esp32.h"
#include "freq.h"
#include "gnss.h"
#include "gnss_pps.h"
#include "pcf8563.h"
#include "rtc.h"
#include "pic_pps.h"
#include "scheduler.h"
#include "sht30.h"
#include "sync_state.h"
#include "tubes.h"
#include "ublox_ubx.h"
#include "ui.h"
#include "veml6040.h"

//#define DEBUG_MESSAGES

extern bool print_data;
extern bool disable_manual_print;
extern uint8_t resync_interval;

extern bool scheduler_sync;
extern bool scheduler_adjust_in_progress;
extern uint8_t t1ms0;
extern uint8_t t10ms0;
extern uint8_t t10ms1;
extern uint8_t t100ms0;
extern uint8_t t100ms1;
extern uint8_t t100ms2;

bool sync_state_machine_run = 0;

// time_t to store UTC, GNSS, RTC and local time
time_t utc;
time_t local;
bool dst_active;

extern time_t rtc;
extern bool rtc_detected;

extern time_t gnss;
extern bool gnss_detected;

extern time_t ntp;
extern bool esp_detected;

extern time_t display;

extern int32_t tz_offset;
extern int32_t dst_offset;

uint32_t fosc_freq = FCYCLE;

extern bool oc_adjust_in_progress;

extern int32_t oc_offset;
extern bool oc_event;
extern bool state_new_oc;
extern bool state_new_ic;

extern bool ic_event;

extern bool pps_sync;
extern bool gnss_fix;

extern bool esp_gnss_data_updated;
extern bool esp_brightness_updated;
extern uint16_t esp_brightness;

extern CLOCK_SYNC_STATUS clock_sync_state;
extern CLOCK_SYNC_STATUS clock_sync_state_old;
extern CLOCK_SYNC_STATUS clock_sync_state_last;

extern UI_DISPLAY_STATE ui_state_current;

extern bool veml6040_detected;
extern uint16_t veml_ambient_light;
extern uint16_t veml_brightness;

extern bool bme280_detected;
extern int32_t bme280_temperature;
extern uint32_t bme280_pressure;
extern int32_t bme280_humidity;

int main(void)
{
    // initialize the device
    PIN_MANAGER_Initialize();
    CLOCK_Initialize();
    INTERRUPT_Initialize();
    UART2_Initialize();
    I2C1_Initialize();
    SPI2_Initialize();
    UART1_Initialize();
    INTERRUPT_GlobalEnable();
    SYSTEM_CORCONModeOperatingSet(CORCON_MODE_PORVALUES);

#ifdef DEBUG_MESSAGES
    printf("\033[2J\033[1;1H"); // Clear the terminal window
    printf("\nHELLO!\n\n"); // And say hello!
    double fosc_freq_f = ((float)fosc_freq * XTAL_FREQ_MHZ)/FCYCLE;
    printf("Running @ 80MHz on %.06fMHz XTAL\n", fosc_freq_f);
#endif
    
    eeprom_init();
    
    veml6040_detected = VEML6040_init();
    if(veml6040_detected)
    {
        veml_ambient_light = VEML6040_get_lux();
        veml_brightness = VEML_calc_brightness(veml_ambient_light);
        display_brightness_set_target(veml_brightness);
    }
    
    bme280_detected = BME280_init();
    if(bme280_detected)
    {
        BME280_write_settings();
    }
        
    DELAY_microseconds(10000);
    
    scheduler_init();
    sync_state_machine_run = 1;
    
    ui_init();

    // Enable WDT (set to 32s timeout non-windowed mode)
    RCONbits.SWDTEN = 1;
    
    while (1)
    {
        if(t1ms0)
        {
            t1ms0=0;
        }

        if(t10ms0)
        {
            t10ms0=0;
            
            if(ui_state_current==UI_DISPLAY_STATE_CLOCK_SSMM)
            {
                display_ssmm(&utc);
                display_latch();
            }
            
            if(sync_state_machine_run)
            {
                sync_state_machine();
            }

            // Run our UBX data task if we have GNSS module
            if(gnss_detected) 
            {
                ubx_data_task();
            }
            
            // Check for any bytes on UART1
            if(esp_detected)
            {
                esp_data_task();
            }
            
            display_brightness_update();

            // Print some statistics if required
            if(print_data)
            {
                print_clocks();
                pic_pps_print_stats();
                print_ubx_data();
                print_esp_data();
                print_veml_data();
                print_bme280_data();
                print_sync_state_machine();
                printf("\n");
                print_data = 0;
            }
        }

        if(t10ms1>=2)
        {
            t10ms1=0;
            ui_button_task();
            ui_buzzer_task();
            ui_display_task();
        }
        if(t100ms0)
        {
            t100ms0 = 0;
            STATUS_LED_Toggle();
            if(veml6040_detected)
            {
                veml_ambient_light = VEML6040_get_lux();
                veml_brightness = VEML_calc_brightness(veml_ambient_light);
                display_brightness_set_target(veml_brightness);
            }
        }
        
        if(t100ms2==3)
        {
            t100ms2 = -2;
        }
        
        if(t100ms1==9)
        {
            t100ms1 = -1;
            display_local_time(utc+1);
            BME280_read_all();
            // Re-enable manual printing
            disable_manual_print = 0;
            
        }
    }
    return 1; 
}
