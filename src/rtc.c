#include "rtc.h"

time_t rtc;
extern time_t utc;
bool rtc_sync = 0;

void get_rtc_calendar(void)
{
    //rtc = DS1307_read();
    rtc = PCF8563_read();
    printf("RTC time is: ");
    ui_print_iso8601_string(rtc);
    printf("\r\n");
}

void set_from_rtc_calendar(void)
{
    //utc = DS1307_read();
    utc = PCF8563_read();
}

void sync_rtc_calendar(time_t utc)
{
    //DS1307_write(utc);
    //rtc_sync = 1; 
}

bool is_rtc_calendar_sync(void)
{
    return 0;//rtc_sync;
}

void reset_rtc_calendar_sync(void)
{
    rtc_sync = 0;
}
