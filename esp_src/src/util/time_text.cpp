#include "time_text.h"

#include <time.h>

static void print_civil(Stream *output, uint32_t seconds)
{
    time_t value = (time_t)seconds;
    struct tm civil;
    char text[24];

    gmtime_r(&value, &civil);
    strftime(text, sizeof(text), "%Y-%m-%dT%H:%M:%S", &civil);
    output->print(text);
}


void time_print_utc(Stream *output, uint32_t utc_seconds)
{
    print_civil(output, utc_seconds);
    output->print('Z');
}


void time_print_wall(Stream *output, uint32_t local_seconds)
{
    print_civil(output, local_seconds);
}


void time_print_local(Stream *output, uint32_t utc_seconds, int32_t offset_s)
{
    print_civil(output, (uint32_t)((int64_t)utc_seconds + offset_s));
    time_print_offset(output, offset_s);
}


void time_print_offset(Stream *output, int32_t offset_s)
{
    uint32_t magnitude = offset_s < 0 ? (uint32_t) - (int64_t)offset_s : (uint32_t)offset_s;

    output->printf("%c%02lu:%02lu", offset_s < 0 ? '-' : '+', (unsigned long)(magnitude / 3600),
                   (unsigned long)(magnitude / 60 % 60));
    if (magnitude % 60 != 0)
    {
        output->printf(":%02lu", (unsigned long)(magnitude % 60));
    }
}


void time_print_duration(Stream *output, uint32_t seconds)
{
    if (seconds >= 86400)
    {
        output->printf("%lud ", (unsigned long)(seconds / 86400));
    }
    output->printf("%02lu:%02lu:%02lu", (unsigned long)(seconds / 3600 % 24),
                   (unsigned long)(seconds / 60 % 60), (unsigned long)(seconds % 60));
}


void time_format_ns(char *text, int64_t ns, bool plus)
{
    static const char *const units[] = {"ns", "us", "ms", "s"};
    uint64_t size = (uint64_t)(ns < 0 ? -ns : ns);
    const char *sign = ns < 0 ? "-" : (plus ? "+" : "");
    unsigned unit = 0;
    uint64_t scale = 1;
    unsigned decimals;
    uint64_t power;
    uint64_t rounded;
    uint64_t whole;

    while (unit < 3 && size >= scale * 1000U)
    {
        scale *= 1000U;
        unit++;
    }
    if (unit == 0)
    {
        snprintf(text, TIME_NS_TEXT, "%s%lu ns", sign, (unsigned long)size);
        return;
    }
    // Four figures: as many decimals as the whole part leaves; rounding may
    // carry into another digit (9.9996 ms is 10.00 ms) or the next unit
    // (999.96 us is 1.000 ms).
    whole = size / scale;
    decimals = whole >= 1000U ? 0 : whole >= 100U ? 1 : whole >= 10U ? 2 : 3;
    for (;;)
    {
        unsigned digits;

        power = decimals == 3 ? 1000U : decimals == 2 ? 100U : decimals == 1 ? 10U : 1U;
        rounded = (size * power + scale / 2U) / scale;
        whole = rounded / power;
        if (whole >= 1000U && unit < 3)
        {
            scale *= 1000U;
            unit++;
            decimals = 3;
            continue;
        }
        digits = whole >= 100U ? 3 : whole >= 10U ? 2 : 1;
        if (decimals > 0 && digits + decimals > 4)
        {
            decimals--;
            continue;
        }
        break;
    }
    if (decimals == 0)
    {
        snprintf(text, TIME_NS_TEXT, "%s%llu %s", sign, (unsigned long long)whole, units[unit]);
    }
    else
    {
        snprintf(text, TIME_NS_TEXT, "%s%llu.%0*llu %s", sign, (unsigned long long)whole,
                 (int)decimals, (unsigned long long)(rounded % power), units[unit]);
    }
}


void time_print_ns(Stream *output, int64_t ns)
{
    char text[TIME_NS_TEXT];

    time_format_ns(text, ns, false);
    output->printf("%s", text);
}
