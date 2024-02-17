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

bool sync_state_machine_run = 0;

// time_t to store UTC, GNSS, RTC and local time
time_t utc;
time_t local;
time_t previous_local;
extern time_t rtc;
extern bool rtc_detected;

extern time_t gnss;
extern bool gnss_detected;

extern time_t ntp;
extern bool esp_detected;

int32_t tz_offset = 0;
int32_t dst_offset = 3600;

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

    printf("\033[2J\033[1;1H"); // Clear the terminal window
    printf("\r\nHELLO!\r\n\r\n"); // And say hello!
    printf("Running @ 80MHz on 10.000000MHz XTAL\r\n");
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
                if(esp_gnss_data_updated) esp_tx_gnss();
                if(esp_brightness_updated)
                {
                    display_brightness_set_target(esp_brightness);
                    esp_brightness_updated = 0;
                }
            }
            display_brightness_update();

            // Print some statistics if required
            if(print_data)
            {
                pic_pps_print_stats();
                if(gnss_detected)
                {
                    print_ubx_tim_tm2_data();
                    print_ubx_nav_timeutc_data();
                    print_ubx_nav_clock_data();
                    print_ubx_nav_status_data();
                    print_ubx_nav_posllh_data();
                }
                else
                {
                    printf("\r\n=== NO GNSS DETECTED ===\r\n");
                }
                print_sync_state_machine();
                printf("\r\n");
                print_data = 0;
            }
        }

        if(t10ms1>=2)
        {
            t10ms1=0;
            ui_buzzer_sounder();
            ui_button_task();
            ui_display_task();
        }
        if(t100ms0==1)
        {
            t100ms0 = 0;
            STATUS_LED_Toggle();
        }
        
        if(t100ms1==9)
        {
            t100ms1 = -1;
            utc++;
            
            previous_local = local;
            local = utc + tz_offset;
            if(isDST(&utc)) local = local+dst_offset; 
            
            if(ui_state_current==UI_DISPLAY_STATE_CLOCK_HHMM) display_time(&local);
            else if(ui_state_current==UI_DISPLAY_STATE_CLOCK_MMSS) display_mmss(&local);
            
            // Re-enable manual printing
            disable_manual_print = 0;
            
        }
    }
    return 1; 
}
