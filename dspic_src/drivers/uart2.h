/*
 * UART2 byte transport for the GNSS link (dsPIC33EP256GP504).
 *
 * Pin mapping (RPINR/RPOR) is set in board_init(); the interrupt
 * priorities by gnss_init(), which owns this link.
 *
 * Concurrency model: one foreground producer for TX, one foreground consumer
 * for RX, the UART ISRs on the other side of each ring. All public calls are
 * foreground-only. No application callbacks run in the ISRs.
 */
#ifndef UART2_H
#define UART2_H
#include <stdint.h>
#include <stdbool.h>

// Ring sizes must be powers of two. One slot always stays empty so that a
// full ring can be told apart from an empty one; usable capacity is size - 1.
// 512 bytes is about 44 ms of continuous traffic at 115200 baud.
#define UART2_RX_RING_SIZE 512U
#define UART2_TX_RING_SIZE 512U

// Receive error flags, accumulated until consumed by uart2_rx_recover().
#define UART2_RX_ERROR_OVERRUN  0x0001U // hardware FIFO overrun (OERR)
#define UART2_RX_ERROR_OVERFLOW 0x0002U // software ring full, byte dropped
#define UART2_RX_ERROR_FRAMING  0x0004U
#define UART2_RX_ERROR_PARITY   0x0008U

typedef struct
{
    uint32_t rx_bytes;
    uint32_t tx_bytes;
    uint32_t overruns;
    uint32_t rx_overflows;
    uint32_t framing_errors;
    uint32_t parity_errors;
    uint32_t tx_rejections;
    uint16_t rx_high_water;
    uint16_t tx_high_water;
} Uart2Stats;

// Configures 8N1 at the nearest achievable baud and enables the UART.
// Clears both rings and the error flags. instruction_hz is FCY (= FP on this
// device). Returns false if the baud rate cannot be generated.
bool uart2_init(uint32_t instruction_hz, uint32_t baud_rate);
void uart2_enable(void);
void uart2_disable(void); // Pauses the UART; does not flush the rings.

uint16_t uart2_rx_available(void);
bool uart2_rx_pop(uint8_t *byte);
uint16_t uart2_rx_read(uint8_t *destination, uint16_t limit);
void uart2_rx_discard(void);

// Cheap check, safe to call per byte: true if receive errors are pending.
bool uart2_rx_error_pending(void);
// Consumes the error flags atomically. If any were set, queued RX is
// discarded, because lost or damaged bytes invalidate any partial frame.
// Returns the consumed flags (0 if none).
uint16_t uart2_rx_recover(void);

uint16_t uart2_tx_free(void);
uint16_t uart2_tx_pending(void);
// All-or-nothing: queues every byte or none (and counts a rejection).
bool uart2_tx_write(const uint8_t *source, uint16_t length);
// True once the ring is empty and the last stop bit has left the pin.
bool uart2_tx_done(void);

void uart2_stats(Uart2Stats *destination);
void uart2_stats_clear(void);

// Intentionally no libc write(): debug output belongs to the SML protocol.

#endif
