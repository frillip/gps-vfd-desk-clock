#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "mcc_generated_files/uart2.h"
#include "mcc_generated_files/uart1.h"
#include "gps.h"
#include "tubes.h"

char nmea_buffer[80] = {0};
uint8_t nmea_offset = 0;
uint8_t nmea_bytes = 0;
uint8_t nmea_checksum = 0;
uint8_t nmea_recieved_checksum = 0;

char ubx_buffer[80] = {0};
char ubx_string[6] = {0xB5, 0x62, 0x0D, 0x03, 0x1C, 0x00};
bool ubx_incoming = 0;
bool ubx_waiting = 0;
bool ubx_valid = 0;
uint16_t ubx_edge_count = 0;
uint16_t ubx_edge_count_old = 0;
uint32_t ubx_rising_ms = 0;
uint32_t ubx_rising_ms_old = 0;
int32_t ubx_rising_ms_diff = 0;
uint32_t ubx_rising_ns = 0;
uint32_t ubx_rising_ns_old = 0;
int32_t ubx_rising_ns_diff = 0;
uint32_t ubx_falling_ms = 0;
uint32_t ubx_falling_ms_old = 0;
int32_t ubx_falling_ms_diff = 0;
uint32_t ubx_falling_ns = 0;
uint32_t ubx_falling_ns_old = 0;
int32_t ubx_falling_ns_diff = 0;
int32_t ubx_ms_diff = 0;
int32_t ubx_ns_diff = 0;
uint32_t ubx_accuracy_ns = 0;

char rmc_buffer[80] = {0};
char rmc_string[7] = "$GNRMC,";
bool rmc_incoming = 0;
bool rmc_waiting = 0;

char gga_buffer[80] = {0};
char gga_string[7] = "$GNGGA,";
bool gga_incoming = 0;
bool gga_waiting = 0;

bool gps_fix = 0;

void rx_gps(void)
{
    char rx_char = 0;
    
    while(UART2_IsRxReady()) 
    {
        rx_char = UART2_Read();
        //UART1_Write(rx_char);
        nmea_buffer[nmea_offset] = rx_char;
        if((rx_char == 0x0a) || (nmea_offset>=79))
        {   
            uint8_t res;
            res = memcmp(nmea_buffer, ubx_string, 6);
            if(res==0)
            {
                ubx_waiting=1;
                memcpy(ubx_buffer, nmea_buffer, 80);
            }
            res = memcmp(nmea_buffer, rmc_string, 7);
            if(res==0)
            {
                rmc_waiting=1;
                memcpy(rmc_buffer, nmea_buffer, 80);
            }
            res = memcmp(nmea_buffer, gga_string, 7);
            if(res==0)
            {
                gga_waiting=1;
                memcpy(gga_buffer, nmea_buffer, 80);
            }
            memset(nmea_buffer, 0, sizeof nmea_buffer);
            nmea_offset=0;
        }
        else nmea_offset++;
    }
}

void process_ubx(void)
{
    ubx_rising_ms_old = ubx_rising_ms;
    ubx_rising_ns_old = ubx_rising_ns;
    ubx_falling_ms_old = ubx_falling_ms;
    ubx_falling_ns_old = ubx_falling_ns;
    ubx_edge_count_old = ubx_edge_count;
    // ublox protocol message UBX-TIM-TM2
    // Byte 0-1 is header 0xB5 0x62 = µb
    // Byte 2 is 0x0D = timing message TIM
    // Byte 3 is 0x03 = Time mark message TM2
    // Byte 4-5 is message length, always 0x1C 0x00 = 28 bytes
    // Byte 6 is channel event occurred on, 0 = EXTINT0, 1 = EXTINT1
    // Byte 7 is bitfield, UBX-13003221 - R26  page 430
    // Byte 8-9 is edge count
    ubx_edge_count = ubx_buffer[8] + (ubx_buffer[9]<<8);
    if(ubx_edge_count==ubx_edge_count_old +1)
    {
        ubx_valid = 1;
    }
    else
    {
        ubx_valid = 0;
    }
    // Byte 10-11 is week number rising edge
    // Byte 12-13 is week number falling edge
    // Byte 14-17 is rising edge time of week in ms
    ubx_rising_ms = ((uint32_t)ubx_buffer[14]&0xFF);
    ubx_rising_ms += ((uint32_t)ubx_buffer[15]&0xFF)<<8;
    ubx_rising_ms += ((uint32_t)ubx_buffer[16]&0xFF)<<16;
    ubx_rising_ms += ((uint32_t)ubx_buffer[17]&0xFF)<<24;
    // Byte 18-21 is rising edge ms fraction in ns
    ubx_rising_ns = ((uint32_t)ubx_buffer[18]&0xFF);
    ubx_rising_ns += ((uint32_t)ubx_buffer[19]&0xFF)<<8;
    ubx_rising_ns += ((uint32_t)ubx_buffer[20]&0xFF)<<16;
    ubx_rising_ns += ((uint32_t)ubx_buffer[21]&0xFF)<<24;
    // Byte 22-25 is falling edge time of week in ms
    ubx_falling_ms = ((uint32_t)ubx_buffer[22]&0xFF);
    ubx_falling_ms += ((uint32_t)ubx_buffer[23]&0xFF)<<8;
    ubx_falling_ms += ((uint32_t)ubx_buffer[24]&0xFF)<<16;
    ubx_falling_ms += ((uint32_t)ubx_buffer[25]&0xFF)<<24;
    // Byte 26-29 is falling edge ms fraction in ns
    ubx_falling_ns = ((uint32_t)ubx_buffer[26]&0xFF);
    ubx_falling_ns += ((uint32_t)ubx_buffer[27]&0xFF)<<8;
    ubx_falling_ns += ((uint32_t)ubx_buffer[28]&0xFF)<<16;
    ubx_falling_ns += ((uint32_t)ubx_buffer[29]&0xFF)<<24;
    // Byte 30-33 is accuracy estimate in ns
    ubx_accuracy_ns = ((uint32_t)ubx_buffer[30]&0xFF);
    ubx_accuracy_ns += ((uint32_t)ubx_buffer[31]&0xFF)<<8;
    ubx_accuracy_ns += ((uint32_t)ubx_buffer[32]&0xFF)<<16;
    ubx_accuracy_ns += ((uint32_t)ubx_buffer[33]&0xFF)<<24;
    
    //ubx_rising_ns = ubx_rising_ns + ((uint32_t)(ubx_rising_ms)*1000000);
    //ubx_falling_ns = ubx_falling_ns + ((uint32_t)(ubx_falling_ms)*1000000);
    
    ubx_rising_ms_diff = ubx_rising_ms - ubx_rising_ms_old;
    ubx_rising_ns_diff = ubx_rising_ns - ubx_rising_ns_old;
    ubx_falling_ms_diff = ubx_falling_ms - ubx_falling_ms_old;
    ubx_falling_ns_diff = ubx_falling_ns - ubx_falling_ns_old;
    
    ubx_ms_diff = ubx_falling_ms - ubx_rising_ms;
    //if(ubx_ms_diff < 0) ubx_ms_diff += 1000;
    
    ubx_ns_diff = ubx_falling_ns - ubx_rising_ns;
    //if(ubx_ns_diff < 0) ubx_ns_diff += 1000000000;
    
    memset(ubx_buffer, 0, sizeof ubx_buffer);
}

time_t process_rmc(void)
{
    struct tm gps_time;
    gps_time.tm_isdst = 0;
    uint8_t i;
    uint8_t field = 0;
    bool field_waiting = 0;
    for(i=0; i<80; i++)
    {
        if(field==1 && field_waiting)
        {
            if(rmc_buffer[i] == ',') return 0; // We don't have a time yet, so return 0
            gps_time.tm_sec = (rmc_buffer[i+5]-0x30);
            gps_time.tm_sec += (rmc_buffer[i+4]-0x30)*10;
            gps_time.tm_min = (rmc_buffer[i+3]-0x30);
            gps_time.tm_min += (rmc_buffer[i+2]-0x30)*10;
            gps_time.tm_hour = (rmc_buffer[i+1]-0x30);
            gps_time.tm_hour += (rmc_buffer[i]-0x30)*10;
            field_waiting = 0;
        }
        else if(field==2 && field_waiting)
        {
            if(rmc_buffer[i] == 'A') gps_fix = 1;
            else gps_fix = 0;
            field_waiting = 0;
        }
        else if(field==9 && field_waiting)
        {
            gps_time.tm_mday = (rmc_buffer[i+1]-0x30);
            gps_time.tm_mday += (rmc_buffer[i]-0x30)*10;
            gps_time.tm_mon = (rmc_buffer[i+3]-0x30);
            gps_time.tm_mon += (rmc_buffer[i+2]-0x30)*10;
            gps_time.tm_mon -= 1; // tm_mon is zero indexed for no reason
            gps_time.tm_year = (rmc_buffer[i+5]-0x30+100);
            gps_time.tm_year += (rmc_buffer[i+4]-0x30)*10;
            field_waiting = 0;
        }
        else field_waiting = 0;
        if(rmc_buffer[i]==',')
        {
            field++;
            field_waiting = 1;
        }
        else if(rmc_buffer[i]==0x0a)
        {
            i=80;
        }
    }
    memset(rmc_buffer, 0, sizeof rmc_buffer);
    time_t utc;
    utc = mktime(&gps_time);
    return utc;
}