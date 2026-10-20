/*
 * A time zone schedule as the ESP holds it, from a tzinfo v1 response, and
 * the SML-TZ-STATE built from it. Portable (ArduinoJson only), so the host
 * tests can run it.
 *
 * tzinfo v1 (GET /smol_clock/api/v1/tzinfo) returns the state in force and
 * up to TZINFO_MAX_TRANSITIONS (8 by default) future states, but no horizon.
 * The horizon (the first transition NOT covered) is therefore taken as:
 *   - next[8], if the server sent more than 8: exact;
 *   - the last entry + 1 s, if it sent exactly 8: it may have stopped
 *     there, so coverage is only certain up to the last entry;
 *   - open-ended (0xFFFFFFFF), if it sent fewer: its 5-year window held no
 *     more transitions.
 * The schedule is refreshed weekly at most, long before any of these.
 *
 * Building TZ-STATE at a known UTC time moves entries already in force into
 * `current`, so the PIC always gets the most future entries that fit.
 */
#ifndef TZ_SCHEDULE_H
#define TZ_SCHEDULE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

extern "C"
{
#include "../common/sml_proto.h"
}

#define TZ_SCHEDULE_ENTRIES    (1 + SML_TZ_TRANSITION_MAX)
#define TZ_SCHEDULE_ZONE_BYTES 48
#define TZ_SCHEDULE_OFFSET_MAX 86399

typedef struct
{
    uint32_t utc_seconds; // in force from; 0 = since before the data
    int32_t base_offset_s;
    int16_t dst_offset_s; // the DST amount, even when not active
    bool dst_active;
} TzEntry;

typedef struct
{
    bool valid;
    bool ambiguous;
    uint8_t source; // SML_TZ_SOURCE_*
    char zone[TZ_SCHEDULE_ZONE_BYTES];
    uint32_t generated_utc;   // server time of the answer
    uint32_t refresh_utc;     // ask again by then (v1 valid_until)
    uint32_t horizon_end_utc; // first transition not covered; 0xFFFFFFFF none known
    int32_t subsecond_offset_ns;
    uint8_t count; // entries used, current included
    TzEntry entries[TZ_SCHEDULE_ENTRIES];
} TzSchedule;

typedef enum
{
    TZ_PARSE_OK = 0,
    TZ_PARSE_JSON,         // not JSON
    TZ_PARSE_SERVER_ERROR, // an error object; its message is in error
    TZ_PARSE_VERSION,      // not protocol version 1
    TZ_PARSE_FIELD,        // a field missing or of the wrong type
    TZ_PARSE_RANGE,        // an offset out of range
    TZ_PARSE_ORDER,        // transitions not strictly ascending
    TZ_PARSE_COUNT
} TzParseResult;

TzParseResult tz_schedule_parse_v1(const char *json, size_t length, TzSchedule *schedule,
                                   char *error, size_t error_size);
const char *tz_parse_name(TzParseResult result);

// The entry in force at utc_seconds.
const TzEntry *tz_schedule_entry_at(const TzSchedule *schedule, uint32_t utc_seconds);
int32_t tz_entry_offset(const TzEntry *entry);

// TZ-STATE for the schedule; have_now: utc_now is known, so entries already in
// force are folded into current.
void tz_schedule_state(const TzSchedule *schedule, bool have_now, uint32_t utc_now,
                       SML_TZ_STATE_PAYLOAD *state);

// Same contents (zone, source, horizon, entries), ignoring the times issued.
bool tz_schedule_same(const TzSchedule *a, const TzSchedule *b);

#endif // TZ_SCHEDULE_H
