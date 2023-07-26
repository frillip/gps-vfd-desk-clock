#include "ui.h"

void print_iso8601_string(time_t iso)
{
    char buf[32] = {0}; // Allocate buffer
    struct tm *iso_time; // Allocate buffer
    iso_time = gmtime(&iso);
    strftime(buf, 32, "%Y-%m-%dT%H:%M:%SZ", iso_time);
    printf(buf);
}