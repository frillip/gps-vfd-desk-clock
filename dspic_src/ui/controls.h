/*
 * The push button and the switch, debounced, and the button's presses.
 *
 * controls_service() samples both every 10 ms (main.c passes the pin
 * states, so this module needs no hardware). A pin counts as having changed
 * once it has read the same for CONTROLS_DEBOUNCE_MS; contact bounce
 * shorter than that is never seen. Times are from the first edge of the
 * change, so the debounce does not lengthen a press.
 *
 * The button, as V2:
 *   short  pressed and released before CONTROLS_LONG_MS
 *   long   held for CONTROLS_LONG_MS: reported then, while still held, so
 *          the user knows when to let go; the release adds nothing
 *   hold   (CONTROLS_HOLD) reserved: held on past a long press, for
 *          repeating or a longer action later. Not reported yet.
 *
 * Each press is counted: whoever acts on presses (the display, the menu,
 * the alarm) and whoever reports them (STAT-UI) keep their own count, so
 * neither takes a press from the other: controls_press() gives the latest
 * and the total so far.
 *
 * The switch (the V2 alarm switch) is a level: on or off, debounced the
 * same way, with its changes counted likewise.
 *
 * The first sample sets the starting levels: a switch already on at
 * start-up is not a change, and a button held through a reset is not a
 * press until it has been released.
 */
#ifndef CONTROLS_H
#define CONTROLS_H

#include <stdbool.h>
#include <stdint.h>

#define CONTROLS_SERVICE_MS  10U   // how often controls_service() runs
#define CONTROLS_DEBOUNCE_MS 30UL  // stable this long before a change counts
#define CONTROLS_LONG_MS     500UL // as V2: 50 x 10 ms

// The same values as SML's SML_BUTTON_* (STAT-UI button_state).
typedef enum
{
    CONTROLS_NONE = 0,
    CONTROLS_SHORT,
    CONTROLS_LONG,
    CONTROLS_HOLD, // reserved
    CONTROLS_PRESS_COUNT
} ControlsPress;

typedef struct
{
    uint32_t presses[CONTROLS_PRESS_COUNT]; // by kind, CONTROLS_NONE unused
    uint32_t switch_changes;
    uint32_t bounces; // pin changes that did not last the debounce time
} ControlsStats;

void controls_init(void);

// Every CONTROLS_SERVICE_MS: the pins as read now (true: pressed / on).
void controls_service(uint32_t now_ms, bool button_pressed, bool switch_on);

// Debounced levels.
bool controls_button_down(void);
bool controls_switch_on(void);

// The latest press and how many there have been (any kind): a press is new
// to a caller when the count differs from the one it saw last.
ControlsPress controls_press(uint32_t *count);
// How many times the switch has changed: as controls_press().
uint32_t controls_switch_changes(void);

const ControlsStats *controls_stats(void);
const char *controls_press_name(ControlsPress press);
// One line: the levels and the counts.
void controls_print(void);

#endif // CONTROLS_H
