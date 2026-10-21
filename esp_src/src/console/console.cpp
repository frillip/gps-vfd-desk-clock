#include "console.h"

#include <stdarg.h>

#include "../net/telnet.h"

void console_line_clear(ConsoleLine *line)
{
    line->length = 0;
    line->text[0] = '\0';
}


bool console_read_line(ConsoleLine *line, Stream *input, bool echo)
{
    while (input->available())
    {
        char c = (char)input->read();
        char previous = line->previous;

        line->previous = c;
        if (c == '\r' || c == '\n')
        {
            if (c == '\n' && previous == '\r')
            {
                continue; // the LF of a CR LF
            }
            if (echo)
            {
                input->print("\r\n");
            }
            line->text[line->length] = '\0';
            return true;
        }
        if (c == 0x7F || c == '\b')
        {
            if (line->length > 0)
            {
                line->length--;
                if (echo)
                {
                    input->print("\b \b");
                }
            }
            continue;
        }
        if ((uint8_t)c < 0x20 || (uint8_t)c >= 0x7F)
        {
            continue; // other control characters, telnet negotiation, non-ASCII
        }
        if (line->length >= CONSOLE_LINE_BYTES - 1)
        {
            input->print("\r\nLine too long\r\n> ");
            console_line_clear(line);
            continue;
        }
        line->text[line->length++] = c;
        if (echo)
        {
            input->print(c);
        }
    }
    return false;
}


void console_write_text(const uint8_t *text, size_t length)
{
    Serial.write(text, length);
    telnet_write_text(text, length);
}


void console_printf(const char *format, ...)
{
    char text[256];
    va_list arguments;
    int length;

    va_start(arguments, format);
    length = vsnprintf(text, sizeof(text), format, arguments);
    va_end(arguments);
    if (length > 0)
    {
        console_write_text((const uint8_t *)text,
                           (size_t)length < sizeof(text) ? (size_t)length : sizeof(text) - 1);
    }
}
