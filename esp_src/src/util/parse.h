/*
 * Strict parsing for console arguments: the whole string must be the value,
 * and out-of-range values are rejected, never wrapped. Portable; no Arduino
 * dependency.
 */
#ifndef PARSE_H
#define PARSE_H

#include <stdbool.h>
#include <stdint.h>

bool parse_u32(const char *text, uint32_t minimum, uint32_t maximum, uint32_t *value);
bool parse_i32(const char *text, int32_t minimum, int32_t maximum, int32_t *value);
// 1/0, true/false, on/off, yes/no (any case).
bool parse_bool(const char *text, bool *value);

// A UTC offset or a DST amount: [+|-]h[:mm[:ss]] ("+1", "-5:30", "+05:45",
// "0", "-0:25:21"), or seconds with an s ("3600s", "-18000s"). Within
// +/- limit_s.
bool parse_offset(const char *text, int32_t limit_s, int32_t *offset_s);
// A time of day as seconds from midnight (0 to 86399): h:mm[:ss], or the
// seconds themselves, alone or with an s ("27000", "27000s").
bool parse_time_of_day(const char *text, uint32_t *seconds);
// A moment as Unix seconds: ISO 8601, loosely, or Unix seconds themselves.
// The date extended (2026-10-18) or basic (20261018); then, after T, t or
// spaces, the time extended (h[:mm[:ss]]) or basic (hhmm[ss]), any fraction
// of a second dropped; then, after any spaces, Z or an offset ((+|-)hh[:mm]
// or hhmm). 1970 to 2106-02-07. A date and time without a zone is a wall
// clock time: *local (if given) is set, and the seconds are that time as if
// it were UTC; the caller decides what that means. Unix seconds and times
// with a zone are UTC instants, *local false.
bool parse_moment(const char *text, uint32_t *utc_seconds, bool *local = nullptr);
// A duration in seconds: a number, alone or with s, m, h or d ("1024",
// "17m", "1h", "7d"), within minimum and maximum seconds.
bool parse_duration(const char *text, uint32_t minimum, uint32_t maximum, uint32_t *seconds);

#endif // PARSE_H
