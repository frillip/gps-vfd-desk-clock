/*
 * The PIC's settings, and keeping them over a reset.
 *
 * The values are SML's settings body (SML_SETTINGS_BODY, see STAT-SETTINGS
 * in common/SML_PROTOCOL.md), plus the debug mask: the manual time zone,
 * TZ and DST auto, the brightness, the display, the stored oscillator
 * frequency, and the alarm, delta and format settings the V2+ UI does not
 * use yet. This module holds them and applies each to the module it belongs
 * to (ui/timezone.h, drivers/tubes.h, ui/display.h, ui/debug.h); the ESP's
 * commands change them here (esp/esp_link.c), never in those modules.
 *
 *   revision  counts changes to the body (not to the debug mask), kept with
 *             it, so the ESP can tell whether its copy is current
 *             (CMD-SETTINGS expected_revision).
 *   dirty     the running values differ from those stored (from the
 *             factory defaults, while nothing is stored).
 *   defaults  the body is the factory defaults.
 *
 * Saving is asked for (settings_save: SAVE, DEFAULTS, CMD-SETTINGS with
 * PERSIST), never automatic: as V2, a change by command runs at once and
 * is kept only once saved. The time zone schedule from the ESP is not a
 * setting: it is stored by itself, SETTINGS_TIMEZONE_DELAY_EPOCHS after its
 * contents last changed (so the CACHE, then GEOIP or GNSS schedules an ESP
 * sends as it starts make one write, not three), and is back in force at
 * start-up before the ESP has said anything.
 *
 * Both are records in the store (settings/store.h). Writing to flash stops
 * the CPU (drivers/flash.h), so settings_service() writes only when told
 * the moment is quiet: once an epoch, late in the second, away from the
 * epoch's work, the GNSS burst and the ESP's per-second traffic, with the
 * I2C bus idle and the RTC source between searches (main.c).
 */
#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdbool.h>
#include <stdint.h>

#include "../../common/sml_proto.h"

// The store's record types.
#define SETTINGS_RECORD_SETTINGS 1U
#define SETTINGS_RECORD_TIMEZONE 2U
#define SETTINGS_RECORD_TIMING   3U

// The settings record: version, STAT-SETTINGS as on the wire (its flags
// zero), the debug mask, reserved.
#define SETTINGS_RECORD_VERSION 1U
#define SETTINGS_RECORD_BYTES   (2U + SML_STAT_SETTINGS_BYTES + 4U)

// When flash may be written: the slot it is tried at, and the latest slot
// it may still start in, leaving an erase (~25 ms) well clear of the epoch.
#define SETTINGS_STORE_SLOT            700U
#define SETTINGS_STORE_LAST_SLOT       900U
#define SETTINGS_TIMEZONE_DELAY_EPOCHS 60U

// The stored oscillator frequency may differ from FCY by this much.
#define SETTINGS_FOSC_LIMIT_PPM 1000UL

typedef struct
{
    bool loaded;            // settings came from the store at start-up
    bool load_rejected;     // a settings record was found but not used
    bool timezone_restored; // a schedule came from the store at start-up
    uint32_t saves;         // settings records written
    uint32_t timezone_saves;
    uint32_t save_failures;
} SettingsStats;

// Reads the store and applies what it holds, or the defaults. Call after
// the modules it applies to are initialised.
void settings_init(void);

// The running values.
void settings_body(SML_SETTINGS_BODY *body);
uint16_t settings_debug_mask(void);
uint32_t settings_revision(void);
// SML_SETTINGS_VALID, DIRTY and DEFAULTS.
uint16_t settings_flags(void);
void settings_stat(SML_STAT_SETTINGS_PAYLOAD *payload);
void settings_defaults(SML_SETTINGS_BODY *body);

// SML_ERROR_NONE if every field is acceptable, else the error for the first
// that is not (ARGUMENT, SCHEMA, or UNSUPPORTED for a display mode not
// ported yet).
uint8_t settings_check(const SML_SETTINGS_BODY *body);
// Checks, then applies every field together; the revision goes up if
// anything changed. Nothing changes on an error.
uint8_t settings_write(const SML_SETTINGS_BODY *body);
void settings_set_debug(uint16_t mask);
// The factory defaults, applied (the debug mask too).
void settings_restore_defaults(void);

// Asks for the settings to be stored at the next quiet moment.
void settings_save(void);
bool settings_save_pending(void);
// The outcome of the latest save: SML_ERROR_NONE, STORAGE or VERIFY.
uint8_t settings_save_error(void);

// Once an epoch. quiet: flash may be written now (see above).
void settings_service(uint32_t epoch, bool quiet);

const SettingsStats *settings_stats(void);
// At start-up: where the settings and the schedule came from. One line.
void settings_print_start(void);
// The running settings, with the revision and flags.
void settings_print(void);
// What the store holds, and the store itself.
void settings_print_stored(void);

#endif // SETTINGS_H
