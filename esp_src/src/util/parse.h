/*
 * Strict number and flag parsing for console arguments: the whole string
 * must be the value, and out-of-range values are rejected, never wrapped.
 * Portable; no Arduino dependency.
 */
#ifndef PARSE_H
#define PARSE_H

#include <stdbool.h>
#include <stdint.h>

bool parse_u32(const char *text, uint32_t minimum, uint32_t maximum, uint32_t *value);
bool parse_i32(const char *text, int32_t minimum, int32_t maximum, int32_t *value);
// 1/0, true/false, on/off, yes/no (any case).
bool parse_bool(const char *text, bool *value);

#endif // PARSE_H
