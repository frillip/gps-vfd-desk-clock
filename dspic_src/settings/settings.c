#include "settings.h"

#include <stdio.h>
#include <string.h>

#include "../../common/sml.h"
#include "../drivers/tubes.h"
#include "../platform/clock.h"
#include "../timing/epoch_control.h"
#include "../ui/brightness.h"
#include "../ui/debug.h"
#include "../ui/display.h"
#include "../ui/timezone.h"
#include "store.h"

#define FLAGS_DEFINED                                                                \
    (SML_SETTING_TZ_AUTO | SML_SETTING_DST_AUTO | SML_SETTING_DST_ACTIVE |           \
     SML_SETTING_ALARM_ENABLED | SML_SETTING_BEEP_ENABLED | SML_SETTING_FORMAT_12H | \
     SML_SETTING_BRI_AUTO)
#define SECONDS_PER_DAY 86400UL
#define FOSC_LIMIT_HZ   (FCY / 1000000UL * SETTINGS_FOSC_LIMIT_PPM)
#define TICKS_PER_US    (FCY / 1000000UL) // flash_set_clock(): the capture timebase

typedef struct
{
    SML_SETTINGS_BODY body;
    uint16_t debug_mask;
} Values;

static Values running;
static Values stored; // as last stored, or the defaults while nothing is
static uint32_t revision;
static bool have_record; // a settings record is stored

static bool save_requested;
static uint8_t save_error;
static bool timezone_pending;
static uint32_t timezone_changed_epoch;
static SettingsStats stats;

// The oscillator record: what is stored, and when it was written this run.
static bool have_oscillator;
static int32_t oscillator_q16;
static bool oscillator_saved;
static uint32_t oscillator_saved_epoch;

void settings_defaults(SML_SETTINGS_BODY *body)
{
    memset(body, 0, sizeof(*body));
    body->schema_version = SML_SETTINGS_SCHEMA;
    body->flags = SML_SETTING_TZ_AUTO | SML_SETTING_DST_AUTO | SML_SETTING_BRI_AUTO;
    body->calibration_fosc_hz = FCY;
    body->brightness = TUBES_BRIGHTNESS_DEFAULT;
    body->selected_display = SML_DISPLAY_HHMM;
}


static bool offset_ok(int32_t offset_s)
{
    return offset_s >= -TIMEZONE_OFFSET_LIMIT_S && offset_s <= TIMEZONE_OFFSET_LIMIT_S;
}


uint8_t settings_check(const SML_SETTINGS_BODY *body)
{
    if (body->schema_version != SML_SETTINGS_SCHEMA)
    {
        return SML_ERROR_SCHEMA;
    }
    if (body->selected_display != SML_DISPLAY_HHMM)
    {
        // Selectable but not ported yet, or not selectable at all.
        return body->selected_display > SML_DISPLAY_HHMM &&
                       body->selected_display < SML_DISPLAY_DASHES
                   ? SML_ERROR_UNSUPPORTED
                   : SML_ERROR_ARGUMENT;
    }
    if ((body->flags & (uint16_t)~FLAGS_DEFINED) != 0U || body->reserved != 0U ||
        !offset_ok(body->base_offset_s) || !offset_ok(body->dst_offset_s) ||
        body->calibration_fosc_hz < FCY - FOSC_LIMIT_HZ ||
        body->calibration_fosc_hz > FCY + FOSC_LIMIT_HZ ||
        body->alarm_offset_s >= SECONDS_PER_DAY || body->brightness > TUBES_BRIGHTNESS_FULL)
    {
        return SML_ERROR_ARGUMENT;
    }
    return SML_ERROR_NONE;
}


// Each value to the module it belongs to. The alarm, delta, beep and
// 12-hour settings are only kept until the UI uses them.
static void apply(void)
{
    const SML_SETTINGS_BODY *body = &running.body;

    timezone_set_auto((body->flags & SML_SETTING_TZ_AUTO) != 0U);
    timezone_set_dst_auto((body->flags & SML_SETTING_DST_AUTO) != 0U);
    timezone_set_manual(body->base_offset_s, body->dst_offset_s,
                        (body->flags & SML_SETTING_DST_ACTIVE) != 0U);
    brightness_set_level(body->brightness);
    brightness_set_auto((body->flags & SML_SETTING_BRI_AUTO) != 0U);
    display_set_mode(DISPLAY_MODE_HHMM); // the only one so far (settings_check)
}


static bool same_body(const SML_SETTINGS_BODY *a, const SML_SETTINGS_BODY *b)
{
    return memcmp(a, b, sizeof(*a)) == 0;
}


// The record's bytes, from values and a revision.
static void encode_record(const Values *values, uint32_t record_revision, uint8_t *record)
{
    SML_STAT_SETTINGS_PAYLOAD payload;

    memset(&payload, 0, sizeof(payload));
    payload.revision = record_revision;
    payload.settings = values->body;
    memset(record, 0, SETTINGS_RECORD_BYTES);
    sml_put_u16(&record[0], SETTINGS_RECORD_VERSION);
    (void)sml_encode(sml_message(SML_CLASS_STAT, SML_STAT_SETTINGS)->layout, &payload, &record[2]);
    sml_put_u16(&record[2U + SML_STAT_SETTINGS_BYTES], values->debug_mask);
}


// The settings record in the store, if there is one and it can be used.
static bool read_record(Values *values, uint32_t *record_revision, bool *rejected)
{
    uint8_t record[SETTINGS_RECORD_BYTES];
    SML_STAT_SETTINGS_PAYLOAD payload;
    uint16_t length;

    *rejected = false;
    if (!store_read(SETTINGS_RECORD_SETTINGS, record, sizeof(record), &length))
    {
        return false;
    }
    if (length != SETTINGS_RECORD_BYTES || sml_get_u16(&record[0]) != SETTINGS_RECORD_VERSION)
    {
        *rejected = true;
        return false;
    }
    (void)sml_decode(sml_message(SML_CLASS_STAT, SML_STAT_SETTINGS)->layout, &record[2], &payload);
    if (settings_check(&payload.settings) != SML_ERROR_NONE)
    {
        *rejected = true;
        return false;
    }
    memset(values, 0, sizeof(*values));
    values->body = payload.settings;
    values->debug_mask = sml_get_u16(&record[2U + SML_STAT_SETTINGS_BYTES]) & DEBUG_ALL;
    *record_revision = payload.revision;
    return true;
}


static void restore_timezone(void)
{
    uint8_t record[SML_TZ_STATE_BYTES];
    SML_TZ_STATE_PAYLOAD state;
    uint16_t length;

    if (!store_read(SETTINGS_RECORD_TIMEZONE, record, sizeof(record), &length) ||
        length != SML_TZ_STATE_BYTES)
    {
        return;
    }
    (void)sml_decode(sml_message(SML_CLASS_TZ, SML_TZ_STATE)->layout, record, &state);
    stats.timezone_restored = timezone_apply(&state) == TIMEZONE_APPLIED;
    (void)timezone_take_changed(); // as stored: nothing to write
}


// The oscillator record: version, flags (0), the Kalman estimate (period -
// nominal, Q16 ticks), and how many epochs it had tracked for.
static void encode_oscillator(int32_t offset_q16, uint32_t epochs, uint8_t *record)
{
    memset(record, 0, SETTINGS_OSCILLATOR_BYTES);
    sml_put_u16(&record[0], SETTINGS_OSCILLATOR_VERSION);
    sml_put_u32(&record[4], (uint32_t)offset_q16);
    sml_put_u32(&record[8], epochs);
}


static bool read_oscillator(int32_t *offset_q16, uint32_t *epochs)
{
    uint8_t record[SETTINGS_OSCILLATOR_BYTES];
    uint16_t length;

    if (!store_read(SETTINGS_RECORD_OSCILLATOR, record, sizeof(record), &length) ||
        length != SETTINGS_OSCILLATOR_BYTES ||
        sml_get_u16(&record[0]) != SETTINGS_OSCILLATOR_VERSION)
    {
        return false;
    }
    *offset_q16 = (int32_t)sml_get_u32(&record[4]);
    *epochs = sml_get_u32(&record[8]);
    return *offset_q16 >= -SETTINGS_OSCILLATOR_LIMIT_Q16 &&
           *offset_q16 <= SETTINGS_OSCILLATOR_LIMIT_Q16;
}


// The loop starts from the stored estimate rather than from nominal.
static void restore_oscillator(void)
{
    uint32_t epochs;

    have_oscillator = read_oscillator(&oscillator_q16, &epochs);
    oscillator_saved = false;
    if (have_oscillator)
    {
        epoch_control_seed(oscillator_q16);
        stats.oscillator_restored = true;
    }
}


void settings_init(void)
{
    memset(&stats, 0, sizeof(stats));
    memset(&running, 0, sizeof(running));
    save_requested = false;
    save_error = SML_ERROR_NONE;
    timezone_pending = false;
    revision = 0;

    store_init();
    settings_defaults(&running.body);
    running.debug_mask = DEBUG_NONE;
    stats.loaded = read_record(&running, &revision, &stats.load_rejected);
    have_record = stats.loaded;
    stored = running;
    apply();
    // A build for the bench (DEBUG_DEFAULT) adds its own on top.
    debug_set((uint16_t)(running.debug_mask | DEBUG_DEFAULT));
    restore_timezone();
    restore_oscillator();
}


void settings_body(SML_SETTINGS_BODY *body)
{
    *body = running.body;
}


uint16_t settings_debug_mask(void)
{
    return running.debug_mask;
}


uint32_t settings_revision(void)
{
    return revision;
}


uint16_t settings_flags(void)
{
    SML_SETTINGS_BODY defaults;
    uint16_t flags = SML_SETTINGS_VALID;

    settings_defaults(&defaults);
    if (!same_body(&running.body, &stored.body) || running.debug_mask != stored.debug_mask)
    {
        flags |= SML_SETTINGS_DIRTY;
    }
    if (same_body(&running.body, &defaults))
    {
        flags |= SML_SETTINGS_DEFAULTS;
    }
    return flags;
}


void settings_stat(SML_STAT_SETTINGS_PAYLOAD *payload)
{
    memset(payload, 0, sizeof(*payload));
    payload->flags = settings_flags();
    payload->revision = revision;
    payload->settings = running.body;
}


uint8_t settings_write(const SML_SETTINGS_BODY *body)
{
    uint8_t error = settings_check(body);

    if (error != SML_ERROR_NONE)
    {
        return error;
    }
    if (!same_body(body, &running.body))
    {
        running.body = *body;
        revision++;
        apply();
    }
    return SML_ERROR_NONE;
}


void settings_set_debug(uint16_t mask)
{
    running.debug_mask = mask & DEBUG_ALL;
    debug_set(running.debug_mask);
}


void settings_restore_defaults(void)
{
    SML_SETTINGS_BODY defaults;

    settings_defaults(&defaults);
    (void)settings_write(&defaults);
    settings_set_debug(DEBUG_NONE);
}


void settings_save(void)
{
    save_requested = true;
}


bool settings_save_pending(void)
{
    return save_requested;
}


uint8_t settings_save_error(void)
{
    return save_error;
}


static uint8_t store_error(StoreResult result)
{
    switch (result)
    {
        case STORE_OK: return SML_ERROR_NONE;
        case STORE_VERIFY: return SML_ERROR_VERIFY;
        default: return SML_ERROR_STORAGE;
    }
}


static void save_settings(void)
{
    uint8_t record[SETTINGS_RECORD_BYTES];

    save_requested = false;
    if ((settings_flags() & SML_SETTINGS_DIRTY) == 0U && have_record)
    {
        save_error = SML_ERROR_NONE; // already as stored
        return;
    }
    encode_record(&running, revision, record);
    save_error = store_error(store_write(SETTINGS_RECORD_SETTINGS, record, sizeof(record)));
    if (save_error == SML_ERROR_NONE)
    {
        stored = running;
        have_record = true;
        stats.saves++;
    }
    else
    {
        stats.save_failures++;
    }
}


static void save_timezone(uint32_t epoch)
{
    uint8_t record[SML_TZ_STATE_BYTES];
    SML_TZ_STATE_PAYLOAD state;

    timezone_pending = false;
    if (!timezone_state(&state))
    {
        return;
    }
    (void)sml_encode(sml_message(SML_CLASS_TZ, SML_TZ_STATE)->layout, &state, record);
    if (store_write(SETTINGS_RECORD_TIMEZONE, record, sizeof(record)) == STORE_OK)
    {
        stats.timezone_saves++;
    }
    else
    {
        // Again after the delay.
        stats.save_failures++;
        timezone_pending = true;
        timezone_changed_epoch = epoch;
    }
}


// The loop's frequency estimate is worth keeping once it has been learning
// from a reference that teaches it (GNSS) for a while, and has moved from
// what is stored; after a write this run, not again for an hour.
static bool oscillator_due(uint32_t epoch)
{
    const EpochControl *control = epoch_control_state();
    int32_t estimate;
    int32_t change;

    if (control->state != EPOCH_STATE_TRACK || !control->profile->learn_frequency ||
        !control->pps_kf.initialized ||
        control->reference_epochs < SETTINGS_OSCILLATOR_TRACK_EPOCHS)
    {
        return false;
    }
    if (!have_oscillator)
    {
        return true;
    }
    estimate = kalman_phase_q16(&control->pps_kf);
    change = estimate > oscillator_q16 ? estimate - oscillator_q16 : oscillator_q16 - estimate;
    return change >= SETTINGS_OSCILLATOR_CHANGE_Q16 &&
           (!oscillator_saved || epoch - oscillator_saved_epoch >= SETTINGS_OSCILLATOR_EPOCHS);
}


static void save_oscillator(uint32_t epoch)
{
    const EpochControl *control = epoch_control_state();
    uint8_t record[SETTINGS_OSCILLATOR_BYTES];
    int32_t estimate = kalman_phase_q16(&control->pps_kf);

    encode_oscillator(estimate, control->reference_epochs, record);
    // Tried again in an hour if it fails.
    oscillator_saved = true;
    oscillator_saved_epoch = epoch;
    if (store_write(SETTINGS_RECORD_OSCILLATOR, record, sizeof(record)) == STORE_OK)
    {
        have_oscillator = true;
        oscillator_q16 = estimate;
        stats.oscillator_saves++;
    }
    else
    {
        stats.save_failures++;
    }
}


void settings_service(uint32_t epoch, bool quiet)
{
    if (timezone_take_changed())
    {
        timezone_pending = true;
        timezone_changed_epoch = epoch;
    }
    if (!quiet)
    {
        return;
    }
    // One record an epoch at most: each may erase a page.
    if (save_requested)
    {
        save_settings();
    }
    else if (timezone_pending && epoch - timezone_changed_epoch >= SETTINGS_TIMEZONE_DELAY_EPOCHS)
    {
        save_timezone(epoch);
    }
    else if (oscillator_due(epoch))
    {
        save_oscillator(epoch);
    }
}


const SettingsStats *settings_stats(void)
{
    return &stats;
}


static void print_offset(int32_t seconds)
{
    uint32_t magnitude = (uint32_t)(seconds < 0 ? -seconds : seconds);

    printf("%c%02lu:%02lu", seconds < 0 ? '-' : '+', (unsigned long)(magnitude / 3600U),
           (unsigned long)(magnitude / 60U % 60U));
    if (magnitude % 60U != 0U)
    {
        printf(":%02lu", (unsigned long)(magnitude % 60U));
    }
}


static void print_values(const Values *values)
{
    const SML_SETTINGS_BODY *body = &values->body;
    uint16_t flags = body->flags;

    printf("  time zone %s, DST %s; manual: base ",
           (flags & SML_SETTING_TZ_AUTO) ? "auto" : "manual",
           (flags & SML_SETTING_DST_AUTO) ? "auto" : "manual");
    print_offset(body->base_offset_s);
    printf(", DST ");
    print_offset(body->dst_offset_s);
    printf(" %s\r\n", (flags & SML_SETTING_DST_ACTIVE) ? "active" : "inactive");
    printf("  brightness %u%s, display %u, %s; oscillator %lu Hz; debug 0x%04X\r\n",
           body->brightness, (flags & SML_SETTING_BRI_AUTO) ? " (auto)" : "",
           body->selected_display, (flags & SML_SETTING_FORMAT_12H) ? "12 h" : "24 h",
           (unsigned long)body->calibration_fosc_hz, values->debug_mask);
    printf("  alarm %s at %02lu:%02lu:%02lu, beep %s, delta from %lu\r\n",
           (flags & SML_SETTING_ALARM_ENABLED) ? "on" : "off",
           (unsigned long)(body->alarm_offset_s / 3600U),
           (unsigned long)(body->alarm_offset_s / 60U % 60U),
           (unsigned long)(body->alarm_offset_s % 60U),
           (flags & SML_SETTING_BEEP_ENABLED) ? "on" : "off", (unsigned long)body->delta_epoch_utc);
}


// Q16 ticks a second to parts per billion of FCY.
static int32_t ppb(int32_t offset_q16)
{
    return (int32_t)((int64_t)offset_q16 * 1000000000LL / ((int64_t)FCY * 65536LL));
}


void settings_print_start(void)
{
    const TimezoneSchedule *schedule = timezone_schedule();

    printf("Settings: %s, revision %lu",
           stats.loaded          ? "from flash"
           : stats.load_rejected ? "stored record unusable, defaults"
                                 : "defaults (nothing stored)",
           (unsigned long)revision);
    if (stats.timezone_restored)
    {
        printf("; time zone schedule from flash (source %u, %u entries)", schedule->source,
               schedule->count);
    }
    if (stats.oscillator_restored)
    {
        printf("; oscillator from flash, %+ld ppb", (long)ppb(oscillator_q16));
    }
    printf("\r\n");
}


void settings_print(void)
{
    uint16_t flags = settings_flags();

    printf("Settings: revision %lu, %s%s\r\n", (unsigned long)revision,
           (flags & SML_SETTINGS_DIRTY) ? "NOT SAVED (pic-save)" : "saved",
           (flags & SML_SETTINGS_DEFAULTS) ? ", factory defaults" : "");
    print_values(&running);
}


// How long erasing and writing took: the CPU is stopped meanwhile.
static void print_flash(void)
{
    const FlashStats *flash = flash_stats();

    printf("Flash: %lu erases (last %lu us, longest %lu us), %lu double words written (longest "
           "%lu us), %lu failures\r\n",
           (unsigned long)flash->erases, (unsigned long)(flash->last_erase_ticks / TICKS_PER_US),
           (unsigned long)(flash->longest_erase_ticks / TICKS_PER_US), (unsigned long)flash->writes,
           (unsigned long)(flash->longest_write_ticks / TICKS_PER_US),
           (unsigned long)flash->failures);
}


static void print_stored_timezone(void)
{
    uint8_t record[SML_TZ_STATE_BYTES];
    SML_TZ_STATE_PAYLOAD state;
    uint16_t length;

    printf("Stored time zone schedule: ");
    if (!store_read(SETTINGS_RECORD_TIMEZONE, record, sizeof(record), &length) ||
        length != SML_TZ_STATE_BYTES)
    {
        printf("none");
    }
    else
    {
        (void)sml_decode(sml_message(SML_CLASS_TZ, SML_TZ_STATE)->layout, record, &state);
        printf("source %u, issued %lu, %u transitions, horizon %lu", state.source,
               (unsigned long)state.issued_utc, state.count, (unsigned long)state.horizon_end_utc);
    }
    printf("%s\r\n", timezone_pending ? " (a newer one is to be written)" : "");
}


static void print_stored_oscillator(void)
{
    int32_t offset_q16;
    uint32_t epochs;

    if (read_oscillator(&offset_q16, &epochs))
    {
        printf("Stored oscillator: %+ld ppb, learnt over %lu s\r\n", (long)ppb(offset_q16),
               (unsigned long)epochs);
    }
    else
    {
        printf("Stored oscillator: none\r\n");
    }
}


void settings_print_stored(void)
{
    Values values;
    uint32_t record_revision = 0;
    bool rejected;

    if (read_record(&values, &record_revision, &rejected))
    {
        printf("Stored settings: revision %lu\r\n", (unsigned long)record_revision);
        print_values(&values);
    }
    else
    {
        printf("Stored settings: %s\r\n", rejected ? "unusable record" : "none (factory defaults)");
    }
    print_stored_timezone();
    print_stored_oscillator();
    store_print();
    print_flash();
}
