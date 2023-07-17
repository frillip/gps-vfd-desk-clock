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

bool print_data = 0;
bool disable_manual_print = 0;
uint8_t resync_interval = 30;

extern bool scheduler_sync;
extern bool scheduler_adjust_in_progress;
extern uint8_t t1ms0;
extern uint8_t t10ms0;
extern uint8_t t100ms0;
extern uint8_t t100ms1;

// time_t to store UTC, GNSS, RTC and local time
time_t utc;
time_t local;

int32_t tz_offset = 0;

uint32_t fosc_freq = FCYCLE;

extern bool oc_adjust_in_progress;

extern uint32_t pps_seq_count;

extern int32_t accumulated_clocks;
extern time_t accumulation_start;
extern time_t accumulation_delta;

extern int32_t oc_offset;
extern bool oc_event;

extern bool pps_sync;
extern bool gnss_fix;

extern bool display_blinking;

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

    bool startup = 1;
    
    // ISO8601 string buffer
    char buf[32] = {0};
    
    printf("\033[2J\033[1;1H"); // Clear the terminal window
    printf("\r\nHELLO!\r\n\r\n"); // And say hello!
    printf("Running @ 80MHz on 10.000000MHz XTAL\r\n");
    DELAY_microseconds(10000);
    // sht30_start_periodic_meas();
    DELAY_microseconds(10000);
    
    set_from_rtc_calendar();
    reset_sync();
    reset_pps_stats();

    local = utc + tz_offset;
    if(isDST(&utc)) local = local+3600; 
    if(DST_SW_GetValue()) display_time(&local);
    else display_mmss(&utc);
    display_latch();
    
    uint8_t minute = 0;
    uint8_t old_minute = 0;
    
    while (1)
    {
        if(t1ms0)
        {
            t1ms0=0;
            // Have we had an OC event?
            if(oc_event)
            {
                // Calculate some PPS statistics
                calculate_pps_stats();

                // Calculate some OC statistics
                calculate_oc_stats();
                if(accumulated_clocks > FCYCLE_ACC_LIM_POSITIVE || accumulated_clocks < FCYCLE_ACC_LIM_NEGATIVE)
                {
                    if((accumulation_delta > FCYCLE_ACC_INTERVAL_MIN && scheduler_sync)||accumulated_clocks > FCYCLE_ACC_RESET_POSITIVE || accumulated_clocks < FCYCLE_ACC_RESET_NEGATIVE)
                    {
                        fosc_freq = calculate_fosc_freq(fosc_freq);
                        printf("\r\nNew Fosc freq: %luHz\r\n", fosc_freq);
                        printf("CLK D: %li CLK T: %li\r\n",accumulated_clocks, accumulation_delta);
                        reset_sync();
                        reset_pps_stats();
                    }
                }
                if(!pps_sync && pps_seq_count>5)
                {
                    set_latch_cycles(fosc_freq + oc_offset);
                    oc_adjust_in_progress = 1;
                    if(!accumulation_start)
                    {
                        accumulation_start = utc;
                        accumulated_clocks = 0;
                    }
                }
                
                //// sht30_read_data();
                
                // Clear window if we're in debug mode
#ifdef __DEBUG
                print_clear_window();
#endif
                // Print IS08601 timestamp to serial
                struct tm *utc_oc;
                utc_oc = gmtime(&utc);
                strftime(buf, 32, "UTC: %Y-%m-%dT%H:%M:%SZ", utc_oc);
                printf(buf);
                printf("\r\n");
                
                // Every minute, print some statistics
                minute =  utc_oc->tm_min;
                if(minute!=old_minute) print_data = 1;
                old_minute = minute;
                
#ifdef __DEBUG
                print_data = 1;
#endif
                oc_event = 0;
            }
        }
        
        if(t10ms0)
        {
            t10ms0=0;
            _LATB7 = !TZ_BT_GetValue();
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
                        reset_rtc_calendar_sync();
                        sync_gnss_calendar();
                        PCF8563_write(utc);
                        // Update our RTC now we have a GNSS time
                        if(!is_rtc_calendar_sync())
                        {
                            printf("Writing RTC\r\n");
                            PCF8563_write(utc);
                            sync_rtc_calendar(utc);
                        }
                    }
                }
                else
                {
                    reset_sync();
                    reset_pps_stats();
                }
            }
            
            // Is there new time mark data available
            if(ubx_timemark_waiting()) ubx_update_timemark();
            
            // Check for any bytes on UART1
            while(U1STAbits.URXDA)
            {
                char c = UART1_Read();
                // print some data if enter has been pressed
                if(c==0x0d && !disable_manual_print)
                {
                    print_data = 1;
                    // Disable spamming in case of cats on keyboards
                    disable_manual_print = 1;
                }
                if(c==0x72 && !resync_interval)
                {
                    resync_interval = 30;
                    fosc_freq = calculate_fosc_freq(fosc_freq);
                    printf("\r\nManual resync\r\n");
                    printf("New Fosc freq: %luHz\r\n", fosc_freq);
                    printf("CLK D: %li CLK T: %li\r\n",accumulated_clocks, accumulation_delta);
                    reset_sync();
                    reset_pps_stats();
                }
            }

            // Print some statistics if required
            if(print_data)
            {
                print_stats();
                print_ubx_tim_tm2_data();
                print_ubx_nav_timeutc_data();
                print_ubx_nav_clock_data();
                print_ubx_nav_status_data();
                //print_sht30_data();
                printf("\r\n");
                print_data = 0;
            }
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
            
            if(DST_SW_GetValue()) display_time(&local);
            else display_mmss(&utc);
            //display_latch();

            //// sht30_start_meas();
            // sht30_read_periodic_data();
            // Re-enable manual printing
            disable_manual_print = 0;
            if(resync_interval) resync_interval--;
            
            //LATCH_GPIO_Toggle();
            //BLANK_Toggle();
        }
    }
    return 1; 
}

void print_clear_window(void)
{
    printf("\033[2J\033[1;1H"); // Clear the terminal window
}