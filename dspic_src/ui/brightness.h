/*
 * How bright the tubes are: automatic, from the ambient light sensor
 * (drivers/veml6040.h), or the manual level from the settings.
 *
 *   auto    the level follows the light: BRIGHTNESS_LUX_CONST
 *           + BRIGHTNESS_LUX_LINEAR x lux_x10 + BRIGHTNESS_LUX_SQUARE x
 *           lux_x10^2, rounded to BRIGHTNESS_STEP and kept within the
 *           tubes' PWM range (TUBES_BRIGHTNESS_MIN to MAX). Too dark to
 *           measure: the dimmest; the sensor saturated: the brightest.
 *           Without a reading (no sensor, or it stopped answering) the
 *           manual level stands in.
 *   manual  the level set (brightness setting, 0 to TUBES_BRIGHTNESS_FULL:
 *           below MIN the tubes are off, above MAX lit continuously).
 *
 * In auto the tubes move to the target by BRIGHTNESS_STEP each
 * brightness_service() (every 10 ms: the whole range in about 4 s), so a
 * change of light fades rather than jumps. Off and fully lit are not PWM
 * levels: a fade up from off starts at MIN, and one down from fully lit at
 * MAX. A manual level is a step: the tubes go straight to it.
 *
 * The settings (settings/settings.h) set the mode and the level; STAT-UI
 * reports the level, the target and the mode.
 */
#ifndef BRIGHTNESS_H
#define BRIGHTNESS_H

#include <stdbool.h>
#include <stdint.h>

#define BRIGHTNESS_LUX_CONST  0UL
#define BRIGHTNESS_LUX_LINEAR 3UL // per 0.1 lux: 3800 (the brightest) from 127 lux
#define BRIGHTNESS_LUX_SQUARE 0UL
#define BRIGHTNESS_STEP       10U // per brightness_service(), and the rounding of the target

// Auto, the level TUBES_BRIGHTNESS_DEFAULT, the tubes at it. After
// tubes_init().
void brightness_init(void);

void brightness_set_auto(bool automatic);
bool brightness_is_auto(void);
// The manual level, 0 to TUBES_BRIGHTNESS_FULL.
void brightness_set_level(uint16_t level);

// Every 10 ms: the target from the light (a step towards it) or the
// setting (straight to it).
void brightness_service(void);

// What the tubes are being taken to (tubes_brightness() is where they are).
uint16_t brightness_target(void);
// The level for a light reading, 0.1 lux (auto, before the fade).
uint16_t brightness_from_lux(uint16_t lux_x10);

// One line: AUTO or MANUAL, the level now and any fade, and the light.
// verbose: the manual level too, while auto.
void brightness_print(bool verbose);

#endif // BRIGHTNESS_H
