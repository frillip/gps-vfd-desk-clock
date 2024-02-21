#include "rtc.h"

time_t rtc;
extern time_t utc;
bool rtc_sync = 0;
bool rtc_detected = 0;

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
    printf("RTC time is: ");
    ui_print_iso8601_string(rtc);
    printf("\r\n");
}

void rtc_set_calendar(void)
{
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
    rtc = utc;
    printf("RTC time is now: ");
    ui_print_iso8601_string(rtc);
    printf("\r\n");
    rtc_sync = 1; 
}

bool rtc_is_calendar_sync(void)
{
    return rtc_sync;
}

void rtc_reset_calendar_sync(void)
{
    rtc_sync = 0;
}