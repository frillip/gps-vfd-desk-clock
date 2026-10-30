/*
 * UART1 byte transport for the debug console. See uart1.h for the
 * concurrency model. Each ring has exactly one writer of each index:
 *
 *   RX: ISR writes rx_ring[] and rx_head; foreground writes rx_tail.
 *   TX: foreground writes tx_ring[] and tx_head; the TX ISR writes tx_tail,
 *       or uart1_tx_poll() does with the TX interrupt masked.
 *
 * 16-bit index loads and stores are single instructions on the dsPIC, so
 * the indices need no locking. The compiler barrier before each index
 * publish stops the (non-volatile) ring data store being reordered after it.
 */
#include <xc.h>
#include <stddef.h>
#include "uart1.h"

#define RING_IS_POWER_OF_TWO(size) ((size) != 0U && ((size) & ((size) - 1U)) == 0U)
#if !RING_IS_POWER_OF_TWO(UART1_RX_RING_SIZE) || !RING_IS_POWER_OF_TWO(UART1_TX_RING_SIZE)
#error UART1 ring sizes must be powers of two
#endif
#define RX_MASK (UART1_RX_RING_SIZE - 1U)
#define TX_MASK (UART1_TX_RING_SIZE - 1U)

#define COMPILER_BARRIER() __asm__ volatile("" ::: "memory")

static uint8_t rx_ring[UART1_RX_RING_SIZE];
static uint8_t tx_ring[UART1_TX_RING_SIZE];
static volatile uint16_t rx_head;
static volatile uint16_t rx_tail;
static volatile uint16_t tx_head;
static volatile uint16_t tx_tail;
static volatile uint16_t rx_error_flags;
static volatile bool uart_enabled;
static volatile Uart1Stats driver_stats;

/* ---- Interrupt-enable masking ------------------------------------------
 * Only the three UART1 sources are masked, so timing interrupts stay live.
 * Every IEC write below assigns a constant, which XC16 emits as a single
 * BSET/BCLR. Assigning a variable to a bitfield can compile to a
 * read-modify-write of the whole register, which would lose a concurrent
 * change to another IEC0 bit: IEC0 also holds T1IE, T2IE, T3IE, IC1IE,
 * IC2IE and OC1IE, all used by the epoch timing. */
typedef struct
{
    bool rx;
    bool tx;
    bool error;
} Uart1InterruptMask;

static Uart1InterruptMask mask_interrupts(void)
{
    Uart1InterruptMask saved;
    saved.rx = IEC0bits.U1RXIE;
    saved.tx = IEC0bits.U1TXIE;
    saved.error = IEC4bits.U1EIE;
    IEC0bits.U1RXIE = 0;
    IEC0bits.U1TXIE = 0;
    IEC4bits.U1EIE = 0;
    return saved;
}


static void restore_interrupts(Uart1InterruptMask saved)
{
    if (saved.error)
    {
        IEC4bits.U1EIE = 1;
    }
    if (saved.rx)
    {
        IEC0bits.U1RXIE = 1;
    }
    if (saved.tx)
    {
        IEC0bits.U1TXIE = 1;
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
        IFS0bits.U1TXIF = 1;
        IEC0bits.U1TXIE = 1;
    }
}


// Fills every free hardware FIFO slot from the ring. Called by the TX ISR,
// or by uart1_tx_poll() with the TX interrupt masked; never both at once.
static void fill_tx_fifo(void)
{
    while (tx_tail != tx_head && !U1STAbits.UTXBF)
    {
        U1TXREG = tx_ring[tx_tail];
        tx_tail = (uint16_t)((tx_tail + 1U) & TX_MASK);
        driver_stats.tx_bytes++;
    }
}


// ---- Configuration -----------------------------------------------------
bool uart1_init(uint32_t instruction_hz, uint32_t baud_rate)
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

    uart1_disable();
    U1MODE = 0;
    U1STA = 0;
    U1MODEbits.BRGH = 1;
    U1BRG = (uint16_t)(divisor - 1U);
    U1STAbits.UTXISEL1 = 1; // UTXISEL = 10: interrupt when the TX FIFO empties
    U1STAbits.UTXISEL0 = 0;
    U1STAbits.URXISEL = 0; // interrupt on each received character

    rx_head = 0;
    rx_tail = 0;
    tx_head = 0;
    tx_tail = 0;
    rx_error_flags = 0;
    uart1_stats_clear();
    uart1_enable();
    return true;
}


void uart1_enable(void)
{
    IFS0bits.U1RXIF = 0;
    IFS0bits.U1TXIF = 0;
    IFS4bits.U1EIF = 0;
    U1MODEbits.UARTEN = 1;
    U1STAbits.UTXEN = 1; // must follow UARTEN
    uart_enabled = true;
    IEC0bits.U1RXIE = 1;
    IEC4bits.U1EIE = 1;
    start_tx();
    if (U1STAbits.URXDA)
    {
        IFS0bits.U1RXIF = 1;
    }
}


void uart1_disable(void)
{
    IEC0bits.U1RXIE = 0;
    IEC0bits.U1TXIE = 0;
    IEC4bits.U1EIE = 0;
    uart_enabled = false;
    U1STAbits.UTXEN = 0;
    U1MODEbits.UARTEN = 0;
}


bool uart1_is_enabled(void)
{
    return uart_enabled;
}


// ---- Receive -----------------------------------------------------------
uint16_t uart1_rx_available(void)
{
    return rx_count(rx_head, rx_tail);
}


bool uart1_rx_pop(uint8_t *byte)
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


uint16_t uart1_rx_read(uint8_t *destination, uint16_t limit)
{
    uint16_t bytes_read = 0;

    if (destination == NULL)
    {
        return 0;
    }
    while (bytes_read < limit && uart1_rx_pop(&destination[bytes_read]))
    {
        bytes_read++;
    }
    return bytes_read;
}


void uart1_rx_discard(void)
{
    Uart1InterruptMask saved = mask_interrupts();
    rx_tail = rx_head;
    restore_interrupts(saved);
}


bool uart1_rx_error_pending(void)
{
    return rx_error_flags != 0U;
}


uint16_t uart1_rx_recover(void)
{
    Uart1InterruptMask saved;
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
uint16_t uart1_tx_pending(void)
{
    return tx_count(tx_head, tx_tail);
}


uint16_t uart1_tx_free(void)
{
    return (uint16_t)(TX_MASK - uart1_tx_pending());
}


// Copies length bytes into the ring and publishes them. The caller has
// checked that they fit. The ISR only reads slots below tx_head, so the copy
// needs no masking.
static void tx_enqueue(const uint8_t *source, uint16_t length)
{
    uint16_t head = tx_head;
    uint16_t byte_index;
    uint16_t queued_bytes;
    Uart1InterruptMask saved;

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
}


bool uart1_tx_write(const uint8_t *source, uint16_t length)
{
    if (length == 0U)
    {
        return true;
    }
    if (!uart_enabled || source == NULL || length > uart1_tx_free())
    {
        driver_stats.tx_rejections++;
        return false;
    }
    tx_enqueue(source, length);
    return true;
}


uint16_t uart1_tx_write_some(const uint8_t *source, uint16_t length)
{
    uint16_t space;

    if (!uart_enabled || source == NULL || length == 0U)
    {
        return 0;
    }
    space = uart1_tx_free();
    if (length > space)
    {
        length = space;
    }
    if (length != 0U)
    {
        tx_enqueue(source, length);
    }
    return length;
}


void uart1_tx_poll(void)
{
    Uart1InterruptMask saved;

    if (!uart_enabled)
    {
        return;
    }
    saved = mask_interrupts(); // the TX ISR is the other tx_tail writer
    fill_tx_fifo();
    restore_interrupts(saved);
    start_tx();
}


bool uart1_tx_done(void)
{
    return tx_head == tx_tail && U1STAbits.TRMT;
}


// ---- Statistics --------------------------------------------------------
void uart1_stats(Uart1Stats *destination)
{
    Uart1InterruptMask saved;

    if (destination == NULL)
    {
        return;
    }
    saved = mask_interrupts();
    *destination = driver_stats;
    restore_interrupts(saved);
}


void uart1_stats_clear(void)
{
    static const Uart1Stats zero = {0};
    Uart1InterruptMask saved = mask_interrupts();
    driver_stats = zero;
    driver_stats.rx_high_water = rx_count(rx_head, rx_tail);
    driver_stats.tx_high_water = tx_count(tx_head, tx_tail);
    restore_interrupts(saved);
}


// ---- Interrupt handlers ------------------------------------------------

/* Drains the 4-deep hardware FIFO. Each byte's FERR/PERR flags describe the
 * byte at the top of the FIFO, so they are sampled before reading U1RXREG.
 * An overrun is cleared only after the FIFO has been drained, because
 * clearing OERR also resets the FIFO and would discard bytes still in it. */
void __attribute__((interrupt, no_auto_psv)) _U1RXInterrupt(void)
{
    IFS0bits.U1RXIF = 0;
    while (U1STAbits.URXDA)
    {
        bool framing_error = U1STAbits.FERR;
        bool parity_error = U1STAbits.PERR;
        uint8_t byte = (uint8_t)U1RXREG;
        uint16_t next_head;
        uint16_t queued_bytes;

        driver_stats.rx_bytes++;
        if (framing_error)
        {
            rx_error_flags |= UART1_RX_ERROR_FRAMING;
            driver_stats.framing_errors++;
        }
        if (parity_error)
        {
            rx_error_flags |= UART1_RX_ERROR_PARITY;
            driver_stats.parity_errors++;
        }
        if (framing_error || parity_error)
        {
            continue;
        }

        next_head = (uint16_t)((rx_head + 1U) & RX_MASK);
        if (next_head == rx_tail)
        {
            rx_error_flags |= UART1_RX_ERROR_OVERFLOW;
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
    if (U1STAbits.OERR)
    {
        rx_error_flags |= UART1_RX_ERROR_OVERRUN;
        driver_stats.overruns++;
        U1STAbits.OERR = 0;
    }
}


// Fills every free FIFO slot; disables itself once the ring is empty.
void __attribute__((interrupt, no_auto_psv)) _U1TXInterrupt(void)
{
    IFS0bits.U1TXIF = 0;
    fill_tx_fifo();
    if (tx_tail == tx_head)
    {
        IEC0bits.U1TXIE = 0;
    }
}


// Errors are accounted in the RX handler, which owns the FIFO. Hand over to
// it; an overrun with an empty FIFO still needs OERR cleared there.
void __attribute__((interrupt, no_auto_psv)) _U1ErrInterrupt(void)
{
    IFS4bits.U1EIF = 0;
    IFS0bits.U1RXIF = 1;
}
