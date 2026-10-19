/*
 * UTC second arithmetic and the calendar.
 *
 * Times are Unix seconds held as uint32_t, as everywhere in the clock and on
 * the ESP link: valid from 1970 to 2106-02-07T06:28:15Z, with no 64-bit
 * arithmetic. Days since 1970 fit in a uint16_t.
 *
 * A leap second has no Unix representation: an inserted 23:59:60 is held as
 * 23:59:59 and flagged, so the count repeats one second across the event; a
 * deleted 23:59:59 is skipped. The event is identified by event_utc, the
 * Unix time of the midnight that follows it (the first second at which the
 * new TAI-UTC applies, as SML-TIME-LEAP and SML-TIME-GNSS give it).
 *
 *   inserted:  E-2  E-1  E-1 (leap)  E  E+1     (23:59:58 59 60 00:00:00 01)
 *   deleted:   E-2  E                           (23:59:58 00:00:00)
 */
#ifndef UTC_H
#define UTC_H

#include <stdbool.h>
#include <stdint.h>

#define UTC_SECONDS_PER_DAY ((uint32_t)86400)
// The last day whose every second is representable: 2106-02-06.
#define UTC_LAST_DAY  49709U
#define UTC_LAST_YEAR 2106U

// An announced leap second. change 0: none known.
typedef struct
{
    uint32_t event_utc; // midnight after the event, Unix seconds
    int8_t change;      // +1 inserted, -1 deleted
} UtcLeap;

// The label `seconds` whole seconds after `label` (itself flagged leap or
// not), applying leap if the interval crosses it. *is_leap, if not NULL,
// says whether the result is an inserted 23:59:60. leap may be NULL.
uint32_t utc_advance(uint32_t label, bool label_is_leap, uint32_t seconds, const UtcLeap *leap,
                     bool *is_leap);

typedef struct
{
    uint16_t year;
    uint8_t month; // 1..12
    uint8_t day;   // 1..31
    uint8_t hour;
    uint8_t minute;
    uint8_t second; // 60 for an inserted leap second
} UtcCivil;

// H. Hinnant's days_from_civil and civil_from_days, for 1970-01-01 (day 0)
// to 2106-02-07 (proleptic Gregorian). The date must be valid and in range.
uint16_t utc_days_from_civil(uint16_t year, uint8_t month, uint8_t day);
void utc_civil_from_days(uint16_t days, UtcCivil *civil);

// Calendar date and time of a Unix second.
void utc_to_civil(uint32_t utc_seconds, bool leap_second, UtcCivil *civil);
// Unix seconds of a calendar time, which must lie in 1970..2106-02-07T06:28:15.
// A second of 60 counts as 59 (the caller flags the leap second).
uint32_t utc_from_civil(const UtcCivil *civil);
// True if civil is a real date and time (second up to 60) within that range.
bool utc_civil_is_valid(const UtcCivil *civil);

// Leap seconds only ever fall at the end of a UTC day: snaps an estimated
// event time to the nearest midnight. False if the estimate is more than
// tolerance_s from one, which means it does not describe a real event.
bool utc_leap_snap(uint32_t estimated_utc, uint32_t tolerance_s, uint32_t *event_utc);

#endif // UTC_H
