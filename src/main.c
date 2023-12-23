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

#include "freq.h"
#include "gnss.h"
#include "gnss_pps.h"
#include "pcf8563.h"
#include "rtc.h"
#include "pic_pps.h"
#include "scheduler.h"
#include "sht30.h"
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

// time_t to store UTC, GNSS, RTC and local time
time_t utc;
time_t local;

int32_t tz_offset = 0;

uint32_t fosc_freq = FCYCLE;

extern bool oc_adjust_in_progress;

extern int32_t oc_offset;
extern bool oc_event;

extern bool pps_sync;
extern bool gnss_fix;

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
    
    gnss_pps_init();
    pic_pps_init();
    UART2_SetRxInterruptHandler(rx_gnss);
    scheduler_init();
    display_init();
 
    printf("\033[2J\033[1;1H"); // Clear the terminal window
    printf("\r\nHELLO!\r\n\r\n"); // And say hello!
    printf("Running @ 80MHz on 10.000000MHz XTAL\r\n");
    DELAY_microseconds(10000);
    
    rtc_read_set_calendar();
    pic_pps_reset_sync();
    reset_pps_stats();

    local = utc + tz_offset;
    if(isDST(&utc)) local = local+3600; 
    display_time(&local);
    display_latch();
    
    // Enable WDT (set to 32s timeout non-windowed mode)
    RCONbits.SWDTEN = 1;
    
    while (1)
    {
        if(t1ms0)
        {
            t1ms0=0;
            // Have we had an OC event?
            if(oc_event)
            {
                // Clear the WDT
                ClrWdt();
                
                // Calculate some PPS statistics
                calculate_pps_stats();

                // Calculate some OC statistics
                pic_pps_calculate_oc_stats();
                
                // Check if we are still in sync
                pic_pps_evaluate_sync();
                
                if(pic_pps_resync_required()) pic_pps_resync();

                // Clear window if we're in debug mode
#ifdef __DEBUG
                ui_print_clear_window();
#endif
                // Print IS08601 timestamp to serial
                printf("UTC: ");
                ui_print_iso8601_string(utc);
                printf("\r\n");

                // Check if we need to beep
                ui_buzzer_interval_beep();
#ifdef __DEBUG
                print_data = 1;
#endif
                oc_event = 0;
            }
        }
        
        if(t10ms0)
        {
            t10ms0=0;
            
            // Is there a new set of GNSS time data available
            if(ubx_gnss_available())
            {
                ubx_update_gnss_time();
                
                // Check our time solution is valid
                if(ubx_gnss_time_valid())
                {
                    //Check that UTC and GNSS time match
                    if(!is_gnss_calendar_sync(utc))
                    {
                        // Trigger a re-sync if not
                        reset_gnss_calendar_sync();
                        rtc_reset_calendar_sync();
                        sync_gnss_calendar();
                        rtc_write_from_calendar(utc);
                        // Update our RTC now we have a GNSS time
                        if(!rtc_is_calendar_sync())
                        {
                            printf("Writing RTC\r\n");
                            rtc_write_from_calendar(utc);
                        }
                    }
                }
                else
                {
                    pic_pps_reset_sync();
                    reset_pps_stats();
                }
            }
            
            // Is there new time mark data available
            if(ubx_timemark_waiting()) ubx_update_timemark();
            
            // Check for any bytes on UART1
            if(U1STAbits.URXDA) ui_uart1_input();

            // Print some statistics if required
            if(print_data)
            {
                pic_pps_print_stats();
                print_ubx_tim_tm2_data();
                print_ubx_nav_timeutc_data();
                print_ubx_nav_clock_data();
                print_ubx_nav_status_data();
                //print_sht30_data();
                printf("\r\n");
                print_data = 0;
            }
        }

        if(t10ms1>=2)
        {
            t10ms1=0;
            ui_buzzer_sounder();
            ui_button_task();
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
            
            local = utc + tz_offset;
            if(isDST(&utc)) local = local+3600; 
            
            display_time(&local);
            
            // Re-enable manual printing
            disable_manual_print = 0;
            
        }
    }
    return 1; 
}
