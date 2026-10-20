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
// 2026-10-19T13:34:56+01:00
void time_print_local(Stream *output, uint32_t utc_seconds, int32_t offset_s);
// +01:00, -00:25:21 (seconds shown only when not whole minutes)
void time_print_offset(Stream *output, int32_t offset_s);
// 3d 04:05:06
void time_print_duration(Stream *output, uint32_t seconds);
// 1.234 ms, 56.7 us, 89 ns
void time_print_ns(Stream *output, int64_t ns);

#endif // TIME_TEXT_H
