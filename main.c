#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include "mcc_generated_files/mcc.h"
#include "tubes.h"
#include "gps.h"

extern bool rmc_waiting;
extern char rmc_buffer[80];
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
    
    struct tm gps_time;
    gps_time.tm_sec = 0;
    gps_time.tm_min = 0;
    gps_time.tm_hour = 0;
    gps_time.tm_mday = 1;
    gps_time.tm_mon = 9; // tm_mon is zero indexed for no reason
    gps_time.tm_year = 121;
    time_t utc;
    
    while (1)
    {
        if(rmc_waiting)
        {
            rmc_waiting=0;
            gps_time.tm_sec = (rmc_buffer[12]-0x30);
            gps_time.tm_sec += (rmc_buffer[11]-0x30)*10;
            gps_time.tm_min = (rmc_buffer[10]-0x30);
            gps_time.tm_min += (rmc_buffer[9]-0x30)*10;
            gps_time.tm_hour = (rmc_buffer[8]-0x30);
            gps_time.tm_hour += (rmc_buffer[7]-0x30)*10;
            if(rmc_buffer[17] == 'A')
            {
                gps_time.tm_mday = (rmc_buffer[54]-0x30);
                gps_time.tm_mday += (rmc_buffer[53]-0x30)*10;
                gps_time.tm_mon = (rmc_buffer[56]-0x30);
                gps_time.tm_mon += (rmc_buffer[55]-0x30)*10;
                gps_time.tm_mon -= 1; // tm_mon is zero indexed for no reason
                gps_time.tm_year = (rmc_buffer[58]-0x30+100);
                gps_time.tm_year += (rmc_buffer[57]-0x30)*10;
            }
            else
            {
                gps_time.tm_mday = (rmc_buffer[26]-0x30);
                gps_time.tm_mday += (rmc_buffer[25]-0x30)*10;
                gps_time.tm_mon = (rmc_buffer[28]-0x30);
                gps_time.tm_mon += (rmc_buffer[27]-0x30)*10;
                gps_time.tm_mon -= 1; // tm_mon is zero indexed for no reason
                gps_time.tm_year = (rmc_buffer[30]-0x30+100);
                gps_time.tm_year += (rmc_buffer[29]-0x30)*10;
            }
            gps_time.tm_isdst = 0;
            memset(rmc_buffer, 0, sizeof rmc_buffer);
            char buf[32] = {0};
            strftime(buf, 32, "%Y-%m-%dT%H:%M:%SZ", &gps_time);
            printf(buf);
            printf("\r\n");
            utc = mktime(&gps_time);
            utc++;
            display_mmss(&utc);
            //display_time(&utc);
            print_data = 1;
        }
        if(print_data)
        {
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
                pps_sync = 1;
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
