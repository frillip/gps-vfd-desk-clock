/*
 * Building tube frames: characters, numbers and blanking, on top of the
 * driver's TubesFrame. Pure functions: no hardware.
 *
 *   TubesFrame frame;
 *   frame_clear(&frame);
 *   frame_number(&frame, 0, 2, hour);   // tubes 0-1
 *   frame_number(&frame, 2, 2, minute); // tubes 2-3
 *   frame.separators |= TUBES_MIDDLE_BOTH;
 *
 * Separators are set directly with the TUBES_START_* / TUBES_MIDDLE_* bits.
 */
#ifndef FRAME_H
#define FRAME_H

#include <stdint.h>

#include "../drivers/tubes.h"

// Segments for a character: 0-9, A-Z (either case, as well as seven
// segments allow), '-' and ' '. Anything else is blank.
uint8_t frame_glyph(char character);

// All tubes and separators off.
void frame_clear(TubesFrame *frame);

// Up to TUBES_COUNT characters from tube 0; tubes past the end are blanked.
void frame_text(TubesFrame *frame, const char *text);

// value in decimal, zero-padded to width tubes from tube first; digits that
// do not fit are dropped from the left.
void frame_number(TubesFrame *frame, uint8_t first, uint8_t width, uint16_t value);

// As frame_number(), with leading zeros blanked (at least one digit shown).
void frame_number_blanked(TubesFrame *frame, uint8_t first, uint8_t width, uint16_t value);

// Blanks count tubes from tube first (leading zeros, flashing fields).
void frame_blank(TubesFrame *frame, uint8_t first, uint8_t count);

// "----" with both lines: no time to show.
void frame_dashes(TubesFrame *frame);

#endif // FRAME_H
