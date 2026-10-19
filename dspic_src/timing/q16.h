/*
 * Q16.16 fixed-point helpers shared by the timing estimator and servo.
 *
 * Written for a 16-bit core: int is 16 bits on XC16, so every constant that
 * can exceed 16 bits is written with INT32_C/UINT32_C (exactly 32 bits on
 * XC16 and on a host), and every such value is held in int32_t/int64_t.
 *
 * Arithmetic policy:
 *   - State is held in int32_t Q16.16 timer ticks.
 *   - Products of two Q16 values are formed in int64_t (Q32) and rounded
 *     back to Q16 to nearest, ties away from zero, so rounding never biases
 *     the estimate in one direction.
 *   - The servo and period synthesis use no division at all. The Kalman
 *     gain needs one 64/32-bit division per second (about 50 us on a
 *     40 MIPS dsPIC33EP), which keeps the filter within 1e-6 tick of a
 *     double-precision reference.
 */
#ifndef Q16_H
#define Q16_H

#include <stdint.h>

#define Q16_SHIFT    16
#define Q16_ONE      INT32_C(65536)
#define Q16_HALF     INT32_C(32768)
#define Q16_FRACTION UINT32_C(0xFFFF)

// Largest whole-tick magnitude that converts to Q16 and back, and whose
// difference with another such value, still fits int32_t: +/-16383 ticks
// (+/-409 ppm of a 40 MHz count).
#define Q16_TICK_LIMIT INT32_C(16383)

#define Q16_FROM_TICKS(ticks) ((int32_t)((int32_t)(ticks) * Q16_ONE))
// Exact ratio as a Q16 constant, rounded to nearest.
#define Q16_RATIO(numerator, denominator) \
    ((int32_t)((((int64_t)(numerator) * Q16_ONE) + ((denominator) / 2)) / (denominator)))

// Rounds a Q32 value to Q16, to nearest with ties away from zero. Shifts
// only non-negative values, so the result does not depend on how the
// compiler shifts negative numbers.
static inline int32_t q16_round_from_q32(int64_t value_q32)
{
    if (value_q32 >= 0)
    {
        return (int32_t)((value_q32 + Q16_HALF) >> Q16_SHIFT);
    }
    return -(int32_t)(((-value_q32) + Q16_HALF) >> Q16_SHIFT);
}

// Rounds value / 2^shift to nearest, ties away from zero.
static inline int64_t q_round_shift(int64_t value, unsigned shift)
{
    int64_t half = (int64_t)1 << (shift - 1U);
    if (value >= 0)
    {
        return (value + half) >> shift;
    }
    return -(((-value) + half) >> shift);
}

// a * b for two Q16 values, rounded.
static inline int32_t q16_multiply(int32_t a_q16, int32_t b_q16)
{
    return q16_round_from_q32((int64_t)a_q16 * (int64_t)b_q16);
}

// Nearest whole tick, ties away from zero.
static inline int32_t q16_round_to_ticks(int32_t value_q16)
{
    if (value_q16 >= 0)
    {
        return (int32_t)(((uint32_t)value_q16 + (uint32_t)Q16_HALF) >> Q16_SHIFT);
    }
    return -(int32_t)(((uint32_t)(-value_q16) + (uint32_t)Q16_HALF) >> Q16_SHIFT);
}

static inline int32_t q16_clamp(int32_t value, int32_t limit)
{
    if (value > limit)
    {
        return limit;
    }
    if (value < -limit)
    {
        return -limit;
    }
    return value;
}

// Splits a signed Q16 value into floor(value) and its fraction (0..65535),
// so that value = whole * 65536 + fraction exactly. Portable: no shift of
// a negative number.
static inline void q16_split(int32_t value_q16, int32_t *whole, uint16_t *fraction)
{
    uint16_t low = (uint16_t)((uint32_t)value_q16 & Q16_FRACTION);
    *fraction = low;
    *whole = (value_q16 - (int32_t)low) / Q16_ONE; // exact: divisible
}

#endif // Q16_H
