/**
  Generated main.c file from MPLAB Code Configurator

  @Company
    Microchip Technology Inc.

  @File Name
    main.c

  @Summary
    This is the generated main.c using PIC24 / dsPIC33 / PIC32MM MCUs.

  @Description
    This source file provides main entry point for system initialization and application code development.
    Generation Information :
        Product Revision  :  PIC24 / dsPIC33 / PIC32MM MCUs - 1.170.0
        Device            :  dsPIC33EP512GP502
    The generated drivers are tested against the following:
        Compiler          :  XC16 v1.61
        MPLAB 	          :  MPLAB X v5.45
*/

/*
    (c) 2020 Microchip Technology Inc. and its subsidiaries. You may use this
    software and any derivatives exclusively with Microchip products.

    THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER
    EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY IMPLIED
    WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS FOR A
    PARTICULAR PURPOSE, OR ITS INTERACTION WITH MICROCHIP PRODUCTS, COMBINATION
    WITH ANY OTHER PRODUCTS, OR USE IN ANY APPLICATION.

    IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE,
    INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND
    WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS
    BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE. TO THE
    FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL CLAIMS IN
    ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF FEES, IF ANY,
    THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.

    MICROCHIP PROVIDES THIS SOFTWARE CONDITIONALLY UPON YOUR ACCEPTANCE OF THESE
    TERMS.
*/

/**
  Section: Included Files
*/
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include "mcc_generated_files/system.h"
#include "mcc_generated_files/pin_manager.h"
#include "mcc_generated_files/mcc.h"
#include "mcc_generated_files/uart1.h"
#include "mcc_generated_files/uart2.h"
#include "tubes.h"
#include "gps.h"

uint16_t counter = 0;
extern bool rmc_waiting;
extern char rmc_buffer[80];
uint8_t seconds;
uint8_t minutes;
uint8_t hours;
uint8_t timezone = 1;

uint16_t ic1_val = 0;
uint16_t ic1_val_old = 0;
uint16_t ic2_val = 0;
uint16_t ic2_val_old = 0;
uint32_t pps_count = 0;
uint32_t pps_count_diff = 0;
uint32_t pps_count_old = 0;
bool first_pps = 1;

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

bool print_data = 0;


/*
                         Main application
 */

void incr_clock(void);

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
    gps_time.tm_mday = 0;
    gps_time.tm_mon = 0;
    gps_time.tm_year = 0;
    time_t utc;
    
    while (1)
    {
        //display_count(counter);
        if(rmc_waiting)
        {
            rmc_waiting=0;
            gps_time.tm_sec = (rmc_buffer[12]-0x30);
            gps_time.tm_sec += (rmc_buffer[11]-0x30)*10;
            gps_time.tm_min = (rmc_buffer[10]-0x30);
            gps_time.tm_min += (rmc_buffer[9]-0x30)*10;
            gps_time.tm_hour = (rmc_buffer[8]-0x30);
            gps_time.tm_hour += (rmc_buffer[7]-0x30)*10;
            gps_time.tm_mday = (rmc_buffer[54]-0x30);
            gps_time.tm_mday += (rmc_buffer[53]-0x30)*10;
            gps_time.tm_mon = (rmc_buffer[56]-0x30);
            gps_time.tm_mon += (rmc_buffer[55]-0x30)*10;
            gps_time.tm_year = (rmc_buffer[58]-0x30+100);
            gps_time.tm_year += (rmc_buffer[57]-0x30)*10;
            gps_time.tm_isdst = 0;
            memset(rmc_buffer, 0, sizeof rmc_buffer);
            //char buf[32] = {0};
            //strftime(buf, 32, "%Y-%m-%dT%H:%M:%SZ", &gps_time);
            //printf(buf);
            utc = mktime(&gps_time);
            utc++;
            //display_time(&utc);
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
            printf("%lu %li\r\n", pps_count_diff, oc_offset);
            //printf("%lu %li\r\n", pps_count, oc_count);
            printf("%.0f %.0f\r\n",pdo_mv, pps_offset_ns);
            print_data = 0;
        }
    }
    return 1; 
}

void incr_clock(void)
{
    if(PPS_GetValue())
    {
        //display_latch();
        ADC1_SoftwareTriggerDisable();
        //while(!ADC1_IsConversionComplete(PDO));
        
        pdo_mv = (ADC1_ConversionResultGet(PDO) * 16) / 19.859;
        pps_offset_ns = (pdo_mv * pdo_mv * 0.000051) + (0.28 * pdo_mv);
        //display_count((uint16_t)pps_offset_ns);
        display_count(counter);
        display_latch();
        print_data = 1;
        counter++;
        //STATUS_LED_SetHigh();
    }
    else
    {
        //STATUS_LED_SetLow();
    }
}

void IC1_CallBack(void)
{
    if(first_pps) 
    {
        OC2CON1bits.OCM = 0;
        OC1CON1bits.OCM = 0;
        OC1CON2 = 0x13F;
        OC2CON2 = 0x11F;
        OC2_Start();
        OC1_Start();
        first_pps = 0;
    }
    ic1_val = IC1_CaptureDataRead();
}
void IC2_CallBack(void)
{
    ic2_val = IC2_CaptureDataRead();
}

void IC3_CallBack(void)
{
    ic3_val = IC3_CaptureDataRead();
}
void IC4_CallBack(void)
{
    //while(!IC4_IsCaptureBufferEmpty)
    ic4_val = IC4_CaptureDataRead();
}
/**
 End of File
*/

