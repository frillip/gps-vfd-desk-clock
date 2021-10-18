#include <string.h>
#include <stdio.h>
#include "mcc_generated_files/uart2.h"
#include "mcc_generated_files/uart1.h"
#include "gps.h"
#include "tubes.h"

char nmea_buffer[80] = {0};
uint8_t nmea_offset = 0;
uint8_t nmea_bytes = 0;
uint8_t nmea_checksum = 0;
uint8_t nmea_recieved_checksum = 0;

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