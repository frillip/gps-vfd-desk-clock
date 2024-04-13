#include "gnss.h"

time_t gnss = 0;
extern time_t utc;
extern time_t power_on_time;
extern uint32_t total_oc_seq_count;

char gnss_string_buffer[GNSS_STRING_BUFFER_SIZE] = {0};
uint8_t gnss_offset = 0;
uint8_t gnss_bytes_remaining = 0;
uint8_t gnss_checksum = 0;
uint8_t gnss_recieved_checksum = 0;
char gnss_check_buffer[GNSS_CHECK_BUFFER_SIZE] = {0};
GNSS_MESSAGE_TYPE gnss_incoming = GNSS_NONE;
GNSS_MESSAGE_TYPE gnss_waiting = GNSS_NONE;

extern char ubx_tim_tm2_buffer[UBX_TIM_TM2_LENGTH];
extern char ubx_tim_tm2_string[GNSS_CHECK_BUFFER_SIZE];
extern bool ubx_tim_tm2_waiting;

extern char ubx_nav_timeutc_buffer[UBX_NAV_TIMEUTC_LENGTH];
extern char ubx_nav_timeutc_string[GNSS_CHECK_BUFFER_SIZE];
extern bool ubx_nav_timeutc_waiting;

extern char ubx_nav_clock_buffer[UBX_NAV_TIMEUTC_LENGTH];
extern char ubx_nav_clock_string[GNSS_CHECK_BUFFER_SIZE];
extern bool ubx_nav_clock_waiting;

extern char ubx_nav_status_buffer[UBX_NAV_TIMEUTC_LENGTH];
extern char ubx_nav_status_string[GNSS_CHECK_BUFFER_SIZE];
extern bool ubx_nav_status_waiting;

extern char ubx_nav_posllh_buffer[UBX_NAV_TIMEUTC_LENGTH];
extern char ubx_nav_posllh_string[GNSS_CHECK_BUFFER_SIZE];
extern bool ubx_nav_posllh_waiting;

char gnss_rmc_buffer[GNSS_STRING_BUFFER_SIZE] = {0};
char gnss_rmc_string[GNSS_CHECK_BUFFER_SIZE] = "$GNRMC";
bool gnss_rmc_waiting = 0;

bool gnss_rmc_fix = 0;

bool gnss_calendar_sync = 0;

bool gnss_detected = 0;

void gnss_init(void)
{
    UART2_Initialize();
    memset(gnss_string_buffer, 0, GNSS_STRING_BUFFER_SIZE);
    memset(gnss_check_buffer, 0, GNSS_CHECK_BUFFER_SIZE);
    UART2_SetRxInterruptHandler(gnss_rx);
}

void gnss_rx(void)
{
    char rx_char = 0;
    
    while(UART2_IsRxReady()) 
    {
        // Read the character from the UART
        rx_char = UART2_Read();
        // Shuffle our check buffer along
        memmove(gnss_check_buffer, gnss_check_buffer+1, GNSS_CHECK_BUFFER_SIZE-1);
        // Add the new character to the end of the buffer
        gnss_check_buffer[GNSS_CHECK_BUFFER_SIZE-1] = rx_char;
        
        // Add the character to the end of the main buffer
        // if we're still receiving a message
        if(gnss_incoming!=GNSS_NONE)
        {
            gnss_detected = 1;
            gnss_string_buffer[gnss_offset] = rx_char;
            gnss_offset++;
            gnss_bytes_remaining--;
            // If we've reached the end of our buffer
            // then we're getting unknown messages
            // Discard anything that doesn't fit
            // and flag the incoming message as waiting
            if(gnss_offset>=GNSS_STRING_BUFFER_SIZE-1 || gnss_bytes_remaining==0)
            {
                gnss_waiting = gnss_incoming;
                gnss_incoming = GNSS_NONE;
            }
        }
        
        // Do we have a message that's finished coming in?
        if(gnss_waiting!=GNSS_NONE)
        {
            // if so, copy it to the right buffer
            gnss_copy_buffer(gnss_waiting);
            // Then reset the message buffer
            memset(gnss_string_buffer, 0, GNSS_STRING_BUFFER_SIZE);
            // Reset the message waiting flag
            gnss_waiting=GNSS_NONE;
        }
        
        // Check if the check buffer matches any of our magic strings
        // But don't do anything with it yet as there may still
        // old message data in the buffer
        
        GNSS_MESSAGE_TYPE gnss_check_res = gnss_check_incoming();
        if(gnss_check_res != GNSS_NONE)
        {
            gnss_waiting = gnss_incoming;
            gnss_incoming = gnss_check_res;
            gnss_offset = GNSS_CHECK_BUFFER_SIZE;
            switch (gnss_incoming)
            {
                case GNSS_UBX_TIM_TM2:
                    gnss_bytes_remaining = UBX_TIM_TM2_LENGTH - GNSS_CHECK_BUFFER_SIZE;
                    memcpy(gnss_string_buffer, gnss_check_buffer, GNSS_CHECK_BUFFER_SIZE);
                    break;
        
                case GNSS_UBX_NAV_TIMEUTC:
                    gnss_bytes_remaining = UBX_NAV_TIMEUTC_LENGTH - GNSS_CHECK_BUFFER_SIZE;
                    memcpy(gnss_string_buffer, gnss_check_buffer, GNSS_CHECK_BUFFER_SIZE);
                    break;
        
                case GNSS_UBX_NAV_CLOCK:
                    gnss_bytes_remaining = UBX_NAV_CLOCK_LENGTH - GNSS_CHECK_BUFFER_SIZE;
                    memcpy(gnss_string_buffer, gnss_check_buffer, GNSS_CHECK_BUFFER_SIZE);
                    break;
        
                case GNSS_UBX_NAV_STATUS:
                    gnss_bytes_remaining = UBX_NAV_STATUS_LENGTH - GNSS_CHECK_BUFFER_SIZE;
                    memcpy(gnss_string_buffer, gnss_check_buffer, GNSS_CHECK_BUFFER_SIZE);
                    break;
        
                case GNSS_UBX_NAV_POSLLH:
                    gnss_bytes_remaining = UBX_NAV_POSLLH_LENGTH - GNSS_CHECK_BUFFER_SIZE;
                    memcpy(gnss_string_buffer, gnss_check_buffer, GNSS_CHECK_BUFFER_SIZE);
                    break;
        
                case GNSS_GNRMC:
                    gnss_bytes_remaining = GNSS_STRING_BUFFER_SIZE - GNSS_CHECK_BUFFER_SIZE;
                    memcpy(gnss_string_buffer, gnss_check_buffer, GNSS_CHECK_BUFFER_SIZE);
                    break;
        
                default:
                    break;
            }
        }
    }
}

void gnss_copy_buffer(GNSS_MESSAGE_TYPE message)
{
    switch (message)
    {
        case GNSS_UBX_TIM_TM2:
            memcpy(ubx_tim_tm2_buffer, gnss_string_buffer, UBX_TIM_TM2_LENGTH);
            ubx_tim_tm2_waiting=1;
            break;

        case GNSS_UBX_NAV_TIMEUTC:
            memcpy(ubx_nav_timeutc_buffer, gnss_string_buffer, UBX_NAV_TIMEUTC_LENGTH);
            ubx_nav_timeutc_waiting=1;
            break;

        case GNSS_UBX_NAV_CLOCK:
            memcpy(ubx_nav_clock_buffer, gnss_string_buffer, UBX_NAV_CLOCK_LENGTH);
            ubx_nav_clock_waiting=1;
            break;

        case GNSS_UBX_NAV_STATUS:
            memcpy(ubx_nav_status_buffer, gnss_string_buffer, UBX_NAV_STATUS_LENGTH);
            ubx_nav_status_waiting=1;
            break;

        case GNSS_UBX_NAV_POSLLH:
            memcpy(ubx_nav_posllh_buffer, gnss_string_buffer, UBX_NAV_POSLLH_LENGTH);
            ubx_nav_posllh_waiting=1;
            break;

        case GNSS_GNRMC:
            memcpy(gnss_rmc_buffer, gnss_string_buffer, GNSS_STRING_BUFFER_SIZE);
            gnss_rmc_waiting=1;
            break;

        default:
            gnss_waiting=GNSS_NONE;
            break;
    }
}

GNSS_MESSAGE_TYPE gnss_check_incoming(void)
{
    if(memcmp(gnss_check_buffer, ubx_tim_tm2_string, GNSS_CHECK_BUFFER_SIZE)==0) return GNSS_UBX_TIM_TM2;
    if(memcmp(gnss_check_buffer, ubx_nav_timeutc_string, GNSS_CHECK_BUFFER_SIZE)==0) return GNSS_UBX_NAV_TIMEUTC;
    if(memcmp(gnss_check_buffer, ubx_nav_clock_string, GNSS_CHECK_BUFFER_SIZE)==0) return GNSS_UBX_NAV_CLOCK;
    if(memcmp(gnss_check_buffer, ubx_nav_status_string, GNSS_CHECK_BUFFER_SIZE)==0) return GNSS_UBX_NAV_STATUS;
    if(memcmp(gnss_check_buffer, ubx_nav_posllh_string, GNSS_CHECK_BUFFER_SIZE)==0) return GNSS_UBX_NAV_POSLLH;
    if(memcmp(gnss_check_buffer, gnss_rmc_string, GNSS_CHECK_BUFFER_SIZE)==0) return GNSS_GNRMC;
    return GNSS_NONE;
}

void gnss_set_calendar(void)
{
#ifdef DEBUG_MESSAGES
    printf("GNSS calendar sync\r\nTime is now: ");
    ui_print_iso8601_string(gnss);
    printf("\r\n");
#endif

    utc = gnss;
    if(!power_on_time) power_on_time = utc - total_oc_seq_count;
    gnss_calendar_sync = 1;
}

bool gnss_is_calendar_sync(time_t utc)
{
    return utc==gnss;
}

void gnss_reset_calendar_sync(void)
{
    gnss_calendar_sync=0;
}

time_t gnss_process_rmc(void)
{
    struct tm gnss_time;
    gnss_time.tm_isdst = 0;
    uint8_t i;
    uint8_t field = 0;
    bool field_waiting = 0;
    for(i=0; i<GNSS_STRING_BUFFER_SIZE; i++)
    {
        if(field==1 && field_waiting)
        {
            if(gnss_rmc_buffer[i] == ',') return 0; // We don't have a time yet, so return 0
            gnss_time.tm_sec = (gnss_rmc_buffer[i+5]-0x30);
            gnss_time.tm_sec += (gnss_rmc_buffer[i+4]-0x30)*10;
            gnss_time.tm_min = (gnss_rmc_buffer[i+3]-0x30);
            gnss_time.tm_min += (gnss_rmc_buffer[i+2]-0x30)*10;
            gnss_time.tm_hour = (gnss_rmc_buffer[i+1]-0x30);
            gnss_time.tm_hour += (gnss_rmc_buffer[i]-0x30)*10;
            field_waiting = 0;
        }
        else if(field==2 && field_waiting)
        {
            if(gnss_rmc_buffer[i] == 'A') gnss_rmc_fix = 1;
            else gnss_rmc_fix = 0;
            field_waiting = 0;
        }
        else if(field==9 && field_waiting)
        {
            gnss_time.tm_mday = (gnss_rmc_buffer[i+1]-0x30);
            gnss_time.tm_mday += (gnss_rmc_buffer[i]-0x30)*10;
            gnss_time.tm_mon = (gnss_rmc_buffer[i+3]-0x30);
            gnss_time.tm_mon += (gnss_rmc_buffer[i+2]-0x30)*10;
            gnss_time.tm_mon -= 1; // tm_mon is zero indexed for no reason
            gnss_time.tm_year = (gnss_rmc_buffer[i+5]-0x30+100);
            gnss_time.tm_year += (gnss_rmc_buffer[i+4]-0x30)*10;
            field_waiting = 0;
        }
        else field_waiting = 0;
        if(gnss_rmc_buffer[i]==',')
        {
            field++;
            field_waiting = 1;
        }
        else if(gnss_rmc_buffer[i]==0x0a)
        {
            i=GNSS_STRING_BUFFER_SIZE;
        }
    }
    memset(gnss_rmc_buffer, 0, GNSS_STRING_BUFFER_SIZE);
    time_t utc;
    utc = mktime(&gnss_time);
    return utc;
}

void gnss_invalidate_data(void)
{
    gnss_rmc_waiting = 0;
    ubx_invalidate_data();
}
