/*
 * What the tubes show, and when.
 *
 * Each epoch, display_prepare() renders the frame for the NEXT second in the
 * mode shown, and display_service() shifts it into the drivers once the
 * epoch pulse has ended (DISPLAY_LOAD_SLOT). The next epoch pulse latches
 * it, so the tubes change exactly on the second, with no software timing in
 * the way:
 *
 *   epoch N   slot 1: display_prepare()  frame for N+1 rendered
 *             slot 100: pulse ends, latch closed
 *             slot 500: display_service() shifts it in (invisible)
 *   epoch N+1 the pulse latches it: N+1 is shown
 *
 * Two things do not wait for the second, and are shown at once with
 * tubes_show(): a change of mode (the button, or a setting), which redraws
 * the second on the tubes and the one prepared; and SSMM's hundredths,
 * which display_tick() redraws every 10 ms.
 *
 * Modes are a table of renderers in display.c: adding one is a renderer and
 * a row. A renderer gets the second being shown (UTC, and local time: UTC
 * plus the time zone offsets in force at that second, timezone.h) and fills
 * a frame with the frame_*() helpers (frame.h).
 *
 * The mode selected (a setting) is the one shown. A short press of the
 * button shows the next mode instead, and after DISPLAY_CYCLE_EPOCHS
 * without another the selected mode comes back, as V2.
 *
 * Until the time of day is known, the tubes show dashes, whatever the mode.
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

// A mode shown by the button goes back to the selected one after this many
// seconds without another press (V2: 20 s).
#define DISPLAY_CYCLE_EPOCHS 20U

// The largest bound the uncertainty mode shows ("999m"); above it, or with
// no bound, " --m".
#define DISPLAY_UNCERTAINTY_MAX_NS 999000000UL

// In the order the button steps through them.
typedef enum
{
    DISPLAY_MODE_HHMM = 0,    // HH:MM local, 24 or 12 hours; colon blinks while synchronised
    DISPLAY_MODE_MMSS,        // MM:SS local
    DISPLAY_MODE_SSMM,        // SS:hh, seconds and hundredths
    DISPLAY_MODE_YYYY,        // the year
    DISPLAY_MODE_MMDD,        // month and day
    DISPLAY_MODE_DELTA,       // time to or since the delta epoch (UTC or local), two units
    DISPLAY_MODE_UNCERTAINTY, // the time's +/- bound: " 89n", " 1.5u", "130u", "12.6m"
    DISPLAY_MODE_COUNT
} DisplayMode;

// The clock's time of day, as of the epoch just passed (see sync.h).
typedef struct
{
    bool known;        // the time of day is known
    bool synchronised; // ... and disciplined to a source
    uint32_t utc_seconds;
    bool leap_second;
    UtcLeap leap;           // announced leap second; change 0 if none
    bool uncertainty_valid; // the time has a bound (sync_uncertainty())
    uint32_t uncertainty_ns;
} DisplayClock;

// What a renderer gets: the second being shown.
typedef struct
{
    UtcCivil local;
    uint32_t utc_seconds;
    bool synchronised;
    TimezoneOffset zone; // the offsets that made it local
    bool uncertainty_valid;
    uint32_t uncertainty_ns;
    uint8_t hundredths; // SSMM: 0 at the second, then display_tick()'s
} DisplayView;

typedef struct
{
    uint32_t prepared;
    uint32_t loaded;
    uint32_t missed;  // prepared, but the next was prepared before it was loaded
    uint32_t redraws; // shown at once: a mode change, or SSMM's hundredths
} DisplayStats;

// Tubes on, showing dashes, mode HHMM, 24 hours, delta epoch 0.
void display_init(void);

// The settings: the mode selected (a new one is shown at once, ending any
// shown by the button), 12 or 24 hours, and the delta mode's epoch: a UTC
// instant, or with local a local wall-clock time (UTC plus the offsets then
// in force, as a scalar) counted against local time.
// Each change is drawn at once.
void display_set_mode(DisplayMode mode);
void display_set_12h(bool twelve_hours);
// The start dot, in every mode but dashes: the alarm is set (as V2).
void display_set_alarm_dot(bool on);
void display_set_delta_epoch(uint32_t seconds, bool local);

// The mode shown now, and the one selected.
DisplayMode display_mode(void);
DisplayMode display_selected(void);
bool display_is_12h(void);
uint32_t display_delta_epoch(void);
bool display_delta_is_local(void);
// The delta epoch as set: 2027-01-01T00:00:00Z, or 2027-01-01T00:00:00 local.
void display_print_epoch(uint32_t seconds, bool local);

// A short press: show the next mode, for DISPLAY_CYCLE_EPOCHS. Nothing while
// the time is unknown (dashes).
void display_cycle(void);

// Once per epoch, soon after it: renders the frame for the next epoch.
void display_prepare(const DisplayClock *clock);
// Loads the prepared frame into the tube drivers. Call once per epoch at
// DISPLAY_LOAD_SLOT (a scheduler slot task). Does nothing if there is no new
// frame, or while the epoch pulse holds the latch open.
void display_service(void);
// Every pass of the main loop, with the slot (ms since the epoch): in SSMM,
// shows the hundredths as they change. Cheap otherwise.
void display_tick(uint16_t slot);

const DisplayStats *display_stats(void);
const char *display_mode_name(DisplayMode mode);
// One line, for diagnostics: the mode, the local offset of the last frame,
// and the frame and tube driver counters.
void display_print(void);

#endif // DISPLAY_H
