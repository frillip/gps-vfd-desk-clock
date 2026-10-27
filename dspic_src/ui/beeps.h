/*
 * When the buzzer sounds (drivers/buzzer.h plays it): with the beeps
 * setting on (SML_SETTING_BEEP_ENABLED, the ESP's "beeps on"), as V2:
 *
 *   chimes  every quarter hour of local time, at the start of the second:
 *           one beep, or on the hour the hour on a 12-hour count (1 to 12,
 *           midnight and noon 12), in fours.
 *   button  a click for each press (beeps_button()).
 *
 * Only while the time of day is known, and never on a leap second. The
 * alarm (to come) sounds whatever the setting.
 */
#ifndef BEEPS_H
#define BEEPS_H

#include <stdbool.h>
#include <stdint.h>

#define BEEPS_CHIME_INTERVAL_S 900UL // a quarter hour

void beeps_init(void);
void beeps_set_enabled(bool enabled);
bool beeps_enabled(void);

// Each epoch, from its work: the local time of day of the second that has
// just begun (seconds since local midnight), if known.
void beeps_epoch(bool known, bool leap_second, uint32_t local_seconds);
// A button press.
void beeps_button(void);

// The beeps for a local time of day: 0 between quarter hours.
uint8_t beeps_chime_count(uint32_t local_seconds);

#endif // BEEPS_H
