#include "display.h"

#include <stddef.h>
#include <stdio.h>

#include "../drivers/tubes.h"
#include "frame.h"

typedef struct
{
    const char *name;
    void (*render)(const DisplayView *view, TubesFrame *frame);
} DisplayModeDefinition;

static void render_hhmm(const DisplayView *view, TubesFrame *frame);

// One row per DisplayMode, in the same order.
static const DisplayModeDefinition modes[DISPLAY_MODE_COUNT] = {
    {"HHMM", render_hhmm},
};

static DisplayMode mode;
static int32_t offset_s; // UTC to local, of the last frame prepared
static TubesFrame prepared;
static bool prepared_pending;
static DisplayStats stats;

// --------------------------------------------------------------------------
// Modes
// --------------------------------------------------------------------------

// The colon blinks with the seconds while synchronised, and stays on while
// the clock is running free, as V2.
static void render_hhmm(const DisplayView *view, TubesFrame *frame)
{
    frame_number(frame, 0, 2, view->local.hour);
    frame_number(frame, 2, 2, view->local.minute);
    if (!view->synchronised || view->local.second % 2U == 0U)
    {
        frame->separators |= TUBES_MIDDLE_BOTH;
    }
}


// --------------------------------------------------------------------------
// Interface
// --------------------------------------------------------------------------

void display_init(void)
{
    TubesFrame frame;

    mode = DISPLAY_MODE_HHMM;
    offset_s = 0;
    prepared_pending = false;
    stats.prepared = 0;
    stats.loaded = 0;
    stats.missed = 0;

    tubes_init();
    frame_clear(&frame);
    frame_dashes(&frame);
    tubes_show(&frame);
}


void display_set_mode(DisplayMode new_mode)
{
    if (new_mode < DISPLAY_MODE_COUNT)
    {
        mode = new_mode;
    }
}


DisplayMode display_mode(void)
{
    return mode;
}


void display_prepare(const DisplayClock *clock)
{
    // The epoch that has just passed latched the last frame loaded.
    tubes_epoch();
    if (prepared_pending)
    {
        stats.missed++;
    }

    frame_clear(&prepared);
    if (clock == NULL || !clock->known)
    {
        frame_dashes(&prepared);
    }
    else
    {
        DisplayView view;
        bool next_leap;
        uint32_t next = utc_advance(clock->utc_seconds, clock->leap_second, 1U,
                                    clock->leap.change != 0 ? &clock->leap : NULL, &next_leap);

        timezone_offset_at(next, &view.zone);
        offset_s = view.zone.offset_s;
        utc_to_civil(next + (uint32_t)offset_s, next_leap, &view.local);
        view.synchronised = clock->synchronised;
        modes[mode].render(&view, &prepared);
    }
    prepared_pending = true;
    stats.prepared++;
}


void display_service(void)
{
    if (!prepared_pending || tubes_latch_open())
    {
        return;
    }
    tubes_load(&prepared);
    prepared_pending = false;
    stats.loaded++;
}


const DisplayStats *display_stats(void)
{
    return &stats;
}


const char *display_mode_name(DisplayMode which)
{
    return which < DISPLAY_MODE_COUNT ? modes[which].name : "?";
}


void display_print(void)
{
    const TubesStats *tubes = tubes_stats();

    printf("Display: %s, UTC%+ld s, frames %lu prepared %lu loaded %lu missed; tubes: brightness "
           "%u, %lu loads (%lu too late), %lu shows, %lu SPI stalls\r\n",
           display_mode_name(mode), (long)offset_s, (unsigned long)stats.prepared,
           (unsigned long)stats.loaded, (unsigned long)stats.missed, tubes_brightness(),
           (unsigned long)tubes->loads, (unsigned long)tubes->loads_too_late,
           (unsigned long)tubes->shows, (unsigned long)tubes->spi_stalls);
}
