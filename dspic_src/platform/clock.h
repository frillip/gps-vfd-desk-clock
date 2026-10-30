/*
 * System clock. One place for the oscillator frequency; everything that
 * needs a rate (delays, UART baud, the 1 s epoch) derives it from here.
 *
 * 40 MHz EC input -> PLL (N1 = 8, M = 64, N2 = 4) -> Fosc 80 MHz, set up by
 * system_init(). FCY = Fosc / 2, and the peripheral clock FP equals FCY on
 * this device, so every timer, capture and compare module counts at FCY.
 *
 * Kept free of <xc.h> and <libpic30.h> so the timing code can include it on
 * the host (tests/host).
 */
#ifndef PLATFORM_CLOCK_H
#define PLATFORM_CLOCK_H

#define SYSTEM_FOSC_HZ 80000000UL
#define FCY            (SYSTEM_FOSC_HZ / 2UL)

#endif // PLATFORM_CLOCK_H
