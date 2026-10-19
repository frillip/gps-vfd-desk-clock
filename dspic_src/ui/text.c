#include "text.h"

#include <stddef.h>

#include "console.h"

static const uint32_t powers_of_ten[10] = {
    1000000000UL, 100000000UL, 10000000UL, 1000000UL, 100000UL, 10000UL, 1000UL, 100UL, 10UL, 1UL};

void text_start(TextLine *line)
{
    line->length = 0;
}


void text_char(TextLine *line, char c)
{
    if (line->length < TEXT_LINE_CAPACITY)
    {
        line->text[line->length++] = c;
    }
}


void text_str(TextLine *line, const char *s)
{
    while (*s != '\0')
    {
        text_char(line, *s++);
    }
}


// Decimal digits of value into digits[], most significant first; returns
// how many (at least one).
static uint8_t decimal(uint32_t value, char digits[10])
{
    uint8_t count = 0;
    uint8_t index;

    for (index = 0; index < 10U; index++)
    {
        uint32_t power = powers_of_ten[index];
        char digit = '0';

        while (value >= power)
        {
            value -= power;
            digit++;
        }
        if (count != 0U || digit != '0' || index == 9U)
        {
            digits[count++] = digit;
        }
    }
    return count;
}


static void pad_to(TextLine *line, uint8_t used, uint8_t width, char pad)
{
    while (used < width)
    {
        text_char(line, pad);
        used++;
    }
}


void text_u32(TextLine *line, uint32_t value, uint8_t width, char pad)
{
    char digits[10];
    uint8_t count = decimal(value, digits);
    uint8_t index;

    pad_to(line, count, width, pad);
    for (index = 0; index < count; index++)
    {
        text_char(line, digits[index]);
    }
}


void text_i32(TextLine *line, int32_t value, uint8_t width, bool plus)
{
    char digits[10];
    bool negative = value < 0;
    uint32_t magnitude = negative ? (uint32_t)0 - (uint32_t)value : (uint32_t)value;
    uint8_t count = decimal(magnitude, digits);
    char sign = negative ? '-' : plus ? '+' : '\0';
    uint8_t index;

    pad_to(line, (uint8_t)(count + (sign != '\0' ? 1U : 0U)), width, ' ');
    if (sign != '\0')
    {
        text_char(line, sign);
    }
    for (index = 0; index < count; index++)
    {
        text_char(line, digits[index]);
    }
}


void text_hex16(TextLine *line, uint16_t value)
{
    static const char hex[] = "0123456789ABCDEF";
    int8_t shift;

    for (shift = 12; shift >= 0; shift -= 4)
    {
        text_char(line, hex[(value >> shift) & 0xFU]);
    }
}


void text_send(TextLine *line)
{
    console_write(line->text, line->length);
    line->length = 0;
}
