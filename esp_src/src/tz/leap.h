/*
 * SML-TIME-LEAP: the leap second in force, sent to the PIC.
 *
 * Source: the PIC's own GNSS (TIME-GNSS leap fields, from UBX-NAV-TIMELS),
 * held after the receiver is lost, which is when the PIC needs it: its
 * synchroniser uses TIME-LEAP whenever its selected source announces none.
 * tzinfo v1 carries no leap data, so there is no server source yet.
 *
 * Once the event has passed (by the ESP's clock) the held data is rolled
 * on: TAI-UTC takes the new value and nothing is pending.
 * Sent on change and every 10 min, to a PIC in SML mode.
 */
#ifndef LEAP_H
#define LEAP_H

#include <Arduino.h>

void leap_service(void);
void leap_print(Stream *output, bool verbose);

#endif // LEAP_H
