/*
 * Which console diagnostics are printed: a mask of subsystems, set at build
 * time by DEBUG_DEFAULT and changeable at run time with debug_set(), from
 * the ESP by SML_USER_SET_DEBUG (the bits are SML's SML_DEBUG_*).
 *
 * Whatever the mask, only the start-up line and faults are printed unasked;
 * everything else, periodic or as it happens, is gated here. The mask is
 * one of the settings (settings/settings.h): kept over a reset once saved,
 * with DEBUG_DEFAULT added at start-up.
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
#define DEBUG_RTC     0x0080U // as they happen: RTC set, second search, release, reads
#define DEBUG_EVENTS  0x0100U // as they happen: sync state changes, controls, sensor
#define DEBUG_ALL     0x01FFU

// Nothing by default: the ESP turns it on ("debug +gnss", say), or
// build with e.g. -DDEBUG_DEFAULT=DEBUG_ALL for a bench session.
#ifndef DEBUG_DEFAULT
#define DEBUG_DEFAULT DEBUG_NONE
#endif

void debug_init(void);
bool debug_enabled(uint16_t subsystems); // any of them
uint16_t debug_mask(void);
void debug_set(uint16_t mask);
// The mask, by name, on the console: "PIC debug: GNSS SYNC".
void debug_print(void);
// The names of the bits in a mask, "GNSS SYNC", or "NONE"; no line end.
void debug_print_names(uint16_t bits);

#endif // DEBUG_H
