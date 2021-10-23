#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include "mcc_generated_files/mcc.h"
#include "tubes.h"
#include "gps.h"
#include "gps_pps.h"
#include "scheduler.h"
#include "ds1307.h"

extern bool rmc_waiting;
uint8_t timezone = 1;

extern uint16_t ic1_val;
extern uint16_t ic1_val_old;
extern uint16_t ic2_val;
extern uint16_t ic2_val_old;
extern uint32_t pps_count;
extern uint32_t pps_count_diff;
extern uint32_t pps_count_old;
extern uint16_t pps_seq_count;

uint16_t ic3_val = 0;
uint16_t ic3_val_old = 0;
uint16_t ic4_val = 0;
uint16_t ic4_val_old = 0;
uint32_t oc_count = 0;
int32_t oc_offset = 0;
uint32_t oc_count_diff = 0;
uint32_t oc_count_old = 0;

float pdo_mv = 0;
float pps_offset_ns = 0;

bool oc_adjust_in_progress = 0;
bool pps_sync = 0;
bool pps_done = 0;
bool oc_event = 0;

bool gps_calendar_sync = 0;
int minute = 0;
int old_minute = 0;

bool print_data = 0;

bool rtc_sync = 0;

extern bool scheduler_sync;
extern bool scheduler_adjust_in_progress;
extern uint8_t t1ms0;
extern uint8_t t10ms0;
extern uint8_t t100ms0;
extern uint8_t t100ms1;

uint8_t bcd2bin(uint8_t val);
uint8_t bin2bcd(uint8_t val);
void incr_clock(void);
void set_latch_cycles(uint32_t);

int main(void)
{
    // initialize the device
    PIN_MANAGER_Initialize();
    CLOCK_Initialize();
    INTERRUPT_Initialize();
    UART1_Initialize();
    IC4_Initialize();
    IC3_Initialize();
    OC2_Initialize();
    OC1_Initialize();
    gps_pps_init();
    UART2_Initialize();
    I2C1_Initialize();
    SPI2_Initialize();
    ADC1_Initialize();
    INTERRUPT_GlobalEnable();
    SYSTEM_CORCONModeOperatingSet(CORCON_MODE_PORVALUES);
    
    printf("\033[2J\033[1;1H"); // Clear the terminal window
    printf("\r\nHELLO!\r\n\r\n"); // And say hello!
    printf("Waiting for Rb lock...");
    while(!OSC_READY_GetValue());
    DELAY_milliseconds(100); // Wait for things to wake up
    printf(" locked!\r\n");
    
    CN_SetInterruptHandler(incr_clock);
    UART2_SetRxInterruptHandler(rx_gps);
    scheduler_init();
    ADC1_ChannelSelect(PDO);
    ADC1_SoftwareTriggerDisable();
    OC1_Start();
    display_init();

    // time_t to store UTC, GPS and RTC time
    time_t utc;
    time_t gps;
    time_t rtc;
    
    // Read RTC for an estimate of current time and display it
    rtc = DS1307_read();
    utc = rtc;
    //display_time(&utc);
    display_mmss(&utc);
    
    // ISO8601 string buffer
    char buf[32] = {0};
    
    while (1)
    {
        // Every 1ms
        if(t1ms0)
        {
            t1ms0=0;
            // Have we had an OC event?
            if(oc_event)
            {
                // Calculate our PPS and OC stats
                pps_count = (((uint32_t)ic2_val)<<16) + ic1_val; // Raw timer
                pps_count_diff = pps_count-pps_count_old; // Difference from last
                pps_count_old = pps_count; // Store the new value as old

                oc_count = (((uint32_t)ic4_val)<<16) + ic3_val; // Raw timer
                oc_count_diff = oc_count - oc_count_old; // Difference from last
                oc_offset = pps_count-oc_count; // Calculate the offset between PPS and OC
                oc_count_old = oc_count; // Store the new value as old
                
                // Do we need to resync?
                if((pps_seq_count>10)&&((oc_offset<-132)||(oc_offset>-92)))
                {
                    pps_sync = 0;
                    scheduler_sync = 0;
                    gps_calendar_sync = 0;
                    rtc_sync = 0;
                }
                // Only sync if required and if we get 40000000 cycles
                if(!pps_sync && pps_count_diff == 40000000)
                {
                    set_latch_cycles(40000000 + oc_offset+107);
                    oc_adjust_in_progress = 1;
                }

                // Print resulting time to serial
                struct tm *utc_oc;
                utc_oc = gmtime(&utc);
                strftime(buf, 32, "UTC: %Y-%m-%dT%H:%M:%SZ", utc_oc);
                printf(buf);
                printf("\r\n");
                
                // Every minute, print some statistics
                minute =  utc_oc->tm_min;
                if(minute!=old_minute) print_data = 1;
                old_minute = minute;
                
                //rtc = DS1307_read();
                oc_event = 0;
            }
        }
        // Every 10ms
        if(t10ms0)
        {
            t10ms0=0;
            // Print some statistics if required
            if(print_data)
            {
                // Cycles between current and last PPS, and the OC offset from this
                printf("PPS D:%lu OC D:%li\r\n", pps_count_diff, oc_offset);
                // Raw timer values for both PPS and OC
                printf("PPS C:%lu OC C:%li\r\n", pps_count, oc_count);
                // PPS sync status
                printf("PPS S: %i ADJ: %i\r\n", pps_sync, oc_adjust_in_progress);
                // Scheduler sync status
                printf("SCH S: %i\r\n", scheduler_sync);
                // PD output information
                printf("mV: %.0f ns: %.0f\r\n",pdo_mv, pps_offset_ns);
                print_data = 0;
            }
        }
        // Every 100ms
        if(t100ms0==1)
        {
            t100ms0 = 0;
            STATUS_LED_Toggle(); // Toggle the LED for no reason
            // Do we have a GPS message waiting?
            if(rmc_waiting)
            {
                rmc_waiting=0;
                // Process our waiting GNRMC message into our time_t
                gps = process_rmc();

                // Do we have a valid time from GPS
                if(gps)
                {
                    // Check if we've still got the correct time
                    if(utc!=gps)
                    {
                        // Trigger a re-sync if not
                        gps_calendar_sync = 0;
                    }

                    //If we're not sync'd
                    if(!gps_calendar_sync)
                    {
                        // Copy GPS time into our UTC calendar
                        utc = gps;
                        // Print resulting time to serial
                        char buf[32] = {0};
                        struct tm *utc_time;
                        utc_time = gmtime(&utc);
                        strftime(buf, 32, "%Y-%m-%dT%H:%M:%SZ", utc_time);
                        printf("GPS calendar sync\r\nTime is now: ");
                        printf(buf);
                        printf("\r\n");

                        // Update our RTC now we have a GPS time
                        if(!rtc_sync)
                        {
                            printf("Writing RTC\r\n");
                            rtc_sync = DS1307_write(utc);
                        }
                        // Our internal calendar is now sync'd with GPS
                        gps_calendar_sync = 1;
                    }
                }
            }
        }
        // Every 1000ms, but at a 900ms offset
        if(t100ms1==9)
        {
            t100ms1 = -1; // Reset to -1 to trigger this every 1s
            // Increment for next PPS and load into display
            utc++;
            //display_time(&utc);
            display_mmss(&utc);
        }
    }
    return 1; 
}

void incr_clock(void)
{
    if(PPS_GetValue())
    {
        // Wait for at least 2us for the capacitor to stop charging
        DELAY_microseconds(2);
        // Trigger a conversion
        ADC1_SoftwareTriggerDisable();
        // Wait a bit more...
        DELAY_microseconds(2);
        // Read ADC and convert reading to mV
        pdo_mv = (ADC1_ConversionResultGet(PDO) * 16) / 19.859;
        // Convert from mV to nanoseconds
        pps_offset_ns = (pdo_mv * pdo_mv * 0.000051) + (0.28 * pdo_mv);
    }
}

void IC3_CallBack(void)
{
    // Only sync the scheduler after OC
    if(pps_sync && !scheduler_sync)
    {
        scheduler_align(); // Align the scheduler with our OC
    }
    if(oc_adjust_in_progress)
    {
        set_latch_cycles(40000000); // Reset our OC to 1Hz
        oc_adjust_in_progress = 0; // Clear the adjustment flag
        pps_sync = 1; // Indicate we are now sync'd with PPS
        print_data = 1; // Print stats after resync
    }
    oc_event = 1; // Flag we've just had an OC event
    rmc_waiting = 0; // Invalidate any GPS data that's waiting
    ic3_val = IC3_CaptureDataRead(); // Read the IC3 timer
}
void IC4_CallBack(void)
{
    ic4_val = IC4_CaptureDataRead(); // Read the IC4 timer
}

// Adjusts the trigger registers for OC1 and OC2
void set_latch_cycles(uint32_t cycles)
{
    // Split into two 16 bit values for OC1 and OC2
    uint16_t msb = ((cycles - 1) >> 16) & 0xFFFF;
    uint16_t lsb = (cycles - 1) & 0xFFFF;
    OC1R = lsb - 3; // Latch pulse width is 3 cycles
    OC1RS = lsb;
    OC2R = msb;
    OC2RS = msb;
}

/**
 End of File
*/
