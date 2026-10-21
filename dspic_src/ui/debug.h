/*
 * Which console diagnostics are printed: a mask of subsystems, set at build
 * time by DEBUG_DEFAULT and changeable at run time with debug_set(), from
 * the ESP by SML_USER_SET_DEBUG (the bits are SML's SML_DEBUG_*).
 *
 * State changes and warnings are printed whatever the mask: they are rare
 * and always worth seeing. Everything periodic is gated here. Not yet kept
 * over a reset: that comes with the settings store.
 */
#ifndef DEBUG_H
#define DEBUG_H

#include <stdbool.h>
#include <stdint.h>

#define DEBUG_NONE    0x0000U
#define DEBUG_TIMING  0x0001U // per epoch: captures, loop, servo, receiver's view
#define DEBUG_GNSS    0x0002U // per epoch: the GNSS edge and its label
#define DEBUG_SYNC    0x0004U // per epoch: the synchroniser's state
#define DEBUG_SOURCES 0x0008U // per epoch: every candidate, ranked
#define DEBUG_NTP     0x0010U // per epoch: the latest TIME-NTP and its pairing
#define DEBUG_MINUTE  0x0020U // each minute: scheduler, GNSS and sync reports
#define DEBUG_TUNING  0x0040U // each minute: loop and source statistics, holdover trials
#define DEBUG_ALL     0x007FU

// Nothing periodic by default: the ESP turns it on (pic-debug-enable), or
// build with e.g. -DDEBUG_DEFAULT=DEBUG_ALL for a bench session.
#ifndef DEBUG_DEFAULT
#define DEBUG_DEFAULT DEBUG_NONE
#endif

void debug_init(void);
bool debug_enabled(uint16_t subsystems); // any of them
uint16_t debug_mask(void);
void debug_set(uint16_t mask);
// The mask, by name, on the console.
void debug_print(void);

#endif // DEBUG_H
