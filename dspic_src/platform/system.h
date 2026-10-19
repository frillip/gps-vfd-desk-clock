#ifndef SYSTEM_H
#define SYSTEM_H

#include <stdint.h>
#include <stdbool.h>

#include "clock.h" // FCY: must precede <libpic30.h>, whose __delay_us()
                   // and __delay_ms() macros exist only if FCY is set
#include <libpic30.h>
#include <xc.h>
#include "board.h"

void system_init(void);

void delay_ms(uint16_t milliseconds);
void delay_us(uint16_t microseconds);

// Software reset (RESET instruction): restarts from the reset vector.
void system_reset(void) __attribute__((noreturn));

#endif // SYSTEM_H
