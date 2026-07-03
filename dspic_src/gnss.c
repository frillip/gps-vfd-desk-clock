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

extern UBX_NAV_CLOCK ubx_nav_clock_buffer;
extern UBX_HEADER ubx_nav_clock_string;
extern bool ubx_nav_clock_waiting;

extern UBX_NAV_POSLLH ubx_nav_posllh_buffer;
extern UBX_HEADER ubx_nav_posllh_string;
extern bool ubx_nav_posllh_waiting;

extern UBX_NAV_STATUS ubx_nav_status_buffer;
extern UBX_HEADER ubx_nav_status_string;
extern bool ubx_nav_status_waiting;

extern UBX_NAV_TIMEUTC ubx_nav_timeutc_buffer;
extern UBX_HEADER ubx_nav_timeutc_string;
extern bool ubx_nav_timeutc_waiting;

extern UBX_TIM_TM2 ubx_tim_tm2_buffer;
extern UBX_HEADER ubx_tim_tm2_string;
extern bool ubx_tim_tm2_waiting;

char gnss_rmc_buffer[GNSS_STRING_BUFFER_SIZE] = {0};
char gnss_rmc_string[GNSS_CHECK_BUFFER_SIZE] = "$GNRMC";
bool gnss_rmc_waiting = 0;

bool gnss_rmc_fix = 0;

bool gnss_calendar_sync = 0;

bool gnss_detected = 0;

volatile uint32_t gnss_rx_byte_count = 0;
volatile uint32_t gnss_valid_packet_count = 0;

void gnss_init(void)
{
    UART2_Initialize();
    gnss_rx_reset();
    UART2_SetRxInterruptHandler(gnss_rx);
}

void gnss_rx_reset_message_only(void)
{
    gnss_incoming = GNSS_NONE;
    gnss_waiting = GNSS_NONE;
    gnss_bytes_remaining = 0;
    gnss_offset = 0;

    memset(gnss_string_buffer, 0, GNSS_STRING_BUFFER_SIZE);
}

void gnss_rx_reset(void)
{
    gnss_rx_reset_message_only();

    memset(gnss_check_buffer, 0, GNSS_CHECK_BUFFER_SIZE);

    while(UART2_IsRxReady())
    {
        (void)UART2_Read();
    }
}

static void gnss_append_rx_char(char rx_char);
static void gnss_start_message(GNSS_MESSAGE_TYPE message_type);
static void gnss_copy_buffer(GNSS_MESSAGE_TYPE message);
static GNSS_MESSAGE_TYPE gnss_check_incoming(void);

void gnss_rx(void)
{
    char rx_char = 0;

    while(UART2_IsRxReady())
    {
        rx_char = UART2_Read();
        gnss_rx_byte_count++;

        // Shuffle buffer along
        memmove(gnss_check_buffer,
                gnss_check_buffer + 1,
                GNSS_CHECK_BUFFER_SIZE - 1);

        gnss_check_buffer[GNSS_CHECK_BUFFER_SIZE - 1] = rx_char;

        // Append character if we are in a message
        gnss_append_rx_char(rx_char);

        // Copy out the buffer if we're done
        if(gnss_waiting != GNSS_NONE)
        {
            gnss_copy_buffer(gnss_waiting);

            memset(gnss_string_buffer, 0, GNSS_STRING_BUFFER_SIZE);
            gnss_waiting = GNSS_NONE;
            gnss_offset = 0;
            gnss_bytes_remaining = 0;
        }

        // Check if we have a match for a new message
        GNSS_MESSAGE_TYPE gnss_check_res = gnss_check_incoming();

        if(gnss_check_res != GNSS_NONE)
        {
            gnss_valid_packet_count++;

            // Reset the existing message if we have a new match
            gnss_rx_reset_message_only();

            // Begin assembling new message
            gnss_start_message(gnss_check_res);
        }
    }
}

static void gnss_append_rx_char(char rx_char)
{
    if(gnss_incoming == GNSS_NONE)
    {
        return;
    }

    gnss_detected = 1;

    // bounds check, reset if we're past it
    if(gnss_offset >= GNSS_STRING_BUFFER_SIZE)
    {
        gnss_rx_reset_message_only();
        return;
    }

    gnss_string_buffer[gnss_offset] = rx_char;
    gnss_offset++;

    if(gnss_bytes_remaining > 0)
    {
        gnss_bytes_remaining--;
    }
    else
    {
        // If we end up here, something is wrong, reset and try again
        gnss_rx_reset_message_only();
        return;
    }

    // Message complete, so set flag to copy complete message out of buffer
    if((gnss_bytes_remaining == 0) || (gnss_offset >= (GNSS_STRING_BUFFER_SIZE - 1)))
    {
        gnss_waiting = gnss_incoming;
        gnss_incoming = GNSS_NONE;
    }
}

static void gnss_start_message(GNSS_MESSAGE_TYPE message_type)
{
    gnss_incoming = message_type;

    switch(message_type)
    {
        case GNSS_UBX_NAV_CLOCK:
            gnss_bytes_remaining = sizeof(ubx_nav_clock_buffer) - sizeof(UBX_HEADER);
            gnss_offset = sizeof(UBX_HEADER);
            memcpy(gnss_string_buffer, gnss_check_buffer, GNSS_CHECK_BUFFER_SIZE);
            break;

        case GNSS_UBX_NAV_POSLLH:
            gnss_bytes_remaining = sizeof(ubx_nav_posllh_buffer) - sizeof(UBX_HEADER);
            gnss_offset = sizeof(UBX_HEADER);
            memcpy(gnss_string_buffer, gnss_check_buffer, GNSS_CHECK_BUFFER_SIZE);
            break;

        case GNSS_UBX_NAV_STATUS:
            gnss_bytes_remaining = sizeof(ubx_nav_status_buffer) - sizeof(UBX_HEADER);
            gnss_offset = sizeof(UBX_HEADER);
            memcpy(gnss_string_buffer, gnss_check_buffer, GNSS_CHECK_BUFFER_SIZE);
            break;

        case GNSS_UBX_NAV_TIMEUTC:
            gnss_bytes_remaining = sizeof(ubx_nav_timeutc_buffer) - sizeof(UBX_HEADER);
            gnss_offset = sizeof(UBX_HEADER);
            memcpy(gnss_string_buffer, gnss_check_buffer, GNSS_CHECK_BUFFER_SIZE);
            break;

        case GNSS_UBX_TIM_TM2:
            gnss_bytes_remaining = sizeof(ubx_tim_tm2_buffer) - sizeof(UBX_HEADER);
            gnss_offset = sizeof(UBX_HEADER);
            memcpy(gnss_string_buffer, gnss_check_buffer, GNSS_CHECK_BUFFER_SIZE);
            break;

        case GNSS_GNRMC:
            gnss_bytes_remaining = GNSS_STRING_BUFFER_SIZE - GNSS_CHECK_BUFFER_SIZE;
            gnss_offset = GNSS_CHECK_BUFFER_SIZE;
            memcpy(gnss_string_buffer, gnss_check_buffer, GNSS_CHECK_BUFFER_SIZE);
            break;

        default:
            gnss_rx_reset_message_only();
            break;
    }
}

static void gnss_copy_buffer(GNSS_MESSAGE_TYPE message)
{
    switch (message)
    {
        case GNSS_UBX_NAV_CLOCK:
            memcpy(ubx_nav_clock_buffer.raw, gnss_string_buffer, sizeof(ubx_nav_clock_buffer));
            ubx_nav_clock_waiting=1;
            break;

        case GNSS_UBX_NAV_POSLLH:
            memcpy(ubx_nav_posllh_buffer.raw, gnss_string_buffer, sizeof(ubx_nav_posllh_buffer));
            ubx_nav_posllh_waiting=1;
            break;

        case GNSS_UBX_NAV_STATUS:
            memcpy(ubx_nav_status_buffer.raw, gnss_string_buffer, sizeof(ubx_nav_status_buffer));
            ubx_nav_status_waiting=1;
            break;

        case GNSS_UBX_NAV_TIMEUTC:
            memcpy(ubx_nav_timeutc_buffer.raw, gnss_string_buffer, sizeof(ubx_nav_timeutc_buffer));
            ubx_nav_timeutc_waiting=1;
            break;

        case GNSS_UBX_TIM_TM2:
            memcpy(ubx_tim_tm2_buffer.raw, gnss_string_buffer, sizeof(ubx_tim_tm2_buffer));
            ubx_tim_tm2_waiting=1;
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

static GNSS_MESSAGE_TYPE gnss_check_incoming(void)
{
    if(memcmp(gnss_check_buffer, ubx_tim_tm2_string.raw, sizeof(UBX_HEADER))==0) return GNSS_UBX_TIM_TM2;
    if(memcmp(gnss_check_buffer, ubx_nav_timeutc_string.raw, sizeof(UBX_HEADER))==0) return GNSS_UBX_NAV_TIMEUTC;
    if(memcmp(gnss_check_buffer, ubx_nav_clock_string.raw, sizeof(UBX_HEADER))==0) return GNSS_UBX_NAV_CLOCK;
    if(memcmp(gnss_check_buffer, ubx_nav_status_string.raw, sizeof(UBX_HEADER))==0) return GNSS_UBX_NAV_STATUS;
    if(memcmp(gnss_check_buffer, ubx_nav_posllh_string.raw, sizeof(UBX_HEADER))==0) return GNSS_UBX_NAV_POSLLH;
    if(memcmp(gnss_check_buffer, gnss_rmc_string, GNSS_CHECK_BUFFER_SIZE)==0) return GNSS_GNRMC;
    return GNSS_NONE;
}

void gnss_set_calendar(void)
{
#ifdef DEBUG_MESSAGES
    printf("GNSS calendar sync\nTime is now: ");
    ui_print_iso8601_string(gnss);
    printf("\n");
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
