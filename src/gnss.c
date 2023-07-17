#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "mcc_generated_files/uart2.h"
#include "mcc_generated_files/uart1.h"
#include "gnss.h"
#include "ublox_ubx.h"

time_t gnss;

char gnss_buffer[GNSS_BUFFER_SIZE] = {0};
uint8_t gnss_offset = 0;
uint8_t gnss_bytes_remaining = 0;
uint8_t gnss_checksum = 0;
uint8_t gnss_recieved_checksum = 0;
char check_buffer[CHECK_BUFFER_SIZE] = {0};
enum gnss_message_type gnss_incoming = GNSS_NONE;
enum gnss_message_type gnss_waiting = GNSS_NONE;

extern char ubx_tim_tm2_buffer[UBX_TIM_TM2_LENGTH];
extern char ubx_tim_tm2_string[CHECK_BUFFER_SIZE];
extern bool ubx_tim_tm2_waiting;

extern char ubx_nav_timeutc_buffer[UBX_NAV_TIMEUTC_LENGTH];
extern char ubx_nav_timeutc_string[CHECK_BUFFER_SIZE];
extern bool ubx_nav_timeutc_waiting;

extern char ubx_nav_clock_buffer[UBX_NAV_TIMEUTC_LENGTH];
extern char ubx_nav_clock_string[CHECK_BUFFER_SIZE];
extern bool ubx_nav_clock_waiting;

extern char ubx_nav_status_buffer[UBX_NAV_TIMEUTC_LENGTH];
extern char ubx_nav_status_string[CHECK_BUFFER_SIZE];
extern bool ubx_nav_status_waiting;

char rmc_buffer[GNSS_BUFFER_SIZE] = {0};
char rmc_string[CHECK_BUFFER_SIZE] = "$GNRMC";
bool rmc_waiting = 0;

bool rmc_gnss_fix = 0;

bool gnss_calendar_sync = 0;

void rx_gnss(void)
{
    char rx_char = 0;
    
    while(UART2_IsRxReady()) 
    {
        // Read the character from the UART
        rx_char = UART2_Read();
        // Shuffle our check buffer along
        memmove(check_buffer, check_buffer+1, 5);
        // Add the new character to the end of the buffer
        check_buffer[5] = rx_char;
        
        // Add the character to the end of the main buffer
        // if we're still receiving a message
        if(gnss_incoming!=GNSS_NONE)
        {
            gnss_buffer[gnss_offset] = rx_char;
            gnss_offset++;
            gnss_bytes_remaining--;
            // If we've reached the end of our buffer
            // then we're getting unknown messages
            // Discard anything that doesn't fit
            // and flag the incoming message as waiting
            if(gnss_offset>=GNSS_BUFFER_SIZE-1 || gnss_bytes_remaining==0)
            {
                gnss_waiting = gnss_incoming;
                gnss_incoming = GNSS_NONE;
            }
        }
        
        // Do we have a message that's finished coming in?
        if(gnss_waiting!=GNSS_NONE)
        {
            // if so, copy it to the right buffer
            if(gnss_waiting==GNSS_UBX_TIM_TM2)
            {
                memcpy(ubx_tim_tm2_buffer, gnss_buffer, UBX_TIM_TM2_LENGTH);
                ubx_tim_tm2_waiting=1;
            }
            else if(gnss_waiting==GNSS_UBX_NAV_TIMEUTC)
            {
                memcpy(ubx_nav_timeutc_buffer, gnss_buffer, UBX_NAV_TIMEUTC_LENGTH);
                ubx_nav_timeutc_waiting=1;
            }
            else if(gnss_waiting==GNSS_UBX_NAV_CLOCK)
            {
                memcpy(ubx_nav_clock_buffer, gnss_buffer, UBX_NAV_CLOCK_LENGTH);
                ubx_nav_clock_waiting=1;
            }
            else if(gnss_waiting==GNSS_UBX_NAV_STATUS)
            {
                memcpy(ubx_nav_status_buffer, gnss_buffer, UBX_NAV_STATUS_LENGTH);
                ubx_nav_status_waiting=1;
            }
            else if(gnss_waiting==GNSS_GNRMC)
            {
                memcpy(rmc_buffer, gnss_buffer, GNSS_BUFFER_SIZE);
                rmc_waiting=1;
            }
            // Then reset the message buffer
            memset(gnss_buffer, 0, GNSS_BUFFER_SIZE);
            // Reset the message waiting flag
            gnss_waiting=GNSS_NONE;
        }
        
        // Check if the check buffer matches any of our magic strings
        // But don't do anything with it yet as there may still
        // old message data in the buffer
        if(memcmp(check_buffer, ubx_tim_tm2_string, CHECK_BUFFER_SIZE)==0)
        {
            gnss_waiting = gnss_incoming;
            gnss_incoming = GNSS_UBX_TIM_TM2;
            gnss_bytes_remaining = UBX_TIM_TM2_LENGTH - CHECK_BUFFER_SIZE;
            gnss_offset = CHECK_BUFFER_SIZE;
            memcpy(gnss_buffer, check_buffer, CHECK_BUFFER_SIZE);
        }
        else if(memcmp(check_buffer, ubx_nav_timeutc_string, CHECK_BUFFER_SIZE)==0)
        {
            gnss_waiting = gnss_incoming;
            gnss_incoming = GNSS_UBX_NAV_TIMEUTC;
            gnss_bytes_remaining = UBX_NAV_TIMEUTC_LENGTH - CHECK_BUFFER_SIZE;
            gnss_offset = CHECK_BUFFER_SIZE;
            memcpy(gnss_buffer, check_buffer, CHECK_BUFFER_SIZE);
        }
        else if(memcmp(check_buffer, ubx_nav_clock_string, CHECK_BUFFER_SIZE)==0)
        {
            gnss_waiting = gnss_incoming;
            gnss_incoming = GNSS_UBX_NAV_CLOCK;
            gnss_bytes_remaining = UBX_NAV_CLOCK_LENGTH - CHECK_BUFFER_SIZE;
            gnss_offset = CHECK_BUFFER_SIZE;
            memcpy(gnss_buffer, check_buffer, CHECK_BUFFER_SIZE);
        }
        else if(memcmp(check_buffer, ubx_nav_status_string, CHECK_BUFFER_SIZE)==0)
        {
            gnss_waiting = gnss_incoming;
            gnss_incoming = GNSS_UBX_NAV_STATUS;
            gnss_bytes_remaining = UBX_NAV_STATUS_LENGTH - CHECK_BUFFER_SIZE;
            gnss_offset = CHECK_BUFFER_SIZE;
            memcpy(gnss_buffer, check_buffer, CHECK_BUFFER_SIZE);
        }
        else if(memcmp(check_buffer, rmc_string, CHECK_BUFFER_SIZE)==0)
        {
            gnss_waiting = gnss_incoming;
            gnss_incoming = GNSS_GNRMC;
            gnss_bytes_remaining = GNSS_BUFFER_SIZE - CHECK_BUFFER_SIZE;
            gnss_offset = CHECK_BUFFER_SIZE;
            memcpy(gnss_buffer, check_buffer, CHECK_BUFFER_SIZE);
        }
    }
}

extern time_t utc;
void sync_gnss_calendar(void)
{
    // Print resulting time to serial
    char buf[32] = {0};
    struct tm *utc_time;
    utc_time = gmtime(&gnss);
    strftime(buf, 32, "%Y-%m-%dT%H:%M:%SZ", utc_time);
    printf("GNSS calendar sync\r\nTime is now: ");
    strftime(buf, 32, "%Y-%m-%dT%H:%M:%SZ", utc_time);
    printf(buf);
    printf("\r\n");
    
    utc = gnss;
    gnss_calendar_sync = 1;
}

bool is_gnss_calendar_sync(time_t utc)
{
    return utc==gnss;
}

void reset_gnss_calendar_sync(void)
{
    gnss_calendar_sync=0;
}

time_t process_rmc(void)
{
    struct tm gnss_time;
    gnss_time.tm_isdst = 0;
    uint8_t i;
    uint8_t field = 0;
    bool field_waiting = 0;
    for(i=0; i<GNSS_BUFFER_SIZE; i++)
    {
        if(field==1 && field_waiting)
        {
            if(rmc_buffer[i] == ',') return 0; // We don't have a time yet, so return 0
            gnss_time.tm_sec = (rmc_buffer[i+5]-0x30);
            gnss_time.tm_sec += (rmc_buffer[i+4]-0x30)*10;
            gnss_time.tm_min = (rmc_buffer[i+3]-0x30);
            gnss_time.tm_min += (rmc_buffer[i+2]-0x30)*10;
            gnss_time.tm_hour = (rmc_buffer[i+1]-0x30);
            gnss_time.tm_hour += (rmc_buffer[i]-0x30)*10;
            field_waiting = 0;
        }
        else if(field==2 && field_waiting)
        {
            if(rmc_buffer[i] == 'A') rmc_gnss_fix = 1;
            else rmc_gnss_fix = 0;
            field_waiting = 0;
        }
        else if(field==9 && field_waiting)
        {
            gnss_time.tm_mday = (rmc_buffer[i+1]-0x30);
            gnss_time.tm_mday += (rmc_buffer[i]-0x30)*10;
            gnss_time.tm_mon = (rmc_buffer[i+3]-0x30);
            gnss_time.tm_mon += (rmc_buffer[i+2]-0x30)*10;
            gnss_time.tm_mon -= 1; // tm_mon is zero indexed for no reason
            gnss_time.tm_year = (rmc_buffer[i+5]-0x30+100);
            gnss_time.tm_year += (rmc_buffer[i+4]-0x30)*10;
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
            i=GNSS_BUFFER_SIZE;
        }
    }
    memset(rmc_buffer, 0, GNSS_BUFFER_SIZE);
    time_t utc;
    utc = mktime(&gnss_time);
    return utc;
}