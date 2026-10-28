/*
 * The alarm, as V2's: a local time of day (a setting, with on and off),
 * armed by the switch.
 *
 *   OFF       the setting is off
 *   DISARMED  on, but the switch is off
 *   ARMED     on, switch on: sounds when local time reaches the alarm
 *   SOUNDING  the buzzer's alarm pattern, for ALARM_SOUND_S, until a press
 *             of the button (any press: it does nothing else), or the
 *             switch turned off; then ARMED (or DISARMED) for the next day
 *
 * It sounds whatever the beeps setting, and only while the time of day is
 * known. It goes off once a local day, at the first second at or past the
 * alarm time, so a time skipped by a DST change (01:30 as clocks go from
 * 01:00 to 02:00) sounds at the change, and a repeated one (as clocks go
 * back) sounds once. A step of the clock of more than ALARM_CATCH_UP_S is
 * a time set, not a time passed, and sounds nothing.
 */
#ifndef ALARM_H
#define ALARM_H

#include <stdbool.h>
#include <stdint.h>

#define ALARM_SOUND_S    120U  // V2: 2 minutes
#define ALARM_CATCH_UP_S 7200U // the largest DST change

typedef enum
{
    ALARM_OFF = 0,
    ALARM_DISARMED,
    ALARM_ARMED,
    ALARM_SOUNDING,
    ALARM_STATE_COUNT
} AlarmState;

typedef struct
{
    uint32_t sounded;
    uint32_t stopped_by_button;
    uint32_t stopped_by_switch;
    uint32_t timed_out;
} AlarmStats;

// Off, switch off.
void alarm_init(void);

// The settings: on or off, and the local time of day (s after midnight,
// below 86400). A new time may sound again the same day.
void alarm_set(bool enabled, uint32_t time_of_day_s);
// The switch: on arms it; off stops it sounding. At once, every 10 ms.
void alarm_set_switch(bool on);
// A press of the button: true if it stopped the alarm (the press is spent).
bool alarm_press(void);
// Each epoch, from its work: the local time (UTC plus the offsets, as a
// scalar of seconds) of the second that has just begun, if known.
void alarm_epoch(bool known, uint32_t local_seconds);

AlarmState alarm_state(void);
bool alarm_enabled(void);
uint32_t alarm_time(void);
// The alarm dot on the tubes: armed or sounding.
bool alarm_is_set(void);
const char *alarm_state_name(AlarmState state);
const AlarmStats *alarm_stats(void);

// One line: "Alarm: 07:30:00, ARMED"; verbose adds the counts.
void alarm_print(bool verbose);

#endif // ALARM_H
