// Synchroniser diagnostics on stdout (the UART1 console).
#ifndef SYNC_REPORT_H
#define SYNC_REPORT_H

#include "sync.h"

// One line per epoch: state, source and the UTC second of our epoch; plus a
// line for any state change.
void sync_report_epoch(const SyncUpdate *update);

// Full status: each source's record, the time of day and the recent state
// changes.
void sync_report_status(void);

#endif // SYNC_REPORT_H
