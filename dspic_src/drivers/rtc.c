#include "rtc.h"

time_t rtc;
extern time_t utc;
bool rtc_sync = 0;
bool rtc_detected = 0;
bool rtc_valid = 0;
#ifdef RTC_SOURCE_DS1307
PIC_RTC_TYPE rtc_type = RTC_DS1307;
#else
    #ifdef RTC_SOURCE_PCF8563
PIC_RTC_TYPE rtc_type = RTC_PCF8563;
    #else
PIC_RTC_TYPE rtc_type = RTC_UNDEFINED;
    #endif
#endif

void rtc_get_calendar(void)
{
#ifdef RTC_SOURCE_DS1307
    rtc = DS1307_read();
#else
    #ifdef RTC_SOURCE_PCF8563
        rtc = PCF8563_read();
    #endif
#endif
    if(rtc) rtc_detected = 1;
}

void rtc_set_calendar(void)
{
#ifdef DEBUG_MESSAGES
    printf("RTC calendar sync\nTime is now: ");
    ui_print_iso8601_string(rtc);
    printf("\n");
#endif
    utc = rtc;
}

void rtc_write_from_calendar(time_t utc)
{
#ifdef RTC_SOURCE_DS1307
    DS1307_write(utc);
#else
    #ifdef RTC_SOURCE_PCF8563
        PCF8563_write(utc);
    #endif
#endif
    rtc_get_calendar();
#ifdef DEBUG_MESSAGES
    printf("RTC time is now: ");
    ui_print_iso8601_string(rtc);
    printf("\n");
#endif
    rtc_sync = 1; 
}

bool rtc_is_calendar_sync(time_t utc)
{
    return utc==rtc;;
}

void rtc_reset_calendar_sync(void)
{
    rtc_sync = 0;
}

void print_rtc_data(void)
{
    if(rtc_detected)
    {
        printf("\n=== RTC ===\n");
#ifdef RTC_SOURCE_DS1307
        printf("DS1307: ");
#else
    #ifdef RTC_SOURCE_PCF8563
        printf("PCF8563: ");
    #endif
#endif
        ui_print_iso8601_string(rtc);
        printf("\nValid: %u Sync: %u\n", rtc_valid, rtc_sync);
    }
    else
    {
        printf("\n=== NO RTC DETECTED ===\n");
    }
}