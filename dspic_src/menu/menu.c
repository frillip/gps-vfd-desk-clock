#include "menu.h"

#include <stddef.h>
#include <string.h>

#include "../../common/sml_proto.h"
#include "../drivers/tubes.h"
#include "../platform/utc.h"
#include "../settings/settings.h"
#include "../sync/source_rtc.h"
#include "../sync/sync.h"
#include "../ui/beeps.h"
#include "../ui/brightness.h"
#include "../ui/display.h"
#include "../ui/frame.h"
#include "../ui/timezone.h"

#define MAX_FIELDS           6U
#define MENU_BRIGHTNESS_STEP (TUBES_BRIGHTNESS_FULL / 10U) // 10 %

typedef enum
{
    ITEM_TITLE, // MENU: a long press closes
    ITEM_OPEN,  // a section
    ITEM_VALUE, // edited in place
    ITEM_BACK,
    ITEM_EXIT,
} ItemKind;

typedef struct
{
    const char *label; // shown while moving through; NULL: the value is
    ItemKind kind;
    uint8_t opens;  // ITEM_OPEN: the section
    uint8_t fields; // ITEM_VALUE: edited in turn
    bool (*visible)(void);
    void (*load)(int16_t *values);
    // A field's range and step; it wraps.
    void (*limits)(const int16_t *values, uint8_t field, int16_t *low, int16_t *high,
                   int16_t *step);
    // field: the one being edited (-1: none); hide: blank it (flashing).
    void (*render)(const int16_t *values, int8_t field, bool hide, TubesFrame *frame);
    void (*store)(const int16_t *values);
    // While editing: shows the value live (on), or puts things back (off).
    void (*preview)(const int16_t *values, bool on);
    bool live_mode; // the display mode: its name, then the mode itself
} MenuItem;

typedef struct
{
    uint8_t sml; // SML_MENU_* section
    const MenuItem *items;
    uint8_t count;
} MenuSection;

static const MenuEsp *esp;
static MenuStats stats;

static bool open;
static uint8_t section;
static uint8_t item;
static bool editing;
static uint8_t field;
static int16_t values[MAX_FIELDS];
static uint32_t now; // as of the last menu_service()
static uint32_t last_activity_ms;
static uint32_t flash_start_ms;
static uint32_t changed_ms; // the value last changed (the mode's name)

// The button, as the menu times it.
static bool waiting_release;
static bool was_down;
static uint32_t down_ms;
static uint32_t down_epoch; // where in the epoch it went down: tIME's moment
static uint16_t down_slot;
static bool repeating;
static uint32_t last_step_ms;

static SML_SETTINGS_BODY work; // the settings as the menu changes them
static bool settings_changed;
static bool esp_changed;

static bool only_rtc; // no source but the RTC has given the time since start
static bool rtc_pending;
static uint32_t rtc_pending_since_ms;

// --------------------------------------------------------------------------
// Values
// --------------------------------------------------------------------------

static void write_settings(void)
{
    uint32_t revision = settings_revision();

    if (settings_write(&work) == SML_ERROR_NONE && settings_revision() != revision)
    {
        settings_changed = true;
    }
    settings_body(&work);
}


static bool flag(uint16_t bit)
{
    return (work.flags & bit) != 0U;
}


static void set_flag(uint16_t bit, bool on)
{
    work.flags = on ? (uint16_t)(work.flags | bit) : (uint16_t)(work.flags & (uint16_t)~bit);
}


static void toggle_limits(const int16_t *v, uint8_t f, int16_t *low, int16_t *high, int16_t *step)
{
    (void)v;
    (void)f;
    *low = 0;
    *high = 1;
    *step = 1;
}


static void text(TubesFrame *frame, const char *characters, bool hide)
{
    frame_text(frame, hide ? "" : characters);
}


// "hhmm" of a magnitude in minutes, the colon lit, the start line for minus.
static void render_offset_minutes(int32_t minutes, int8_t hide_tubes, TubesFrame *frame)
{
    uint32_t magnitude = (uint32_t)(minutes < 0 ? -minutes : minutes);

    frame_number(frame, 0, 2, (uint16_t)(magnitude / 60U));
    frame_number(frame, 2, 2, (uint16_t)(magnitude % 60U));
    frame->separators = TUBES_MIDDLE_BOTH | (minutes < 0 ? TUBES_START_LINE : 0U);
    if (hide_tubes >= 0)
    {
        frame_blank(frame, (uint8_t)hide_tubes, 2);
    }
}


// ---- tZ -------------------------------------------------------------------

static void tz_auto_load(int16_t *v)
{
    v[0] = flag(SML_SETTING_TZ_AUTO) ? 1 : 0;
}


static void auto_render(const int16_t *v, int8_t f, bool hide, TubesFrame *frame)
{
    (void)f;
    text(frame, v[0] != 0 ? "AUtO" : " MAN", hide);
}


static void tz_auto_store(const int16_t *v)
{
    set_flag(SML_SETTING_TZ_AUTO, v[0] != 0);
    write_settings();
}


static bool tz_manual(void)
{
    return !flag(SML_SETTING_TZ_AUTO);
}


// Hours -12 to +14 and minutes in quarters: the minutes take the hours' sign.
static void tz_offset_load(int16_t *v)
{
    int32_t offset = work.base_offset_s;
    int32_t magnitude = offset < 0 ? -offset : offset;

    v[0] = (int16_t)(offset / 3600L);
    v[1] = (int16_t)((magnitude % 3600L) / 900L * 15L);
}


static void tz_offset_limits(const int16_t *v, uint8_t f, int16_t *low, int16_t *high,
                             int16_t *step)
{
    (void)v;
    *low = f == 0U ? -12 : 0;
    *high = f == 0U ? 14 : 45;
    *step = f == 0U ? 1 : 15;
}


static int32_t tz_offset_minutes(const int16_t *v)
{
    return v[0] < 0 ? (int32_t)v[0] * 60 - v[1] : (int32_t)v[0] * 60 + v[1];
}


static void tz_offset_render(const int16_t *v, int8_t f, bool hide, TubesFrame *frame)
{
    render_offset_minutes(tz_offset_minutes(v), hide ? (int8_t)(f * 2) : (int8_t)-1, frame);
}


static void tz_offset_store(const int16_t *v)
{
    work.base_offset_s = tz_offset_minutes(v) * 60L;
    write_settings();
}


static bool remote_known(void)
{
    bool on;
    return esp != NULL && esp->remote_tz != NULL && esp->remote_tz(now, &on);
}


static void remote_load(int16_t *v)
{
    bool on = false;

    if (esp != NULL && esp->remote_tz != NULL)
    {
        (void)esp->remote_tz(now, &on);
    }
    v[0] = on ? 1 : 0;
}


static void remote_render(const int16_t *v, int8_t f, bool hide, TubesFrame *frame)
{
    (void)f;
    text(frame, v[0] != 0 ? "r ON" : "rOFF", hide);
}


static void remote_store(const int16_t *v)
{
    if (esp != NULL && esp->request != NULL &&
        esp->request(SML_USER_ESP_TZ_REMOTE, v[0] != 0 ? 1U : 0U) != 0U)
    {
        esp_changed = true;
    }
}


// ---- dSt ------------------------------------------------------------------

static void dst_auto_load(int16_t *v)
{
    v[0] = flag(SML_SETTING_DST_AUTO) ? 1 : 0;
}


static void dst_auto_store(const int16_t *v)
{
    set_flag(SML_SETTING_DST_AUTO, v[0] != 0);
    write_settings();
}


static bool dst_manual(void)
{
    return !flag(SML_SETTING_DST_AUTO);
}


static void on_off_render(const int16_t *v, int8_t f, bool hide, TubesFrame *frame)
{
    (void)f;
    text(frame, v[0] != 0 ? "  ON" : " OFF", hide);
}


static void dst_active_load(int16_t *v)
{
    v[0] = flag(SML_SETTING_DST_ACTIVE) ? 1 : 0;
}


static void dst_active_store(const int16_t *v)
{
    set_flag(SML_SETTING_DST_ACTIVE, v[0] != 0);
    write_settings();
}


// The DST amount in quarter hours, -1 h (Dublin) to +2 h.
static void dst_offset_load(int16_t *v)
{
    int32_t minutes = work.dst_offset_s / 60L;

    minutes = minutes / 15L * 15L;
    v[0] = (int16_t)(minutes < -60L ? -60L : minutes > 120L ? 120L : minutes);
}


static void dst_offset_limits(const int16_t *v, uint8_t f, int16_t *low, int16_t *high,
                              int16_t *step)
{
    (void)v;
    (void)f;
    *low = -60;
    *high = 120;
    *step = 15;
}


static void dst_offset_render(const int16_t *v, int8_t f, bool hide, TubesFrame *frame)
{
    (void)f;
    render_offset_minutes(v[0], -1, frame);
    if (hide)
    {
        frame_blank(frame, 0, TUBES_COUNT);
    }
}


static void dst_offset_store(const int16_t *v)
{
    work.dst_offset_s = (int32_t)v[0] * 60L;
    write_settings();
}


// ---- ALrM -----------------------------------------------------------------

static void alarm_on_load(int16_t *v)
{
    v[0] = flag(SML_SETTING_ALARM_ENABLED) ? 1 : 0;
}


static void alarm_on_store(const int16_t *v)
{
    set_flag(SML_SETTING_ALARM_ENABLED, v[0] != 0);
    write_settings();
}


static bool alarm_on(void)
{
    return flag(SML_SETTING_ALARM_ENABLED);
}


static void time_of_day_limits(const int16_t *v, uint8_t f, int16_t *low, int16_t *high,
                               int16_t *step)
{
    (void)v;
    *low = 0;
    *high = f == 0U ? 23 : 59;
    *step = 1;
}


static void hh_mm_render(int16_t hours, int16_t minutes, int8_t hide_tubes, TubesFrame *frame)
{
    frame_number(frame, 0, 2, (uint16_t)hours);
    frame_number(frame, 2, 2, (uint16_t)minutes);
    frame->separators = TUBES_MIDDLE_BOTH;
    if (hide_tubes >= 0)
    {
        frame_blank(frame, (uint8_t)hide_tubes, 2);
    }
}


static void alarm_time_load(int16_t *v)
{
    v[0] = (int16_t)(work.alarm_offset_s / 3600UL);
    v[1] = (int16_t)(work.alarm_offset_s / 60UL % 60UL);
}


static void alarm_time_render(const int16_t *v, int8_t f, bool hide, TubesFrame *frame)
{
    hh_mm_render(v[0], v[1], hide ? (int8_t)(f * 2) : (int8_t)-1, frame);
}


static void alarm_time_store(const int16_t *v)
{
    work.alarm_offset_s = (uint32_t)v[0] * 3600U + (uint32_t)v[1] * 60U;
    write_settings();
}


// ---- bEEP -----------------------------------------------------------------

static void beep_load(int16_t *v)
{
    v[0] = flag(SML_SETTING_BEEP_ENABLED) ? 1 : 0;
}


static void beep_store(const int16_t *v)
{
    set_flag(SML_SETTING_BEEP_ENABLED, v[0] != 0);
    write_settings();
}


// ---- dISP -----------------------------------------------------------------

static void hours_load(int16_t *v)
{
    v[0] = flag(SML_SETTING_FORMAT_12H) ? 1 : 0;
}


static void hours_render(const int16_t *v, int8_t f, bool hide, TubesFrame *frame)
{
    (void)f;
    text(frame, v[0] != 0 ? "  12" : "  24", hide);
}


static void hours_store(const int16_t *v)
{
    set_flag(SML_SETTING_FORMAT_12H, v[0] != 0);
    write_settings();
}


// The display modes in the button's order, as SML_DISPLAY_* values.
static const uint8_t mode_codes[DISPLAY_MODE_COUNT] = {
    SML_DISPLAY_HHMM, SML_DISPLAY_MMSS,  SML_DISPLAY_SSMM,        SML_DISPLAY_YYYY,
    SML_DISPLAY_MMDD, SML_DISPLAY_DELTA, SML_DISPLAY_UNCERTAINTY,
};
static const char *const mode_names[DISPLAY_MODE_COUNT] = {
    "HHMM", "MMSS", "SSMM", "YYYY", "MMDD", "dELt", "UNC ",
};

static void mode_load(int16_t *v)
{
    uint8_t index;

    v[0] = 0;
    for (index = 0; index < DISPLAY_MODE_COUNT; index++)
    {
        if (mode_codes[index] == work.selected_display)
        {
            v[0] = (int16_t)index;
        }
    }
}


static void mode_limits(const int16_t *v, uint8_t f, int16_t *low, int16_t *high, int16_t *step)
{
    (void)v;
    (void)f;
    *low = 0;
    *high = (int16_t)(DISPLAY_MODE_COUNT - 1U);
    *step = 1;
}


static void mode_render(const int16_t *v, int8_t f, bool hide, TubesFrame *frame)
{
    (void)f;
    text(frame, mode_names[v[0]], hide);
}


static void mode_store(const int16_t *v)
{
    work.selected_display = mode_codes[v[0]];
    write_settings();
}


// AUtO, or 10 % to 100 % of the PWM in tens.
static void brightness_load(int16_t *v)
{
    uint16_t tenths =
        (uint16_t)((work.brightness + MENU_BRIGHTNESS_STEP / 2U) / MENU_BRIGHTNESS_STEP);

    v[0] = flag(SML_SETTING_BRI_AUTO) ? 0 : (int16_t)(tenths < 1U ? 1U : tenths);
}


static void brightness_limits(const int16_t *v, uint8_t f, int16_t *low, int16_t *high,
                              int16_t *step)
{
    (void)v;
    (void)f;
    *low = 0;
    *high = 10;
    *step = 1;
}


static void brightness_render(const int16_t *v, int8_t f, bool hide, TubesFrame *frame)
{
    (void)f;
    if (v[0] == 0)
    {
        text(frame, "AUtO", hide);
        return;
    }
    frame_text(frame, "b");
    frame_number_blanked(frame, 1, 3, (uint16_t)(v[0] * 10));
    if (hide)
    {
        frame_blank(frame, 0, TUBES_COUNT);
    }
}


static void brightness_preview(const int16_t *v, bool on)
{
    if (on && v[0] != 0)
    {
        brightness_set_level((uint16_t)(v[0] * (int16_t)MENU_BRIGHTNESS_STEP));
        brightness_set_auto(false);
        return;
    }
    // Auto, or back to the settings.
    brightness_set_level(work.brightness);
    brightness_set_auto(on || flag(SML_SETTING_BRI_AUTO));
}


static void brightness_store(const int16_t *v)
{
    set_flag(SML_SETTING_BRI_AUTO, v[0] == 0);
    if (v[0] != 0)
    {
        work.brightness = (uint16_t)(v[0] * (int16_t)MENU_BRIGHTNESS_STEP);
    }
    write_settings();
}


// ---- tIME -----------------------------------------------------------------

static uint8_t days_in_month(int16_t year, int16_t month)
{
    static const uint8_t days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    if (month == 2 && year % 4 == 0 && (year % 100 != 0 || year % 400 == 0))
    {
        return 29U;
    }
    return days[month - 1];
}


// The local time now, or 2026-01-01 00:00 if the time is not known.
static void time_load(int16_t *v)
{
    const SyncTime *time = &sync_status()->time;
    TimezoneOffset zone;
    UtcCivil civil;

    if (time->known)
    {
        timezone_offset_at(time->utc_seconds, &zone);
        utc_to_civil(time->utc_seconds + (uint32_t)zone.offset_s, false, &civil);
    }
    else
    {
        civil.year = 2026U;
        civil.month = 1U;
        civil.day = 1U;
        civil.hour = 0U;
        civil.minute = 0U;
    }
    v[0] = (int16_t)civil.year;
    v[1] = (int16_t)civil.month;
    v[2] = (int16_t)civil.day;
    v[3] = (int16_t)civil.hour;
    v[4] = (int16_t)civil.minute;
    v[5] = 0;
}


static void time_limits(const int16_t *v, uint8_t f, int16_t *low, int16_t *high, int16_t *step)
{
    static const int16_t lows[MAX_FIELDS] = {2000, 1, 1, 0, 0, 0};
    static const int16_t highs[MAX_FIELDS] = {2099, 12, 31, 23, 59, 0};

    *low = lows[f];
    *high = f == 2U ? (int16_t)days_in_month(v[0], v[1]) : highs[f];
    *step = 1;
}


static void time_render(const int16_t *v, int8_t f, bool hide, TubesFrame *frame)
{
    if (f < 0)
    {
        frame_text(frame, "tIME");
    }
    else if (f == 0)
    {
        frame_number(frame, 0, 4, (uint16_t)v[0]);
        if (hide)
        {
            frame_blank(frame, 0, TUBES_COUNT);
        }
    }
    else if (f <= 2)
    {
        frame_number(frame, 0, 2, (uint16_t)v[1]);
        frame_number(frame, 2, 2, (uint16_t)v[2]);
        frame->separators = TUBES_MIDDLE_LINE;
        if (hide)
        {
            frame_blank(frame, (uint8_t)((f - 1) * 2), 2);
        }
    }
    else if (f <= 4)
    {
        hh_mm_render(v[3], v[4], hide ? (int8_t)((f - 3) * 2) : (int8_t)-1, frame);
    }
    else
    {
        text(frame, "  OK", hide);
    }
}


// hh:mm:00 local at the moment the button went down: to UTC with the
// offsets in force then, the clock relabelled (sync_set_time), and the RTC
// written as soon as the time is in force.
static void time_store(const int16_t *v)
{
    UtcCivil civil;
    TimezoneOffset zone;
    uint32_t local;
    uint32_t utc;

    civil.year = (uint16_t)v[0];
    civil.month = (uint8_t)v[1];
    civil.day = (uint8_t)v[2];
    civil.hour = (uint8_t)v[3];
    civil.minute = (uint8_t)v[4];
    civil.second = 0U;
    local = utc_from_civil(&civil);
    timezone_offset_at(local, &zone); // near enough to find the offset...
    utc = local - (uint32_t)zone.offset_s;
    timezone_offset_at(utc, &zone); // ... and that offset's own
    utc = local - (uint32_t)zone.offset_s;
    if (sync_set_time(utc, down_epoch, down_slot))
    {
        stats.time_sets++;
        rtc_pending = true;
        rtc_pending_since_ms = now;
    }
}


// ---- rSt ------------------------------------------------------------------

static void yes_no_render(const int16_t *v, int8_t f, bool hide, TubesFrame *frame)
{
    (void)f;
    text(frame, v[0] != 0 ? " YES" : "  NO", hide);
}


static void no_load(int16_t *v)
{
    v[0] = 0;
}


static bool esp_linked(void)
{
    return esp != NULL && esp->linked != NULL && esp->linked();
}


static void esp_command(uint16_t command)
{
    if (esp != NULL && esp->request != NULL)
    {
        (void)esp->request(command, 0U);
    }
}


static void reset_pic(void)
{
    settings_restore_defaults();
    settings_save();
    settings_body(&work);
    settings_changed = false; // saved already
    stats.saves++;
}


static void reset_wifi_store(const int16_t *v)
{
    if (v[0] != 0)
    {
        esp_command(SML_USER_ESP_WIFI_FORGET);
    }
}


static void reset_settings_store(const int16_t *v)
{
    if (v[0] != 0)
    {
        reset_pic();
    }
}


// Both: the ESP's defaults saved, then its network forgotten (it restarts).
static void reset_all_store(const int16_t *v)
{
    if (v[0] != 0)
    {
        reset_pic();
        esp_command(SML_USER_ESP_DEFAULTS);
        esp_command(SML_USER_ESP_SAVE);
        esp_command(SML_USER_ESP_WIFI_FORGET);
        esp_changed = false;
    }
}


// --------------------------------------------------------------------------
// The menu
// --------------------------------------------------------------------------

#define TOGGLE(label_, visible_, load_, render_, store_)                                       \
    {                                                                                          \
        label_, ITEM_VALUE, 0, 1, visible_, load_, toggle_limits, render_, store_, NULL, false \
    }
#define BACK                                                               \
    {                                                                      \
        "bACK", ITEM_BACK, 0, 0, NULL, NULL, NULL, NULL, NULL, NULL, false \
    }

enum
{
    SECTION_TOP = 0,
    SECTION_TZ,
    SECTION_DST,
    SECTION_ALARM,
    SECTION_BEEP,
    SECTION_DISPLAY,
    SECTION_RESET,
    SECTION_COUNT
};

static bool time_visible(void)
{
    return only_rtc;
}


static const MenuItem top_items[] = {
    {"MENU", ITEM_TITLE, 0, 0, NULL, NULL, NULL, NULL, NULL, NULL, false},
    {"  tZ", ITEM_OPEN, SECTION_TZ, 0, NULL, NULL, NULL, NULL, NULL, NULL, false},
    {" dSt", ITEM_OPEN, SECTION_DST, 0, NULL, NULL, NULL, NULL, NULL, NULL, false},
    {"ALrM", ITEM_OPEN, SECTION_ALARM, 0, NULL, NULL, NULL, NULL, NULL, NULL, false},
    {"bEEP", ITEM_OPEN, SECTION_BEEP, 0, NULL, NULL, NULL, NULL, NULL, NULL, false},
    {"dISP", ITEM_OPEN, SECTION_DISPLAY, 0, NULL, NULL, NULL, NULL, NULL, NULL, false},
    {"tIME", ITEM_VALUE, 0, 6, time_visible, time_load, time_limits, time_render, time_store, NULL,
     false},
    {" rSt", ITEM_OPEN, SECTION_RESET, 0, NULL, NULL, NULL, NULL, NULL, NULL, false},
    {"EXIt", ITEM_EXIT, 0, 0, NULL, NULL, NULL, NULL, NULL, NULL, false},
};

static const MenuItem tz_items[] = {
    TOGGLE(NULL, NULL, tz_auto_load, auto_render, tz_auto_store),
    {NULL, ITEM_VALUE, 0, 2, tz_manual, tz_offset_load, tz_offset_limits, tz_offset_render,
     tz_offset_store, NULL, false},
    TOGGLE(NULL, remote_known, remote_load, remote_render, remote_store),
    BACK,
};

static const MenuItem dst_items[] = {
    TOGGLE(NULL, NULL, dst_auto_load, auto_render, dst_auto_store),
    TOGGLE(NULL, dst_manual, dst_active_load, on_off_render, dst_active_store),
    {NULL, ITEM_VALUE, 0, 1, dst_manual, dst_offset_load, dst_offset_limits, dst_offset_render,
     dst_offset_store, NULL, false},
    BACK,
};

static const MenuItem alarm_items[] = {
    TOGGLE(NULL, NULL, alarm_on_load, on_off_render, alarm_on_store),
    {NULL, ITEM_VALUE, 0, 2, alarm_on, alarm_time_load, time_of_day_limits, alarm_time_render,
     alarm_time_store, NULL, false},
    BACK,
};

static const MenuItem beep_items[] = {
    TOGGLE(NULL, NULL, beep_load, on_off_render, beep_store),
    BACK,
};

static const MenuItem display_items[] = {
    TOGGLE(NULL, NULL, hours_load, hours_render, hours_store),
    {NULL, ITEM_VALUE, 0, 1, NULL, mode_load, mode_limits, mode_render, mode_store, NULL, true},
    {NULL, ITEM_VALUE, 0, 1, NULL, brightness_load, brightness_limits, brightness_render,
     brightness_store, brightness_preview, false},
    BACK,
};

static const MenuItem reset_items[] = {
    TOGGLE("WIFI", esp_linked, no_load, yes_no_render, reset_wifi_store),
    TOGGLE("SEt ", NULL, no_load, yes_no_render, reset_settings_store),
    TOGGLE(" ALL", esp_linked, no_load, yes_no_render, reset_all_store),
    BACK,
};

#define ITEMS(list) list, (uint8_t)(sizeof(list) / sizeof(list[0]))

static const MenuSection sections[SECTION_COUNT] = {
    {SML_MENU_TOP, ITEMS(top_items)},     {SML_MENU_TZ, ITEMS(tz_items)},
    {SML_MENU_DST, ITEMS(dst_items)},     {SML_MENU_ALARM, ITEMS(alarm_items)},
    {SML_MENU_BEEP, ITEMS(beep_items)},   {SML_MENU_DISPLAY, ITEMS(display_items)},
    {SML_MENU_RESET, ITEMS(reset_items)},
};

static const MenuItem *current(void)
{
    return &sections[section].items[item];
}


static bool shown(const MenuItem *entry)
{
    return entry->visible == NULL || entry->visible();
}


// The next item shown in the section, after index (wrapping).
static uint8_t next_item(uint8_t index)
{
    uint8_t count = sections[section].count;
    uint8_t tries;

    for (tries = 0; tries < count; tries++)
    {
        index = (uint8_t)((index + 1U) % count);
        if (shown(&sections[section].items[index]))
        {
            return index;
        }
    }
    return index;
}


static void click(void)
{
    beeps_button();
}


static void restart_flash(uint32_t now_ms)
{
    flash_start_ms = now_ms;
}


static void stop_editing(void)
{
    const MenuItem *entry = current();

    if (entry->preview != NULL)
    {
        entry->preview(values, false);
    }
    display_preview(DISPLAY_MODE_COUNT);
    editing = false;
}


static void close_menu(void)
{
    if (editing)
    {
        stop_editing();
    }
    open = false;
    display_set_overlay(NULL);
    display_preview(DISPLAY_MODE_COUNT);
    if (settings_changed)
    {
        settings_save();
        settings_changed = false;
        stats.saves++;
    }
    if (esp_changed)
    {
        esp_command(SML_USER_ESP_SAVE);
        esp_changed = false;
    }
}


static void start_editing(uint32_t now_ms)
{
    const MenuItem *entry = current();

    settings_body(&work);
    memset(values, 0, sizeof(values));
    entry->load(values);
    editing = true;
    field = 0;
    changed_ms = now_ms;
    restart_flash(now_ms);
    if (entry->preview != NULL)
    {
        entry->preview(values, true);
    }
}


static bool single_valued(void)
{
    int16_t low, high, step;

    current()->limits(values, field, &low, &high, &step);
    return low == high;
}


// One step of the field being edited, up or down, wrapping.
static void step(int8_t direction, uint32_t now_ms)
{
    const MenuItem *entry = current();
    int16_t low, high, size;
    int16_t value = values[field];

    entry->limits(values, field, &low, &high, &size);
    if (low == high)
    {
        return;
    }
    if (direction > 0)
    {
        value = value + size > high ? low : (int16_t)(value + size);
    }
    else
    {
        value = value - size < low ? high : (int16_t)(value - size);
    }
    values[field] = value;
    changed_ms = now_ms;
    restart_flash(now_ms);
    if (entry->preview != NULL)
    {
        entry->preview(values, true);
    }
}


// The field confirmed: the next one (its value brought within its range),
// or the value stored.
static void confirm(uint32_t now_ms)
{
    const MenuItem *entry = current();
    int16_t low, high, size;

    if ((uint8_t)(field + 1U) < entry->fields)
    {
        field++;
        entry->limits(values, field, &low, &high, &size);
        if (values[field] > high)
        {
            values[field] = high;
        }
        if (values[field] < low)
        {
            values[field] = low;
        }
        restart_flash(now_ms);
        return;
    }
    entry->store(values);
    stop_editing();
}


// Moving through: a short press.
static void move_short(void)
{
    item = next_item(item);
}


// Moving through: a long press.
static void move_long(uint32_t now_ms)
{
    const MenuItem *entry = current();
    uint8_t index;

    switch (entry->kind)
    {
        case ITEM_TITLE:
        case ITEM_EXIT: close_menu(); break;
        case ITEM_OPEN:
            section = entry->opens;
            item = shown(&sections[section].items[0]) ? 0U : next_item(0U);
            break;
        case ITEM_BACK:
            for (index = 0; index < sections[SECTION_TOP].count; index++)
            {
                if (sections[SECTION_TOP].items[index].kind == ITEM_OPEN &&
                    sections[SECTION_TOP].items[index].opens == section)
                {
                    item = index;
                }
            }
            section = SECTION_TOP;
            break;
        case ITEM_VALUE: start_editing(now_ms); break;
    }
}


static void render(uint32_t now_ms, bool down)
{
    const MenuItem *entry = current();
    TubesFrame frame;
    uint32_t held = now_ms - down_ms;
    bool long_held = editing && down && !waiting_release && !repeating && held >= MENU_LONG_MS;
    // Held long enough to confirm: a blink, then steady until it repeats.
    bool cue = long_held && held < MENU_LONG_MS + MENU_CUE_MS;
    bool steady = long_held && !cue;
    bool hide = cue || (editing && !steady &&
                        (now_ms - flash_start_ms) % MENU_FLASH_PERIOD_MS >= MENU_FLASH_ON_MS);

    frame_clear(&frame);
    if (editing && entry->live_mode && now_ms - changed_ms >= MENU_NAME_MS)
    {
        display_set_overlay(NULL);
        display_preview((DisplayMode)values[0]);
        return;
    }
    display_preview(DISPLAY_MODE_COUNT);
    if (editing)
    {
        entry->render(values, (int8_t)field, hide, &frame);
    }
    else if (entry->label != NULL)
    {
        frame_text(&frame, entry->label);
    }
    else
    {
        int16_t shown_values[MAX_FIELDS];

        memset(shown_values, 0, sizeof(shown_values));
        entry->load(shown_values);
        entry->render(shown_values, -1, false, &frame);
    }
    display_set_overlay(&frame);
}


// The button: edges and how long it has been held.
static void follow_button(uint32_t now_ms, bool down, uint32_t epoch, uint16_t slot)
{
    uint32_t held = now_ms - down_ms;

    if (waiting_release)
    {
        if (!down)
        {
            waiting_release = false;
            last_activity_ms = now_ms;
        }
        was_down = down;
        return;
    }
    if (down && !was_down)
    {
        down_ms = now_ms;
        down_epoch = epoch;
        down_slot = slot;
        repeating = false;
        last_activity_ms = now_ms;
        held = 0;
    }
    if (down)
    {
        // A long press acted on while held is done with: the rest of it,
        // however long, is ignored, so it cannot go on to repeat or step in
        // whatever it opened.
        if (!editing)
        {
            if (held >= MENU_LONG_MS)
            {
                waiting_release = true;
                click();
                move_long(now_ms);
            }
        }
        else if (single_valued())
        {
            if (held >= MENU_LONG_MS)
            {
                waiting_release = true;
                click();
                confirm(now_ms);
            }
        }
        else if (!repeating && held >= MENU_REPEAT_MS)
        {
            repeating = true;
            last_step_ms = now_ms;
            click();
            step(1, now_ms);
        }
        else if (repeating && now_ms - last_step_ms >= MENU_REPEAT_PERIOD_MS)
        {
            last_step_ms = now_ms;
            click();
            step(1, now_ms);
        }
    }
    else if (was_down)
    {
        last_activity_ms = now_ms;
        if (!editing)
        {
            click();
            move_short();
        }
        else if (repeating)
        {
            if (now_ms - last_step_ms < MENU_TAKE_BACK_MS)
            {
                step(-1, now_ms);
            }
        }
        else if (held < MENU_LONG_MS)
        {
            click();
            step(1, now_ms);
        }
        else
        {
            click();
            confirm(now_ms);
        }
    }
    was_down = down;
}


// After tIME: the RTC written with the clock's new time once it is in force
// (the epoch after the setting), retried while the RTC source is busy.
static void write_rtc(uint32_t now_ms, uint16_t slot)
{
    const SyncTime *time = &sync_status()->time;

    if (!rtc_pending)
    {
        return;
    }
    if (now_ms - rtc_pending_since_ms > MENU_RTC_WRITE_MS)
    {
        rtc_pending = false;
        return;
    }
    if (time->known && time->source == SYNC_SOURCE_NONE &&
        source_rtc_set(time->utc_seconds + (slot >= 500U ? 1U : 0U)))
    {
        rtc_pending = false;
    }
}


// --------------------------------------------------------------------------
// Interface
// --------------------------------------------------------------------------

void menu_init(const MenuEsp *esp_link)
{
    esp = esp_link;
    open = false;
    editing = false;
    waiting_release = false;
    was_down = false;
    settings_changed = false;
    esp_changed = false;
    only_rtc = true;
    rtc_pending = false;
    memset(&stats, 0, sizeof(stats));
}


void menu_open(uint32_t now_ms)
{
    if (open)
    {
        return;
    }
    open = true;
    section = SECTION_TOP;
    item = 0;
    editing = false;
    waiting_release = true;
    last_activity_ms = now_ms;
    now = now_ms;
    settings_body(&work);
    settings_changed = false;
    esp_changed = false;
    stats.opened++;
    render(now_ms, true);
}


bool menu_is_open(void)
{
    return open;
}


void menu_service(uint32_t now_ms, bool button_down, uint32_t epoch, uint16_t slot)
{
    now = now_ms;
    write_rtc(now_ms, slot);
    if (!open)
    {
        was_down = button_down;
        return;
    }
    follow_button(now_ms, button_down, epoch, slot);
    if (!open)
    {
        return; // closed by the press
    }
    if (!button_down && now_ms - last_activity_ms >= MENU_TIMEOUT_MS)
    {
        stats.timeouts++;
        close_menu();
        return;
    }
    // An item hidden meanwhile (tIME once a source gives the time).
    if (!editing && !shown(current()))
    {
        item = next_item(item);
    }
    render(now_ms, button_down);
}


void menu_epoch(void)
{
    const SyncStatus *status = sync_status();
    uint8_t index;

    for (index = 0; index < SYNC_SOURCE_COUNT; index++)
    {
        if (sync_source_steers(index) && status->sources[index].report.time_valid)
        {
            only_rtc = false;
        }
    }
}


bool menu_time_settable(void)
{
    return only_rtc;
}


uint8_t menu_state(void)
{
    if (!open)
    {
        return SML_MENU_NONE;
    }
    return (uint8_t)(sections[section].sml | (item & SML_MENU_ITEM_MASK) |
                     (editing ? SML_MENU_EDITING : 0U));
}


const MenuStats *menu_stats(void)
{
    return &stats;
}
