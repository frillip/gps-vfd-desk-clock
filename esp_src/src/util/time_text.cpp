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


void time_print_ns(Stream *output, int64_t ns)
{
    int64_t magnitude = ns < 0 ? -ns : ns;

    if (magnitude >= 1000000000LL)
    {
        output->printf("%.6f s", (float)ns / 1e9f);
    }
    else if (magnitude >= 1000000)
    {
        output->printf("%.3f ms", (float)ns / 1e6f);
    }
    else if (magnitude >= 1000)
    {
        output->printf("%.1f us", (float)ns / 1e3f);
    }
    else
    {
        output->printf("%ld ns", (long)ns);
    }
}
