#include "debug.h"

#include "text.h"

static const char *const names[] = {"TIMING", "GNSS", "SYNC", "SOURCES", "NTP", "MINUTE", "TUNING"};

static uint16_t mask = DEBUG_DEFAULT;

void debug_init(void)
{
    mask = DEBUG_DEFAULT;
}


bool debug_enabled(uint16_t subsystems)
{
    return (mask & subsystems) != 0U;
}


uint16_t debug_mask(void)
{
    return mask;
}


void debug_set(uint16_t new_mask)
{
    mask = new_mask & DEBUG_ALL;
}


void debug_print(void)
{
    uint8_t bit;
    TextLine line;

    text_start(&line);
    text_str(&line, "PIC debug 0x");
    text_hex16(&line, mask);
    text_char(&line, ':');
    for (bit = 0; bit < sizeof(names) / sizeof(names[0]); bit++)
    {
        if ((mask & (1U << bit)) != 0U)
        {
            text_char(&line, ' ');
            text_str(&line, names[bit]);
        }
    }
    text_str(&line, mask == 0U ? " none\r\n" : "\r\n");
    text_send(&line);
}
