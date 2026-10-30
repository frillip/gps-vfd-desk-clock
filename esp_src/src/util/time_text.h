/*
 * Times as text: ISO 8601 UTC, local time with its offset, and durations.
 * Unix seconds are uint32_t (to 2106), as on the wire.
 */
#ifndef TIME_TEXT_H
#define TIME_TEXT_H

#include <Arduino.h>
#include <stdint.h>

// 2026-10-19T12:34:56Z
void time_print_utc(Stream *output, uint32_t utc_seconds);
// 2026-10-19T13:34:56: a wall-clock time, given as a scalar like UTC's
void time_print_wall(Stream *output, uint32_t local_seconds);
// 2026-10-19T13:34:56+01:00
void time_print_local(Stream *output, uint32_t utc_seconds, int32_t offset_s);
// +01:00, -00:25:21 (seconds shown only when not whole minutes)
void time_print_offset(Stream *output, int32_t offset_s);
// 3d 04:05:06
void time_print_duration(Stream *output, uint32_t seconds);
// An age as chronyc prints one: 12s, 5m, 3h, 2d; "never" for UINT32_MAX.
#define TIME_AGE_TEXT 12
void time_format_age(char *text, uint32_t seconds);
// A time interval to four significant figures, in the largest unit that
// leaves a whole part: 89 ns, 56.70 us, 5.185 ms, 11.43 ms, 1.250 s (whole
// seconds past 999 s). plus: a + for positive values. time_format_ns
// writes it to text (TIME_NS_TEXT bytes).
#define TIME_NS_TEXT 32
void time_format_ns(char *text, int64_t ns, bool plus);
void time_print_ns(Stream *output, int64_t ns);

#endif // TIME_TEXT_H
