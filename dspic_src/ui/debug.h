/*
 * Which console diagnostics are printed: a mask of subsystems, set at build
 * time by DEBUG_DEFAULT and changeable at run time with debug_set().
 *
 * State changes and warnings are printed whatever the mask: they are rare
 * and always worth seeing. Everything periodic is gated here. Not yet
 * settable from the ESP or kept over a reset: that comes with the SML
 * command (SML_USER_SET_DEBUG) and the settings store.
 */
#ifndef DEBUG_H
#define DEBUG_H

#include <stdbool.h>
#include <stdint.h>

#define DEBUG_TIMING  0x0001U // per epoch: captures, loop, servo, receiver's view
#define DEBUG_GNSS    0x0002U // per epoch: the GNSS edge and its label
#define DEBUG_SYNC    0x0004U // per epoch: the synchroniser's state
#define DEBUG_SOURCES 0x0008U // per epoch: every candidate, ranked
#define DEBUG_NTP     0x0010U // per epoch: the latest TIME-NTP and its pairing
#define DEBUG_MINUTE  0x0020U // each minute: scheduler, GNSS and sync reports
#define DEBUG_TUNING  0x0040U // each minute: loop and source statistics, holdover trials
#define DEBUG_ALL     0x007FU

// Everything, for now, while the source selection is tuned on the bench.
#ifndef DEBUG_DEFAULT
#define DEBUG_DEFAULT DEBUG_ALL
#endif

void debug_init(void);
bool debug_enabled(uint16_t subsystems); // any of them
uint16_t debug_mask(void);
void debug_set(uint16_t mask);

#endif // DEBUG_H
