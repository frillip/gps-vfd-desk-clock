// Synchroniser diagnostics on stdout (the UART1 console).
#ifndef SYNC_REPORT_H
#define SYNC_REPORT_H

#include "sync.h"

// A line for a state change, if there was one this epoch. Printed whatever
// the debug settings: changes are rare and always worth seeing.
void sync_report_transition(const SyncUpdate *update);

// One line per epoch: state, source and the UTC second of our epoch.
void sync_report_epoch(const SyncUpdate *update);

// One line per epoch: every candidate, best first, with its bound, verdict
// and phase; * marks the one in use, + the second choice.
void sync_report_sources(void);

// The state and what it steers by, and the time of day. verbose: the
// candidates ranked, each source's record and the recent state changes.
void sync_report_status(bool verbose);

#endif // SYNC_REPORT_H
