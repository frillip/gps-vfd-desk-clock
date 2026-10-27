#include "parse.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static bool has_text(const char *text)
{
    return text != NULL && *text != '\0' && !isspace((unsigned char)*text);
}

bool parse_u32(const char *text, uint32_t minimum, uint32_t maximum, uint32_t *value)
{
    char *end;
    unsigned long long parsed;

    if (!has_text(text) || *text == '-')
    {
        return false;
    }
    errno = 0;
    parsed = strtoull(text, &end, 10);
    if (errno != 0 || *end != '\0' || parsed < minimum || parsed > maximum)
    {
        return false;
    }
    *value = (uint32_t)parsed;
    return true;
}

bool parse_i32(const char *text, int32_t minimum, int32_t maximum, int32_t *value)
{
    char *end;
    long long parsed;

    if (!has_text(text))
    {
        return false;
    }
    errno = 0;
    parsed = strtoll(text, &end, 10);
    if (errno != 0 || *end != '\0' || parsed < minimum || parsed > maximum)
    {
        return false;
    }
    *value = (int32_t)parsed;
    return true;
}

bool parse_bool(const char *text, bool *value)
{
    static const char *const yes[] = {"1", "true", "on", "yes"};
    static const char *const no[] = {"0", "false", "off", "no"};
    unsigned index;

    if (text == NULL)
    {
        return false;
    }
    for (index = 0; index < 4; index++)
    {
        if (strcasecmp(text, yes[index]) == 0)
        {
            *value = true;
            return true;
        }
        if (strcasecmp(text, no[index]) == 0)
        {
            *value = false;
            return true;
        }
    }
    return false;
}

// Digits only, at least one, at most max_digits; *end left after them.
static bool digits(const char **text, unsigned max_digits, uint32_t *value)
{
    const char *start = *text;
    uint32_t parsed = 0;

    while (isdigit((unsigned char)**text))
    {
        if ((unsigned)(*text - start) == max_digits)
        {
            return false;
        }
        parsed = parsed * 10U + (uint32_t)(**text - '0');
        (*text)++;
    }
    *value = parsed;
    return *text != start;
}

// h[:mm[:ss]] from *text: hours of at most two digits, minutes and seconds
// of exactly two and below 60. *text left after it.
static bool clock_fields(const char **text, uint32_t *seconds)
{
    uint32_t hours;
    uint32_t minutes = 0;
    uint32_t secs = 0;
    const char *start;

    if (!digits(text, 2, &hours))
    {
        return false;
    }
    if (**text == ':')
    {
        (*text)++;
        start = *text;
        if (!digits(text, 2, &minutes) || *text - start != 2 || minutes > 59)
        {
            return false;
        }
        if (**text == ':')
        {
            (*text)++;
            start = *text;
            if (!digits(text, 2, &secs) || *text - start != 2 || secs > 59)
            {
                return false;
            }
        }
    }
    *seconds = hours * 3600U + minutes * 60U + secs;
    return true;
}

bool parse_offset(const char *text, int32_t limit_s, int32_t *offset_s)
{
    bool negative = false;
    uint32_t magnitude;
    size_t length;

    if (!has_text(text))
    {
        return false;
    }
    length = strlen(text);
    if (text[length - 1] == 's' || text[length - 1] == 'S')
    {
        char number[16];
        int32_t value;

        if (length >= sizeof(number))
        {
            return false;
        }
        memcpy(number, text, length - 1);
        number[length - 1] = '\0';
        if (!parse_i32(number, -limit_s, limit_s, &value))
        {
            return false;
        }
        *offset_s = value;
        return true;
    }
    if (*text == '+' || *text == '-')
    {
        negative = *text == '-';
        text++;
    }
    if (!clock_fields(&text, &magnitude) || *text != '\0' || magnitude > (uint32_t)limit_s)
    {
        return false;
    }
    *offset_s = negative ? -(int32_t)magnitude : (int32_t)magnitude;
    return true;
}

bool parse_time_of_day(const char *text, uint32_t *seconds)
{
    uint32_t value;

    if (!has_text(text) || strchr(text, ':') == NULL || !clock_fields(&text, &value) ||
        *text != '\0' || value >= 86400U)
    {
        return false;
    }
    *seconds = value;
    return true;
}

// Days from 1970-01-01 to a date in the proleptic Gregorian calendar.
static int64_t days_from_civil(int64_t year, uint32_t month, uint32_t day)
{
    int64_t era;
    uint32_t year_of_era;
    uint32_t day_of_year;
    uint32_t day_of_era;

    year -= month <= 2 ? 1 : 0;
    era = (year >= 0 ? year : year - 399) / 400;
    year_of_era = (uint32_t)(year - era * 400);
    day_of_year = (153U * (month > 2 ? month - 3U : month + 9U) + 2U) / 5U + day - 1U;
    day_of_era = year_of_era * 365U + year_of_era / 4U - year_of_era / 100U + day_of_year;
    return era * 146097 + (int64_t)day_of_era - 719468;
}

static uint32_t days_in_month(uint32_t year, uint32_t month)
{
    static const uint8_t days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    bool leap = (year % 4U == 0U && year % 100U != 0U) || year % 400U == 0U;

    return month == 2U && leap ? 29U : days[month - 1U];
}

bool parse_moment(const char *text, uint32_t *utc_seconds)
{
    uint32_t year;
    uint32_t month;
    uint32_t day;
    uint32_t clock = 0;
    int32_t offset = 0;
    const char *start = text;
    int64_t seconds;

    if (!has_text(text))
    {
        return false;
    }
    if (strchr(text, '-') == NULL || *text == '-')
    {
        return parse_u32(text, 0, UINT32_MAX, utc_seconds); // Unix seconds
    }
    if (!digits(&text, 4, &year) || text - start != 4 || *text++ != '-')
    {
        return false;
    }
    start = text;
    if (!digits(&text, 2, &month) || text - start != 2 || *text++ != '-')
    {
        return false;
    }
    start = text;
    if (!digits(&text, 2, &day) || text - start != 2 || month < 1 || month > 12 || day < 1 ||
        day > days_in_month(year, month))
    {
        return false;
    }
    if (*text == 'T' || *text == 't' || *text == ' ')
    {
        text++;
        if (!clock_fields(&text, &clock) || clock >= 86400U)
        {
            return false;
        }
    }
    if (*text == 'Z' || *text == 'z')
    {
        text++;
    }
    else if (*text == '+' || *text == '-')
    {
        if (!parse_offset(text, 86399, &offset))
        {
            return false;
        }
        text += strlen(text);
    }
    if (*text != '\0')
    {
        return false;
    }
    seconds = days_from_civil(year, month, day) * 86400 + clock - offset;
    if (seconds < 0 || seconds > (int64_t)UINT32_MAX)
    {
        return false;
    }
    *utc_seconds = (uint32_t)seconds;
    return true;
}

bool parse_duration(const char *text, uint32_t minimum, uint32_t maximum, uint32_t *seconds)
{
    static const struct
    {
        char suffix;
        uint32_t scale;
    } units[] = {{'s', 1}, {'m', 60}, {'h', 3600}, {'d', 86400}};
    char number[16];
    size_t length;
    uint32_t scale = 1;
    uint32_t value;
    unsigned index;

    if (!has_text(text) || (length = strlen(text)) >= sizeof(number))
    {
        return false;
    }
    memcpy(number, text, length + 1);
    for (index = 0; index < sizeof(units) / sizeof(units[0]); index++)
    {
        if (tolower((unsigned char)number[length - 1]) == units[index].suffix)
        {
            scale = units[index].scale;
            number[length - 1] = '\0';
            break;
        }
    }
    if (!parse_u32(number, 0, UINT32_MAX / scale, &value) || value * scale < minimum ||
        value * scale > maximum)
    {
        return false;
    }
    *seconds = value * scale;
    return true;
}
