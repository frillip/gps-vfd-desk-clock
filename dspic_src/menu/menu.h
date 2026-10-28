/*
 * The menu on the tubes, worked with the one button, as V2's:
 *
 *   MENU  tZ  dSt  ALrM  bEEP  dISP  [tIME]  rSt  EXIt
 *
 * A long press on the clock opens it. Moving through it, a short press goes
 * to the next item and a long press (MENU_LONG_MS, acted on while held)
 * opens a section, starts editing a value, or goes bACK or EXIt. Each
 * section ends with bACK.
 *
 * Editing a value (it flashes), each press is judged on release:
 *
 *   shorter than MENU_LONG_MS       +1 (wrapping)
 *   MENU_LONG_MS to MENU_REPEAT_MS  confirm: the next field, or the value
 *                                   stored. At MENU_LONG_MS the value blinks
 *                                   off (MENU_CUE_MS), then shows steadily:
 *                                   let go to confirm
 *   longer than MENU_REPEAT_MS      +1 every MENU_REPEAT_PERIOD_MS while
 *                                   held; letting go does not confirm, and
 *                                   a step less than MENU_TAKE_BACK_MS
 *                                   before it is taken back (reaction time)
 *
 * A field with one value (tIME's OK) is confirmed at MENU_LONG_MS while
 * held. A long press acted on while held (opening a section, starting to
 * edit, bACK, EXIt, OK) ends there: the rest of it, however long, is
 * ignored until the button is let go. MENU_TIMEOUT_MS without a press closes the menu; a value being
 * edited is dropped. Values confirmed take effect at once, as any change of
 * the settings; the settings are saved once, when the menu closes, if any
 * changed (and the ESP's, if its remote time zone setting was changed).
 *
 * tIME is there only while no source but the RTC has given the time since
 * start-up: the clock tells the time, but an offline clock has to be told
 * it once. YYYY, then MM-DD, then hh:mm, then OK: the long press on OK sets
 * the time to hh:mm:00 local at the moment the button went down.
 *
 * rSt: WIFI (the ESP forgets its network), SEt (the PIC's factory settings,
 * saved) and ALL (both, the ESP's saved too), each NO unless changed to YES.
 *
 * Above the settings, below the ESP link: the ESP is reached through
 * MenuEsp, given by main.c.
 */
#ifndef MENU_H
#define MENU_H

#include <stdbool.h>
#include <stdint.h>

#define MENU_LONG_MS          500U
#define MENU_REPEAT_MS        1200U
#define MENU_REPEAT_PERIOD_MS 200U // 5 Hz
#define MENU_CUE_MS           150U // the blink at MENU_LONG_MS: let go to confirm
#define MENU_TAKE_BACK_MS     120U
#define MENU_TIMEOUT_MS       20000U
#define MENU_FLASH_PERIOD_MS  1000U // a value being edited: lit for
#define MENU_FLASH_ON_MS      600U  // ... this much of each period
#define MENU_NAME_MS          2000U // a display mode's name, before its preview
#define MENU_RTC_WRITE_MS     5000U // to write the RTC after tIME, while it is busy

// The ESP, for the menu: a command (esp_link_request: its token, 0 if not
// sent), and its remote time zone setting (from STAT-NET), if known.
typedef struct
{
    uint16_t (*request)(uint16_t command, uint32_t argument);
    bool (*remote_tz)(uint32_t now_ms, bool *enabled);
    bool (*linked)(void); // the ESP speaks SML
} MenuEsp;

typedef struct
{
    uint32_t opened;
    uint32_t timeouts;
    uint32_t saves;
    uint32_t time_sets;
} MenuStats;

// Closed. esp is kept.
void menu_init(const MenuEsp *esp);

// A long press on the clock: the menu opens on MENU, the press that opened
// it ignored until released.
void menu_open(uint32_t now_ms);
bool menu_is_open(void);
// Every 10 ms, open or not, with the button as the menu should see it (a
// press spent elsewhere, stopping the alarm, as up) and where in the epoch
// it is now (epoch_timer_position()).
void menu_service(uint32_t now_ms, bool button_down, uint32_t epoch, uint16_t slot);
// Each epoch, after sync_update(): whether only the RTC has given the time.
void menu_epoch(void);

// tIME is offered.
bool menu_time_settable(void);
// STAT-UI menu_state: SML_MENU_NONE, or section | item (| SML_MENU_EDITING).
uint8_t menu_state(void);
const MenuStats *menu_stats(void);

#endif // MENU_H
