#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include "mcc_generated_files/mcc.h"
#include "tubes.h"
#include "gps.h"
#include "scheduler.h"

extern bool rmc_waiting;
uint8_t timezone = 1;

uint16_t ic1_val = 0;
uint16_t ic1_val_old = 0;
uint16_t ic2_val = 0;
uint16_t ic2_val_old = 0;
uint32_t pps_count = 0;
uint32_t pps_count_diff = 0;
uint32_t pps_count_old = 0;
uint16_t pps_seq_count = 0;

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
bool oc_done = 0;

bool gps_calendar_sync = 0;

bool print_data = 0;

extern bool scheduler_sync;
extern bool scheduler_adjust_in_progress;
extern uint8_t t1ms0;
extern uint8_t t10ms0;
extern uint8_t t100ms0;
extern uint8_t t100ms1;

void incr_clock(void);
void set_latch_cycles(uint32_t);

int main(void)
{
    // initialize the device
    SYSTEM_Initialize();
    CN_SetInterruptHandler(incr_clock);
    UART2_SetRxInterruptHandler(rx_gps);
    TMR2_SetInterruptHandler(scheduler_run);
    TMR2_Start();
    ADC1_ChannelSelect(PDO);
    ADC1_SoftwareTriggerDisable();
    OC1_Start();
    display_init();

    time_t utc;
    time_t gps;
    
    while (1)
    {
        if(t1ms0)
        {
            if(oc_done)
            {
                // Calculate our PPS and OC stats
                pps_count = (((uint32_t)ic2_val)<<16) + ic1_val;
                pps_count_diff = pps_count-pps_count_old;
                pps_count_old = pps_count;

                oc_count = (((uint32_t)ic4_val)<<16) + ic3_val;
                oc_count_diff = oc_count - oc_count_old;
                oc_offset = pps_count-oc_count;
                oc_count_old = oc_count;
                if((pps_seq_count>10)&&((oc_offset<-132)||(oc_offset>-92)))
                {
                    pps_sync = 0;
                    scheduler_sync = 0;
                    gps_calendar_sync = 0;
                }
                if(!pps_sync && pps_count_diff == 40000000)
                {
                    set_latch_cycles(40000000 + oc_offset+107);
                    oc_adjust_in_progress = 1;
                }

                // Print it to serial
                char buf2[32] = {0};
                struct tm *utc_oc;
                utc_oc = gmtime(&utc);
                strftime(buf2, 32, "%Y-%m-%dT%H:%M:%SZ", utc_oc);
                printf("OC: ");
                printf(buf2);
                printf("\r\n");
                
                oc_done = 0;
            }
        }
        if(t10ms0)
        {
            t10ms0=0;
            if(print_data)
            {
                printf("PPS D:%lu OC D:%li\r\n", pps_count_diff, oc_offset);
                printf("PPS C:%lu OC C:%li\r\n", pps_count, oc_count);
                printf("PPS S: %i ADJ: %i\r\n", pps_sync, oc_adjust_in_progress);
                printf("SCH S: %i ADJ: %i\r\n", scheduler_sync, scheduler_adjust_in_progress);
                printf("mV: %.0fns: %.0f\r\n",pdo_mv, pps_offset_ns);
                print_data = 0;
            }
        }
        if(t100ms0==1)
        {
            t100ms0 = 0;
            STATUS_LED_Toggle();
            if(rmc_waiting)
            {
                rmc_waiting=0;

                // Process our waiting GNRMC message into UTC
                gps = process_rmc();

                if(utc!=gps)
                {
                    gps_calendar_sync = 0;
                }

                if(!gps_calendar_sync)
                {
                    utc = gps;
                    // Print it to serial
                    char buf[32] = {0};
                    struct tm *utc_time;
                    utc_time = gmtime(&utc);
                    strftime(buf, 32, "%Y-%m-%dT%H:%M:%SZ", utc_time);
                    //printf("GPS calendar sync\r\nTime is now: ");
                    printf("GPS: ");
                    printf(buf);
                    printf("\r\n");

                    gps_calendar_sync = 1;
                }
            }
        }
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
        //STATUS_LED_SetHigh();
        DELAY_microseconds(2);
        ADC1_SoftwareTriggerDisable();
        DELAY_microseconds(2);
        pdo_mv = (ADC1_ConversionResultGet(PDO) * 16) / 19.859;
        pps_offset_ns = (pdo_mv * pdo_mv * 0.000051) + (0.28 * pdo_mv);
    }
    else
    {
        //STATUS_LED_SetLow();
    }
}

void IC1_CallBack(void)
{
    pps_seq_count++;
    ic1_val = IC1_CaptureDataRead();
}
void IC2_CallBack(void)
{
    ic2_val = IC2_CaptureDataRead();
}

void IC3_CallBack(void)
{
    if(pps_sync && !scheduler_sync)
    {
        scheduler_align();
    }
    if(oc_adjust_in_progress)
    {
        set_latch_cycles(40000000);
        oc_adjust_in_progress = 0;
        pps_sync = 1;
        print_data = 1;
    }
    oc_done = 1;
    rmc_waiting = 0;
    ic3_val = IC3_CaptureDataRead();
}
void IC4_CallBack(void)
{
    ic4_val = IC4_CaptureDataRead();
}

void set_latch_cycles(uint32_t cycles)
{
    uint16_t msb = ((cycles - 1) >> 16) & 0xFFFF;
    uint16_t lsb = (cycles - 1) & 0xFFFF;
    OC1R = lsb - 3;
    OC1RS = lsb;
    OC2R = msb;
    OC2RS = msb;
}

/**
 End of File
*/
