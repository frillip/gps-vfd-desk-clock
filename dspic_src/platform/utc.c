#include "utc.h"

#include <stddef.h>

// 1970-01-01 is day 719468 of the era-based count from 0000-03-01.
#define DAYS_TO_1970            ((uint32_t)719468)
#define DAYS_PER_ERA            ((uint32_t)146097)
#define LAST_SECOND_OF_LAST_DAY ((uint32_t)6 * 3600U + 28U * 60U + 15U) // 06:28:15

uint32_t utc_advance(uint32_t label, bool label_is_leap, uint32_t seconds, const UtcLeap *leap,
                     bool *is_leap)
{
    uint32_t result = label + seconds;
    bool result_is_leap = label_is_leap && seconds == 0U;

    if (leap != NULL && seconds > 0U)
    {
        uint32_t event = leap->event_utc;

        // Inserted: the first label to reach the event repeats E-1 as the
        // leap second, and everything after it is one behind. Counting from
        // the leap second itself is already behind.
        if (leap->change > 0 && !label_is_leap && label < event && result >= event)
        {
            result_is_leap = result == event;
            result -= 1U;
        }
        // Deleted: E-1 never happens; everything from there is one ahead.
        else if (leap->change < 0 && label < event - 1U && result >= event - 1U)
        {
            result += 1U;
        }
    }
    if (is_leap != NULL)
    {
        *is_leap = result_is_leap;
    }
    return result;
}


bool utc_leap_snap(uint32_t estimated_utc, uint32_t tolerance_s, uint32_t *event_utc)
{
    uint32_t into_day = estimated_utc % UTC_SECONDS_PER_DAY;
    uint32_t midnight = estimated_utc - into_day;

    if (into_day > UTC_SECONDS_PER_DAY / 2U)
    {
        // Nearer the next midnight, which must itself be representable.
        if (UTC_SECONDS_PER_DAY - into_day > tolerance_s ||
            midnight / UTC_SECONDS_PER_DAY > UTC_LAST_DAY)
        {
            return false;
        }
        midnight += UTC_SECONDS_PER_DAY;
    }
    else if (into_day > tolerance_s)
    {
        return false;
    }
    if (event_utc != NULL)
    {
        *event_utc = midnight;
    }
    return true;
}


uint16_t utc_days_from_civil(uint16_t year, uint8_t month, uint8_t day)
{
    // Years counted from March, so the leap day ends the year. From 1970 the
    // era (400 years) is never negative: everything is unsigned.
    uint32_t y = (uint32_t)year - (month <= 2U ? 1U : 0U);
    uint32_t era = y / 400U;
    uint32_t year_of_era = y - era * 400U;
    uint32_t day_of_year = (153U * (month > 2U ? month - 3U : month + 9U) + 2U) / 5U + day - 1U;
    uint32_t day_of_era = year_of_era * 365U + year_of_era / 4U - year_of_era / 100U + day_of_year;

    return (uint16_t)(era * DAYS_PER_ERA + day_of_era - DAYS_TO_1970);
}


void utc_civil_from_days(uint16_t days, UtcCivil *civil)
{
    uint32_t z = (uint32_t)days + DAYS_TO_1970;
    uint32_t era = z / DAYS_PER_ERA;
    uint32_t day_of_era = z - era * DAYS_PER_ERA;
    uint32_t year_of_era =
        (day_of_era - day_of_era / 1460U + day_of_era / 36524U - day_of_era / 146096U) / 365U;
    uint32_t day_of_year =
        day_of_era - (365U * year_of_era + year_of_era / 4U - year_of_era / 100U);
    uint32_t month_index = (5U * day_of_year + 2U) / 153U; // from March
    uint32_t month = month_index < 10U ? month_index + 3U : month_index - 9U;

    civil->year = (uint16_t)(year_of_era + era * 400U + (month <= 2U ? 1U : 0U));
    civil->month = (uint8_t)month;
    civil->day = (uint8_t)(day_of_year - (153U * month_index + 2U) / 5U + 1U);
}


void utc_to_civil(uint32_t utc_seconds, bool leap_second, UtcCivil *civil)
{
    uint32_t second_of_day = utc_seconds % UTC_SECONDS_PER_DAY;
    uint16_t minute_of_day = (uint16_t)(second_of_day / 60U);

    utc_civil_from_days((uint16_t)(utc_seconds / UTC_SECONDS_PER_DAY), civil);
    civil->hour = (uint8_t)(minute_of_day / 60U);
    civil->minute = (uint8_t)(minute_of_day % 60U);
    civil->second = (uint8_t)(leap_second ? 60U : second_of_day % 60U);
}


uint32_t utc_from_civil(const UtcCivil *civil)
{
    uint32_t days = utc_days_from_civil(civil->year, civil->month, civil->day);
    uint32_t second = civil->second > 59U ? 59U : civil->second;

    return days * UTC_SECONDS_PER_DAY + (uint32_t)civil->hour * 3600U +
           (uint32_t)civil->minute * 60U + second;
}


static uint8_t days_in_month(uint16_t year, uint8_t month)
{
    static const uint8_t lengths[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    bool leap_year = (year % 4U == 0U && year % 100U != 0U) || year % 400U == 0U;

    return (uint8_t)(lengths[month - 1U] + (month == 2U && leap_year ? 1U : 0U));
}


bool utc_civil_is_valid(const UtcCivil *civil)
{
    uint32_t second_of_day;

    if (civil->year < 1970U || civil->year > UTC_LAST_YEAR || civil->month < 1U ||
        civil->month > 12U || civil->day < 1U ||
        civil->day > days_in_month(civil->year, civil->month) || civil->hour > 23U ||
        civil->minute > 59U || civil->second > 60U)
    {
        return false;
    }
    if (civil->year < UTC_LAST_YEAR || civil->month == 1U ||
        (civil->month == 2U && civil->day < 7U))
    {
        return true;
    }
    second_of_day = (uint32_t)civil->hour * 3600U + (uint32_t)civil->minute * 60U + civil->second;
    return civil->month == 2U && civil->day == 7U && second_of_day <= LAST_SECOND_OF_LAST_DAY;
}
