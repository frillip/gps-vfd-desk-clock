/*
 * The buzzer (RB7, active high: a self-oscillating buzzer switched by Q2),
 * and the sounds it plays.
 *
 * A sound is a pattern and a count, not a buffer: count beeps of on_ms,
 * gap_ms apart, in groups of group with pause_ms between groups, so 12 is
 * heard as 4-4-4 (V2's chimes). A repeating pattern starts again after a
 * pause until stopped (the alarm). V2 built a 100-step buffer for each
 * sound; a pattern does the same in a few bytes, for any count.
 *
 * Timer5 times the steps at 1 ms (priority 1, running only while a sound
 * plays), so a busy main loop (a minute report, a flash erase) does not
 * stretch a beep. A sound asked for in the epoch's work starts within a
 * millisecond of the epoch.
 *
 * One sound at a time, by priority: a sound replaces one of the same or a
 * lower priority, and is refused while a higher one plays. So a button
 * beep never cuts into a chime, and nothing cuts into the alarm.
 */
#ifndef BUZZER_H
#define BUZZER_H

#include <stdbool.h>
#include <stdint.h>

#define BUZZER_INTERRUPT_PRIORITY 1
#define BUZZER_TICK_MS            1U

typedef struct
{
    uint16_t on_ms;    // each beep
    uint16_t gap_ms;   // between beeps in a group
    uint8_t group;     // beeps per group; 0: all in one
    uint16_t pause_ms; // between groups, and before a repeat
    bool repeat;       // from the start again after the last, until stopped
} BuzzerPattern;

typedef enum
{
    BUZZER_PRIORITY_NONE = 0, // nothing playing
    BUZZER_PRIORITY_BUTTON,
    BUZZER_PRIORITY_CHIME,
    BUZZER_PRIORITY_ALARM,
} BuzzerPriority;

// As V2's (its steps were 20 ms).
extern const BuzzerPattern buzzer_button; // one short click: 60 ms
extern const BuzzerPattern buzzer_chime;  // 100 ms beeps, 100 ms apart, 4s, 300 ms pause
extern const BuzzerPattern buzzer_alarm;  // as the chime, 500 ms pause, repeating

typedef struct
{
    uint32_t played;
    uint32_t refused; // a higher priority was playing
    uint32_t stopped; // ended by buzzer_stop()
} BuzzerStats;

// Timer5 set up, buzzer off. Pins are set by board_init().
void buzzer_init(void);

// Plays count beeps of pattern (count 0: nothing). False if refused.
bool buzzer_play(const BuzzerPattern *pattern, uint8_t count, BuzzerPriority priority);
// Ends what is playing if its priority is at most priority.
void buzzer_stop(BuzzerPriority priority);
// What is playing (NONE when silent).
BuzzerPriority buzzer_playing(void);

const BuzzerStats *buzzer_stats(void);

#endif // BUZZER_H
