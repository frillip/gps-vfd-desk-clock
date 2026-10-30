#include "frame.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#define A TUBES_SEGMENT_A
#define B TUBES_SEGMENT_B
#define C TUBES_SEGMENT_C
#define D TUBES_SEGMENT_D
#define E TUBES_SEGMENT_E
#define F TUBES_SEGMENT_F
#define G TUBES_SEGMENT_G

static const uint8_t digits[10] = {
    A | B | C | D | E | F,     // 0
    B | C,                     // 1
    A | B | D | E | G,         // 2
    A | B | C | D | G,         // 3
    B | C | F | G,             // 4
    A | C | D | F | G,         // 5
    A | C | D | E | F | G,     // 6
    A | B | C,                 // 7
    A | B | C | D | E | F | G, // 8
    A | B | C | D | F | G,     // 9
};

// As V2's, so menus read the same.
static const uint8_t letters[26] = {
    A | B | C | E | F | G, // A
    C | D | E | F | G,     // b
    A | D | E | F,         // C
    B | C | D | E | G,     // d
    A | D | E | F | G,     // E
    A | E | F | G,         // F
    A | C | D | E | F,     // G
    B | C | E | F | G,     // H
    E | F,                 // I
    B | C | D | E,         // J
    A | C | E | F | G,     // K
    D | E | F,             // L
    A | C | E,             // M
    A | B | C | E | F,     // N
    A | B | C | D | E | F, // O
    A | B | E | F | G,     // P
    A | B | C | F | G,     // q
    A | B | E | F,         // R
    A | C | D | F | G,     // S
    D | E | F | G,         // t
    B | C | D | E | F,     // U
    B | C | D | F,         // V
    B | D | F,             // W
    B | C | E | F | G,     // X
    B | C | D | F | G,     // y
    A | B | D | E | G,     // Z
};

uint8_t frame_glyph(char character)
{
    if (character >= '0' && character <= '9')
    {
        return digits[character - '0'];
    }
    if (character >= 'A' && character <= 'Z')
    {
        return letters[character - 'A'];
    }
    if (character >= 'a' && character <= 'z')
    {
        return letters[character - 'a'];
    }
    if (character == '-')
    {
        return G;
    }
    return 0;
}


void frame_clear(TubesFrame *frame)
{
    memset(frame, 0, sizeof(*frame));
}


void frame_text(TubesFrame *frame, const char *text)
{
    uint8_t tube;
    bool ended = text == NULL;

    for (tube = 0; tube < TUBES_COUNT; tube++)
    {
        if (!ended && text[tube] == '\0')
        {
            ended = true;
        }
        frame->segments[tube] = ended ? 0U : frame_glyph(text[tube]);
    }
}


void frame_number(TubesFrame *frame, uint8_t first, uint8_t width, uint16_t value)
{
    uint8_t place = width;

    while (place > 0U)
    {
        place--;
        if ((uint16_t)(first + place) < TUBES_COUNT)
        {
            frame->segments[first + place] = digits[value % 10U];
        }
        value /= 10U;
    }
}


void frame_number_blanked(TubesFrame *frame, uint8_t first, uint8_t width, uint16_t value)
{
    uint8_t shown = 1;
    uint16_t rest = value;

    while (rest >= 10U)
    {
        rest /= 10U;
        shown++;
    }
    frame_number(frame, first, width, value);
    if (shown < width)
    {
        frame_blank(frame, first, (uint8_t)(width - shown));
    }
}


void frame_blank(TubesFrame *frame, uint8_t first, uint8_t count)
{
    uint8_t tube;

    for (tube = first; tube < TUBES_COUNT && tube < (uint16_t)(first + count); tube++)
    {
        frame->segments[tube] = 0;
    }
}


void frame_dashes(TubesFrame *frame)
{
    frame_text(frame, "----");
    frame->separators = TUBES_START_LINE | TUBES_MIDDLE_LINE;
}
