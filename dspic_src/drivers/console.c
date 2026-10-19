// Version: 2026-10-18T17:00:00+01:00
#include <xc.h>
#include "console.h"
#include "uart1.h"

#include <stddef.h>

static uint32_t stalls;

bool console_init(uint32_t instruction_hz)
{
    IPC2bits.U1RXIP = CONSOLE_INTERRUPT_PRIORITY;
    IPC3bits.U1TXIP = CONSOLE_INTERRUPT_PRIORITY;
    IPC16bits.U1EIP = CONSOLE_INTERRUPT_PRIORITY;
    return UART1_Initialize(instruction_hz, CONSOLE_BAUD_RATE);
}

uint32_t console_stall_count(void)
{
    return stalls;
}

void console_flush(void)
{
    while (UART1_IsEnabled() && !UART1_IsTxDone())
    {
        UART1_TxPoll();
    }
}

/*
 * The libc output primitive. handle is 1 (stdout) or 2 (stderr); both go to
 * the console. Returns len: every byte is either queued or, before
 * console_init(), deliberately dropped, so the library never sees a short
 * write it would have to retry.
 */
int __attribute__((__section__(".libc.write"))) write(int handle, void *buffer, unsigned int len)
{
    const uint8_t *next = (const uint8_t *)buffer;
    unsigned int remaining = len;
    bool stalled = false;

    (void)handle;
    if (!UART1_IsEnabled() || buffer == NULL)
    {
        return (int)len;
    }
    while (remaining != 0U)
    {
        uint16_t queued = UART1_TxWriteSome(next, (uint16_t)remaining);

        next += queued;
        remaining -= queued;
        if (remaining != 0U)
        {
            /* Ring full: push bytes into the UART ourselves, which also
             * works while its interrupt cannot run. */
            if (!stalled)
            {
                stalled = true;
                stalls++;
            }
            UART1_TxPoll();
        }
    }
    return (int)len;
}
