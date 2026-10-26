/*
 * Local time: UTC plus offsets. The clock itself keeps UTC; changing time
 * zone never moves it (common/SML_PROTOCOL.md, TZ-STATE).
 *
 * Two sources of offsets:
 *
 *   schedule  from the ESP (SML-TZ-STATE): the entry in force now and up to
 *             8 future ones, each with its own base offset, DST amount and
 *             DST-active flag. The PIC knows no zone names or rules; it moves
 *             to each entry at its UTC second. The ESP builds it from its
 *             tzinfo lookups or from a zone configured on the ESP itself.
 *   manual    base offset, DST amount and DST active, from the settings.
 *
 * The latest valid schedule is always held. Two independent settings choose
 * what is taken from it:
 *
 *   TZ auto   the base offset (and any sub-second offset), else manual base
 *   DST auto  the DST amount and DST active, else the manual ones
 *
 * Either falls back to the manual setting while no schedule is held.
 *
 *   local = UTC + base + (DST active ? DST amount : 0)
 *
 * A schedule replaces the one held only if it is valid and well formed, and
 * either newer (issued_utc not older) or from a different source. Once UTC
 * passes the schedule's horizon_end_utc the last entry is held and the
 * offset is flagged stale; it never falls back to UTC on its own.
 *
 * The sub-second offset of special timebases (Mecca time) is rounded to the
 * nearest second: the display has no sub-second phase shift yet.
 *
 * Persistence: the schedule's CRC (without issued_utc, so periodic resends
 * of the same schedule do not count as changes) tells the settings store
 * when it needs writing; see timezone_take_changed(). It is stored as the
 * TZ-STATE it came in as (timezone_state), and at start-up goes back in
 * through timezone_apply(), checked like any other.
 */
#ifndef TIMEZONE_H
#define TIMEZONE_H

#include <stdbool.h>
#include <stdint.h>

#include "../../common/sml_proto.h"

// Offsets beyond a day are not time zones.
#define TIMEZONE_OFFSET_LIMIT_S 86399L

typedef struct
{
    uint32_t utc_seconds; // in force from here; 0: since before the data
    int32_t base_offset_s;
    int16_t dst_offset_s; // the zone's DST amount, even when not active
    bool dst_active;
} TimezoneEntry;

typedef struct
{
    bool held;      // a schedule has been applied since start
    uint8_t source; // SML_TZ_SOURCE_*
    bool ambiguous;
    uint32_t issued_utc;
    uint32_t horizon_end_utc; // 0xFFFFFFFF: open-ended
    int32_t subsecond_offset_ns;
    uint8_t count; // entries, the one in force at issue included
    TimezoneEntry entries[1U + SML_TZ_TRANSITION_MAX];
    uint16_t crc; // of the schedule without issued_utc
} TimezoneSchedule;

// The offsets in force at a given UTC second.
typedef struct
{
    int32_t offset_s; // UTC to local, all included
    int32_t base_offset_s;
    int32_t dst_offset_s; // the DST amount, even when not active
    bool dst_active;
    bool base_automatic; // base from the schedule, else the manual setting
    bool dst_automatic;  // DST from the schedule, else the manual settings
    bool stale;          // something taken from a schedule past its horizon
    bool ambiguous;      // ... from a schedule the ESP flagged as a best guess
} TimezoneOffset;

typedef enum
{
    TIMEZONE_APPLIED = 0,
    TIMEZONE_UNCHANGED, // applied, and the same schedule as before
    TIMEZONE_NOT_VALID, // VALID flag clear: the schedule held is kept
    TIMEZONE_OLDER,     // issued before the one held, same source
    TIMEZONE_MALFORMED, // count, order or offsets out of range
    TIMEZONE_RESULT_COUNT
} TimezoneResult;

typedef struct
{
    uint32_t results[TIMEZONE_RESULT_COUNT];
} TimezoneStats;

// TZ auto and DST auto on, manual offsets zero, no schedule: UTC until one
// arrives.
void timezone_init(void);

void timezone_set_auto(bool automatic);
bool timezone_is_auto(void);
void timezone_set_dst_auto(bool automatic);
bool timezone_is_dst_auto(void);
void timezone_set_manual(int32_t base_offset_s, int32_t dst_offset_s, bool dst_active);
// The manual settings, whether in use or not.
void timezone_manual(TimezoneOffset *offset);

// An SML-TZ-STATE payload, decoded (sml_decode).
TimezoneResult timezone_apply(const SML_TZ_STATE_PAYLOAD *state);

void timezone_offset_at(uint32_t utc_seconds, TimezoneOffset *offset);

const TimezoneSchedule *timezone_schedule(void);
// The schedule held, as the TZ-STATE that would set it: false if none.
bool timezone_state(SML_TZ_STATE_PAYLOAD *state);
// True once after each change to the schedule's contents (CRC): time to
// persist it. Never set by a resend of the same schedule.
bool timezone_take_changed(void);
const TimezoneStats *timezone_stats(void);

const char *timezone_result_name(TimezoneResult result);
// The offsets at this UTC second and the schedule held, for diagnostics.
void timezone_print(uint32_t utc_seconds);

#endif // TIMEZONE_H
