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

//#include "ds1307.h"
#include "pcf8563.h"

void get_rtc_calendar(void);
void set_from_rtc_calendar(void);
void sync_rtc_calendar(time_t utc);
bool is_rtc_calendar_sync(void);
void reset_rtc_calendar_sync(void);

#ifdef	__cplusplus
}
#endif

#endif	/* RTC_H */

