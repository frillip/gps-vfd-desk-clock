/*
 * UART2 byte transport for the GNSS link. See uart2.h for the concurrency
 * model. Each ring has exactly one writer of each index:
 *
 *   RX: ISR writes rx_ring[] and rx_head; foreground writes rx_tail.
 *   TX: foreground writes tx_ring[] and tx_head; ISR writes tx_tail.
 *
 * 16-bit index loads and stores are single instructions on the dsPIC, so
 * the indices need no locking. The compiler barrier before each index
 * publish stops the (non-volatile) ring data store being reordered after it.
 */
#include <xc.h>
#include <stddef.h>
#include "uart2.h"

#define RING_IS_POWER_OF_TWO(size) ((size) != 0U && ((size) & ((size) - 1U)) == 0U)
#if !RING_IS_POWER_OF_TWO(UART2_RX_RING_SIZE) || !RING_IS_POWER_OF_TWO(UART2_TX_RING_SIZE)
#error UART2 ring sizes must be powers of two
#endif
#define RX_MASK (UART2_RX_RING_SIZE - 1U)
#define TX_MASK (UART2_TX_RING_SIZE - 1U)

#define COMPILER_BARRIER() __asm__ volatile("" ::: "memory")

static uint8_t rx_ring[UART2_RX_RING_SIZE];
static uint8_t tx_ring[UART2_TX_RING_SIZE];
static volatile uint16_t rx_head;
static volatile uint16_t rx_tail;
static volatile uint16_t tx_head;
static volatile uint16_t tx_tail;
static volatile uint16_t rx_error_flags;
static volatile bool uart_enabled;
static volatile Uart2Stats driver_stats;

/* ---- Interrupt-enable masking ------------------------------------------
 * Only the three UART2 sources are masked, so timing interrupts stay live.
 * Every IEC write below assigns a constant, which XC16 emits as a single
 * BSET/BCLR. Assigning a variable to a bitfield can compile to a
 * read-modify-write of the whole register, which would lose a concurrent
 * change to another IEC1/IEC4 bit (T4IE and T5IE share IEC1). */
typedef struct
{
    bool rx;
    bool tx;
    bool error;
} Uart2InterruptMask;

static Uart2InterruptMask mask_interrupts(void)
{
    Uart2InterruptMask saved;
    saved.rx = IEC1bits.U2RXIE;
    saved.tx = IEC1bits.U2TXIE;
    saved.error = IEC4bits.U2EIE;
    IEC1bits.U2RXIE = 0;
    IEC1bits.U2TXIE = 0;
    IEC4bits.U2EIE = 0;
    return saved;
}


static void restore_interrupts(Uart2InterruptMask saved)
{
    if (saved.error)
    {
        IEC4bits.U2EIE = 1;
    }
    if (saved.rx)
    {
        IEC1bits.U2RXIE = 1;
    }
    if (saved.tx)
    {
        IEC1bits.U2TXIE = 1;
    }
}


static uint16_t rx_count(uint16_t head, uint16_t tail)
{
    return (uint16_t)((head - tail) & RX_MASK);
}


static uint16_t tx_count(uint16_t head, uint16_t tail)
{
    return (uint16_t)((head - tail) & TX_MASK);
}


// Starts (or restarts) transmission after bytes are published. Setting the
// flag by hand also covers the race where the ISR emptied the ring and
// disabled itself between the publish and this call.
static void start_tx(void)
{
    if (uart_enabled && tx_head != tx_tail)
    {
        IFS1bits.U2TXIF = 1;
        IEC1bits.U2TXIE = 1;
    }
}


// ---- Configuration -----------------------------------------------------
bool uart2_init(uint32_t instruction_hz, uint32_t baud_rate)
{
    uint32_t denominator;
    uint32_t divisor;

    if (baud_rate == 0U || baud_rate > UINT32_MAX / 4U)
    {
        return false;
    }
    // BRGH = 1: baud = FCY / (4 * (UxBRG + 1)). Round to the nearest divisor
    // without forming instruction_hz + 2 * baud_rate, which could overflow.
    denominator = baud_rate * 4U;
    divisor = instruction_hz / denominator;
    if (instruction_hz % denominator >= denominator / 2U)
    {
        divisor++;
    }
    if (divisor == 0U || divisor > 65536UL)
    {
        return false;
    }

    uart2_disable();
    U2MODE = 0;
    U2STA = 0;
    U2MODEbits.BRGH = 1;
    U2BRG = (uint16_t)(divisor - 1U);
    U2STAbits.UTXISEL1 = 1; // UTXISEL = 10: interrupt when the TX FIFO empties
    U2STAbits.UTXISEL0 = 0;
    U2STAbits.URXISEL = 0; // interrupt on each received character

    rx_head = 0;
    rx_tail = 0;
    tx_head = 0;
    tx_tail = 0;
    rx_error_flags = 0;
    uart2_stats_clear();
    uart2_enable();
    return true;
}


void uart2_enable(void)
{
    IFS1bits.U2RXIF = 0;
    IFS1bits.U2TXIF = 0;
    IFS4bits.U2EIF = 0;
    U2MODEbits.UARTEN = 1;
    U2STAbits.UTXEN = 1; // must follow UARTEN
    uart_enabled = true;
    IEC1bits.U2RXIE = 1;
    IEC4bits.U2EIE = 1;
    start_tx();
    if (U2STAbits.URXDA)
    {
        IFS1bits.U2RXIF = 1;
    }
}


void uart2_disable(void)
{
    IEC1bits.U2RXIE = 0;
    IEC1bits.U2TXIE = 0;
    IEC4bits.U2EIE = 0;
    uart_enabled = false;
    U2STAbits.UTXEN = 0;
    U2MODEbits.UARTEN = 0;
}


// ---- Receive -----------------------------------------------------------
uint16_t uart2_rx_available(void)
{
    return rx_count(rx_head, rx_tail);
}


bool uart2_rx_pop(uint8_t *byte)
{
    uint16_t tail = rx_tail;

    if (byte == NULL || tail == rx_head)
    {
        return false;
    }
    COMPILER_BARRIER(); // read the data only after seeing the new head
    *byte = rx_ring[tail];
    COMPILER_BARRIER(); // finish reading before releasing the slot
    rx_tail = (uint16_t)((tail + 1U) & RX_MASK);
    return true;
}


uint16_t uart2_rx_read(uint8_t *destination, uint16_t limit)
{
    uint16_t bytes_read = 0;

    if (destination == NULL)
    {
        return 0;
    }
    while (bytes_read < limit && uart2_rx_pop(&destination[bytes_read]))
    {
        bytes_read++;
    }
    return bytes_read;
}


void uart2_rx_discard(void)
{
    Uart2InterruptMask saved = mask_interrupts();
    rx_tail = rx_head;
    restore_interrupts(saved);
}


bool uart2_rx_error_pending(void)
{
    return rx_error_flags != 0U;
}


uint16_t uart2_rx_recover(void)
{
    Uart2InterruptMask saved;
    uint16_t errors;

    if (rx_error_flags == 0U)
    {
        return 0; // fast path: no masking when nothing is pending
    }
    saved = mask_interrupts();
    errors = rx_error_flags;
    rx_tail = rx_head;
    rx_error_flags = 0;
    restore_interrupts(saved);
    return errors;
}


// ---- Transmit ----------------------------------------------------------
uint16_t uart2_tx_pending(void)
{
    return tx_count(tx_head, tx_tail);
}


uint16_t uart2_tx_free(void)
{
    return (uint16_t)(TX_MASK - uart2_tx_pending());
}


bool uart2_tx_write(const uint8_t *source, uint16_t length)
{
    uint16_t head = tx_head;
    uint16_t byte_index;
    uint16_t queued_bytes;
    Uart2InterruptMask saved;

    if (length == 0U)
    {
        return true;
    }
    if (!uart_enabled || source == NULL || length > uart2_tx_free())
    {
        driver_stats.tx_rejections++;
        return false;
    }
    // The ISR only reads slots below tx_head, so the copy needs no masking.
    for (byte_index = 0; byte_index < length; byte_index++)
    {
        tx_ring[head] = source[byte_index];
        head = (uint16_t)((head + 1U) & TX_MASK);
    }
    COMPILER_BARRIER(); // publish only after every byte has been copied
    tx_head = head;

    saved = mask_interrupts(); // stats are also written by the ISRs
    queued_bytes = tx_count(tx_head, tx_tail);
    if (queued_bytes > driver_stats.tx_high_water)
    {
        driver_stats.tx_high_water = queued_bytes;
    }
    restore_interrupts(saved);
    start_tx();
    return true;
}


bool uart2_tx_done(void)
{
    return tx_head == tx_tail && U2STAbits.TRMT;
}


// ---- Statistics --------------------------------------------------------
void uart2_stats(Uart2Stats *destination)
{
    Uart2InterruptMask saved;

    if (destination == NULL)
    {
        return;
    }
    saved = mask_interrupts();
    *destination = driver_stats;
    restore_interrupts(saved);
}


void uart2_stats_clear(void)
{
    static const Uart2Stats zero = {0};
    Uart2InterruptMask saved = mask_interrupts();
    driver_stats = zero;
    driver_stats.rx_high_water = rx_count(rx_head, rx_tail);
    driver_stats.tx_high_water = tx_count(tx_head, tx_tail);
    restore_interrupts(saved);
}


// ---- Interrupt handlers ------------------------------------------------

/* Drains the 4-deep hardware FIFO. Each byte's FERR/PERR flags describe the
 * byte at the top of the FIFO, so they are sampled before reading U2RXREG.
 * An overrun is cleared only after the FIFO has been drained, because
 * clearing OERR also resets the FIFO and would discard bytes still in it. */
void __attribute__((interrupt, no_auto_psv)) _U2RXInterrupt(void)
{
    IFS1bits.U2RXIF = 0;
    while (U2STAbits.URXDA)
    {
        bool framing_error = U2STAbits.FERR;
        bool parity_error = U2STAbits.PERR;
        uint8_t byte = (uint8_t)U2RXREG;
        uint16_t next_head;
        uint16_t queued_bytes;

        driver_stats.rx_bytes++;
        if (framing_error)
        {
            rx_error_flags |= UART2_RX_ERROR_FRAMING;
            driver_stats.framing_errors++;
        }
        if (parity_error)
        {
            rx_error_flags |= UART2_RX_ERROR_PARITY;
            driver_stats.parity_errors++;
        }
        if (framing_error || parity_error)
        {
            continue;
        }

        next_head = (uint16_t)((rx_head + 1U) & RX_MASK);
        if (next_head == rx_tail)
        {
            rx_error_flags |= UART2_RX_ERROR_OVERFLOW;
            driver_stats.rx_overflows++;
            continue;
        }
        rx_ring[rx_head] = byte;
        COMPILER_BARRIER(); // data before index
        rx_head = next_head;

        queued_bytes = rx_count(next_head, rx_tail);
        if (queued_bytes > driver_stats.rx_high_water)
        {
            driver_stats.rx_high_water = queued_bytes;
        }
    }
    if (U2STAbits.OERR)
    {
        rx_error_flags |= UART2_RX_ERROR_OVERRUN;
        driver_stats.overruns++;
        U2STAbits.OERR = 0;
    }
}


// Fills every free FIFO slot; disables itself once the ring is empty.
void __attribute__((interrupt, no_auto_psv)) _U2TXInterrupt(void)
{
    IFS1bits.U2TXIF = 0;
    while (tx_tail != tx_head && !U2STAbits.UTXBF)
    {
        U2TXREG = tx_ring[tx_tail];
        tx_tail = (uint16_t)((tx_tail + 1U) & TX_MASK);
        driver_stats.tx_bytes++;
    }
    if (tx_tail == tx_head)
    {
        IEC1bits.U2TXIE = 0;
    }
}


// Errors are accounted in the RX handler, which owns the FIFO. Hand over to
// it; an overrun with an empty FIFO still needs OERR cleared there.
void __attribute__((interrupt, no_auto_psv)) _U2ErrInterrupt(void)
{
    IFS4bits.U2EIF = 0;
    IFS1bits.U2RXIF = 1;
}
