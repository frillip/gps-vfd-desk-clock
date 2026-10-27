/*
 * What the tubes show, and when.
 *
 * Each epoch, display_prepare() renders the frame for the NEXT second in the
 * selected mode, and display_service() shifts it into the drivers once the
 * epoch pulse has ended (DISPLAY_LOAD_SLOT). The next epoch pulse latches
 * it, so the tubes change exactly on the second, with no software timing in
 * the way:
 *
 *   epoch N   slot 1: display_prepare()  frame for N+1 rendered
 *             slot 100: pulse ends, latch closed
 *             slot 500: display_service() shifts it in (invisible)
 *   epoch N+1 the pulse latches it: N+1 is shown
 *
 * Modes are a table of renderers in display.c: adding one is a renderer and
 * a row. A renderer gets the local time of the second being prepared (UTC
 * plus the time zone offsets in force at that second, timezone.h) and fills
 * a frame with the frame_*() helpers (frame.h).
 *
 * Until the time of day is known, the tubes show dashes.
 */
#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdbool.h>
#include <stdint.h>

#include "../platform/utc.h"
#include "timezone.h"

// Load the prepared frame from this slot of the epoch: after the epoch
// pulse (100 ms) has closed the latch, and well before the next epoch.
#define DISPLAY_LOAD_SLOT 500U

typedef enum
{
    DISPLAY_MODE_HHMM = 0, // 24-hour HH:MM, colon blinking with the seconds
    DISPLAY_MODE_COUNT
} DisplayMode;

// The clock's time of day, as of the epoch just passed (see sync.h).
typedef struct
{
    bool known;        // the time of day is known
    bool synchronised; // ... and disciplined to a source
    uint32_t utc_seconds;
    bool leap_second;
    UtcLeap leap; // announced leap second; change 0 if none
} DisplayClock;

// What a renderer gets: the second being prepared, in local time.
typedef struct
{
    UtcCivil local;
    bool synchronised;
    TimezoneOffset zone; // the offsets that made it local
} DisplayView;

typedef struct
{
    uint32_t prepared;
    uint32_t loaded;
    uint32_t missed; // prepared, but the next was prepared before it was loaded
} DisplayStats;

// Tubes on, showing dashes, mode HHMM.
void display_init(void);

void display_set_mode(DisplayMode mode);
DisplayMode display_mode(void);

// Once per epoch, soon after it: renders the frame for the next epoch.
void display_prepare(const DisplayClock *clock);
// Loads the prepared frame into the tube drivers. Call once per epoch at
// DISPLAY_LOAD_SLOT (a scheduler slot task). Does nothing if there is no new
// frame, or while the epoch pulse holds the latch open.
void display_service(void);

const DisplayStats *display_stats(void);
const char *display_mode_name(DisplayMode mode);
// One line, for diagnostics: the mode, the local offset of the last frame,
// and the frame and tube driver counters.
void display_print(void);

#endif // DISPLAY_H
