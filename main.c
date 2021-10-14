#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include "mcc_generated_files/mcc.h"
#include "tubes.h"
#include "gps.h"

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

bool adjust_in_progress = 0;
bool pps_sync = 0;
bool print_data = 0;

void incr_clock(void);
void set_latch_cycles(uint32_t);

int main(void)
{
    // initialize the device
    SYSTEM_Initialize();
    CN_SetInterruptHandler(incr_clock);
    UART2_SetRxInterruptHandler(rx_gps);
    ADC1_ChannelSelect(PDO);
    ADC1_SoftwareTriggerDisable();
    OC1_Start();
    display_init();

    time_t utc;
    
    while (1)
    {
        if(rmc_waiting)
        {
            rmc_waiting=0;
            // Process our waiting GNRMC message into UTC
            utc = process_rmc();
            
            // Print it to serial
            char buf[32] = {0};
            struct tm *utc_time;
            utc_time = gmtime(&utc);
            strftime(buf, 32, "%Y-%m-%dT%H:%M:%SZ", utc_time);
            printf(buf);
            printf("\r\n");
            
            // Increment for next PPS and load into display
            utc++;
            display_time(&utc);
            print_data = 1;
        }
        if(print_data)
        {
            // Calculate our PPS and OC stats, needs to be moved to a timer loop?
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
            }
            if(!pps_sync && pps_count_diff == 40000000)
            {
                set_latch_cycles(40000000 + oc_offset+107);
                adjust_in_progress = 1;
            }
            printf("PPS D:%lu OC D:%li\r\n", pps_count_diff, oc_offset);
            printf("PPS C:%lu OC C:%li\r\n", pps_count, oc_count);
            printf("PPS S: %i ADJ: %i\r\n", pps_sync, adjust_in_progress);
            printf("mV: %.0f ns: %.0f\r\n",pdo_mv, pps_offset_ns);
            print_data = 0;
        }
    }
    return 1; 
}

void incr_clock(void)
{
    if(PPS_GetValue())
    {
        STATUS_LED_SetHigh();
        DELAY_microseconds(2);
        ADC1_SoftwareTriggerDisable();
        DELAY_microseconds(2);
        pdo_mv = (ADC1_ConversionResultGet(PDO) * 16) / 19.859;
        pps_offset_ns = (pdo_mv * pdo_mv * 0.000051) + (0.28 * pdo_mv);
    }
    else
    {
        STATUS_LED_SetLow();
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
    if(adjust_in_progress)
    {
        set_latch_cycles(40000000);
        adjust_in_progress = 0;
        pps_sync = 1;
    }
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
