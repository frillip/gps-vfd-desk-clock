#ifndef TIME_MS_H
#define TIME_MS_H
#include <stdbool.h>
#include <stdint.h>
/* Millisecond ticks wrap every 2^32 ms (~49.71 days). Unsigned subtraction
 * measures elapsed time across that boundary: never compare absolute deadlines.
 * Use for short intervals and service active timers regularly. A complete
 * unobserved 49.71-day gap cannot be distinguished from no elapsed time.
 * This is a wrapping scheduler clock, not UTC or a persistent uptime counter.
 * Read the 32-bit tick atomically when its producer is an ISR on a 16-bit MCU. */
static inline uint32_t time_ms_elapsed(uint32_t now_ms, uint32_t previous_ms)
{
    return (uint32_t)(now_ms - previous_ms);
}

static inline bool time_ms_has_elapsed(uint32_t now_ms, uint32_t previous_ms, uint32_t interval_ms)
{
    return time_ms_elapsed(now_ms, previous_ms) >= interval_ms;
}

#endif
