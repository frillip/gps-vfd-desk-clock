/*
 * Fast text formatting for the console reports.
 *
 * XC16's printf costs about 1.5 ms a call on this part (measured: the five
 * per-epoch report lines took 67 ms of CPU whatever the baud rate), so the
 * lines printed every epoch are built here instead and written in one go
 * with console_write(). Numbers are converted by subtracting powers of ten:
 * no division, a few microseconds a number.
 *
 * Each function appends to a TextLine and silently stops at its capacity.
 * The formats match printf's: text_u32(line, v, 5, '0') is "%05lu",
 * text_u32(line, v, 10, ' ') is "%10lu", text_i32(line, v, 4, false) is
 * "%4ld", text_i32(line, v, 0, true) is "%+ld", text_hex16 is "%04X".
 */
#ifndef TEXT_H
#define TEXT_H

#include <stdbool.h>
#include <stdint.h>

#define TEXT_LINE_CAPACITY 200U

typedef struct
{
    char text[TEXT_LINE_CAPACITY];
    uint16_t length;
} TextLine;

void text_start(TextLine *line);
void text_char(TextLine *line, char c);
void text_str(TextLine *line, const char *s);
// At least width characters, padded on the left with pad (' ' or '0').
void text_u32(TextLine *line, uint32_t value, uint8_t width, char pad);
// At least width characters, padded with spaces; plus: '+' for positive.
void text_i32(TextLine *line, int32_t value, uint8_t width, bool plus);
// Four upper-case hex digits.
void text_hex16(TextLine *line, uint16_t value);
// Writes the line to the console (console_write) and starts it afresh.
void text_send(TextLine *line);

#endif // TEXT_H
