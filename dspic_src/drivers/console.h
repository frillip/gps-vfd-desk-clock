// Version: 2026-10-18T17:00:00+01:00
/*
 * Debug console on UART1: routes the C library's stdout and stderr (printf,
 * puts, putchar) to the UART1 driver.
 *
 * XC16's libc sends all stream output through write(); console.c provides
 * that function in the .libc.write section, which replaces the library's
 * default (the simulator's UART). Nothing else is needed for the existing
 * printf calls to work.
 *
 * Output policy: write() blocks until every byte is queued. The 1 KB TX ring
 * absorbs the per-second status burst, so in normal running it never waits.
 * If it does wait, it moves bytes into the UART itself (UART1_TxPoll), so it
 * cannot deadlock when called with the UART interrupt held off, for example
 * inside a __builtin_disi() window. Interrupts are never disabled by it, so
 * the epoch timing runs undisturbed while it waits.
 *
 * Foreground use only: do not printf from an interrupt handler.
 */
#ifndef CONSOLE_H
#define CONSOLE_H
#include <stdint.h>
#include <stdbool.h>

#define CONSOLE_BAUD_RATE           115200UL
/* Below the timing interrupts (4) and alongside the captures (1). */
#define CONSOLE_INTERRUPT_PRIORITY  1

/* Sets the UART1 interrupt priorities and starts the UART. Output written
 * before this is discarded. Returns false if the baud rate is unreachable. */
bool console_init(uint32_t instruction_hz);

/* Times write() had to wait for ring space, for diagnostics. */
uint32_t console_stall_count(void);

/* Waits until everything queued has left the pin, e.g. before a reset. */
void console_flush(void);

#endif
