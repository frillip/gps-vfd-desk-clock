/*
 * VFD tube driver: four 7-segment tubes and two separator positions, driven
 * through a chain of serial high-voltage drivers on SPI2.
 *
 *   DISPLAY_SPI2_SCK, DISPLAY_SPI2_SDO -> driver shift register
 *   LATCH = DISPLAY_LATCH_OC (the epoch output, OC1) OR DISPLAY_LATCH_GPIO:
 *           level-triggered, so the drivers' outputs follow the shift
 *           register while it is high
 *   DISPLAY_BLANK = OC3 PWM: brightness
 *
 * (Pins in platform/board.h.)
 *
 * Content is described by a TubesFrame: what each tube and separator shows,
 * independent of the wiring. The board wiring (which driver output each
 * segment lands on, and the order the words go out) lives only in tubes.c.
 *
 * Two ways to put a frame on the tubes:
 *
 *   tubes_load()  shifts it in and leaves it there. The next epoch pulse
 *                 latches it, so it appears exactly on the second. Load
 *                 while the epoch pulse is low (tubes_latch_open() false),
 *                 or it shows at once.
 *   tubes_show()  shifts it in and latches it now with LATCH_GPIO, for
 *                 updates that must not wait for the second (menus). A frame
 *                 already loaded for the next epoch is shifted back in
 *                 afterwards, so the second still changes on time.
 */
#ifndef TUBES_H
#define TUBES_H

#include <stdbool.h>
#include <stdint.h>

#define TUBES_COUNT 4U // tube 0 is the leftmost

// Segments of one tube, as bits of TubesFrame.segments[]:
//    aaa
//   f   b
//    ggg
//   e   c
//    ddd
#define TUBES_SEGMENT_A   0x01U
#define TUBES_SEGMENT_B   0x02U
#define TUBES_SEGMENT_C   0x04U
#define TUBES_SEGMENT_D   0x08U
#define TUBES_SEGMENT_E   0x10U
#define TUBES_SEGMENT_F   0x20U
#define TUBES_SEGMENT_G   0x40U
#define TUBES_SEGMENT_ALL 0x7FU

// Separators, as bits of TubesFrame.separators. START is left of tube 0;
// MIDDLE is between tubes 1 and 2 (the colon of HH:MM).
#define TUBES_START_DOT   0x01U
#define TUBES_START_LINE  0x02U
#define TUBES_MIDDLE_DOT  0x04U
#define TUBES_MIDDLE_LINE 0x08U
#define TUBES_START_BOTH  (TUBES_START_DOT | TUBES_START_LINE)
#define TUBES_MIDDLE_BOTH (TUBES_MIDDLE_DOT | TUBES_MIDDLE_LINE)

typedef struct
{
    uint8_t segments[TUBES_COUNT]; // TUBES_SEGMENT_* per tube
    uint8_t separators;            // TUBES_START_* | TUBES_MIDDLE_*
} TubesFrame;

// Brightness: the share of each 100 us PWM period the tubes are lit, in
// FP ticks. Below MIN the tubes are blanked; above MAX they are lit
// continuously (OC3 off, BLANK held low).
#define TUBES_BRIGHTNESS_FULL    4000U // = the PWM period
#define TUBES_BRIGHTNESS_MAX     3800U
#define TUBES_BRIGHTNESS_MIN     200U
#define TUBES_BRIGHTNESS_DEFAULT 2000U
#define TUBES_BRIGHTNESS_OFF     0U

#define TUBES_WORDS 4U // 16-bit words per frame on the wire

typedef struct
{
    uint32_t loads;          // frames shifted in for the next epoch
    uint32_t shows;          // frames latched at once
    uint32_t reloads;        // epoch frames shifted back in after a show
    uint32_t loads_too_late; // loaded while the epoch latch was open
    uint32_t spi_stalls;     // SPI did not finish in time (should never happen)
} TubesStats;

// SPI2 and OC3 set up, tubes blank, brightness TUBES_BRIGHTNESS_DEFAULT.
// Pins are mapped by board_init().
void tubes_init(void);

void tubes_load(const TubesFrame *frame);
void tubes_show(const TubesFrame *frame);
// The epoch pulse is high: the drivers' outputs follow whatever is loaded.
bool tubes_latch_open(void);

void tubes_brightness_set(uint16_t level);
uint16_t tubes_brightness(void);

// Call once per epoch, after it: the epoch pulse has latched what was loaded.
void tubes_epoch(void);

// The frame on the tubes now, and the one in the shift register (shown at
// the next epoch, unless already on the tubes).
const TubesFrame *tubes_shown(void);
const TubesFrame *tubes_loaded(void);
const TubesStats *tubes_stats(void);

// The words sent for a frame, in the order they go out. Pure: exposed for
// tests and diagnostics.
void tubes_encode(const TubesFrame *frame, uint16_t words[TUBES_WORDS]);

#endif // TUBES_H
