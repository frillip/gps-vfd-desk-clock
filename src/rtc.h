/* 
 * File:   rtc.h
 * Author: Frillip
 *
 * Created on May 21, 2023, 9:54 PM
 */

#ifndef RTC_H
#define	RTC_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#define RTC_SOURCE_PCF8563
//#define RTC_SOURCE_DS1307

#ifdef RTC_SOURCE_DS1307
    #include "ds1307.h"
#else
    #ifdef RTC_SOURCE_PCF8563
        #include "pcf8563.h"
    #endif
#endif

#include "ui.h"

#define RTC_DETECT_LIMIT 30
    
void rtc_get_calendar(void);
void rtc_set_calendar(void);
void rtc_write_from_calendar(time_t utc);
bool rtc_is_calendar_sync(void);
void rtc_reset_calendar_sync(void);

#ifdef	__cplusplus
}
#endif

#endif	/* RTC_H */

