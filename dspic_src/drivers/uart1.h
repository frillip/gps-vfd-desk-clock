/*
 * UART1 byte transport for the ESP32 link (dsPIC33EP256GP504): SML frames
 * and console text.
 *
 * Port of the V2+ UART2 driver (drivers/uart2.c on dev/v2plus) to UART1,
 * replacing the MCC-generated UART2 driver this test build used. Pin mapping
 * (RPINR18/RPOR) is set in pin_manager.c; interrupt priorities are set by
 * the caller (see ui/console.c).
 *
 * Concurrency model: one foreground producer for TX, one foreground consumer
 * for RX, the UART ISRs on the other side of each ring. All public calls are
 * foreground-only. No application callbacks run in the ISRs.
 */
#ifndef UART1_H
#define UART1_H
#include <stdint.h>
#include <stdbool.h>

/* Ring sizes must be powers of two. One slot always stays empty so that a
 * full ring can be told apart from an empty one; usable capacity is size - 1.
 * TX: 1 KB, about 89 ms of output at 115200 baud, holds a burst of status
 * lines without stalling the main loop (11 ms at 921600). RX: 1 KB, 11 ms at
 * 921600: more than the 10 ms between ESP link services, and several whole
 * SML frames. */
#define UART1_RX_RING_SIZE 1024U
#define UART1_TX_RING_SIZE 1024U

// Receive error flags, accumulated until consumed by uart1_rx_recover().
#define UART1_RX_ERROR_OVERRUN  0x0001U // hardware FIFO overrun (OERR)
#define UART1_RX_ERROR_OVERFLOW 0x0002U // software ring full, byte dropped
#define UART1_RX_ERROR_FRAMING  0x0004U
#define UART1_RX_ERROR_PARITY   0x0008U

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
} Uart1Stats;

// Configures 8N1 at the nearest achievable baud and enables the UART.
// Clears both rings and the error flags. instruction_hz is FCY (= FP on this
// device). Returns false if the baud rate cannot be generated.
bool uart1_init(uint32_t instruction_hz, uint32_t baud_rate);
void uart1_enable(void);
void uart1_disable(void); // Pauses the UART; does not flush the rings.
bool uart1_is_enabled(void);

uint16_t uart1_rx_available(void);
bool uart1_rx_pop(uint8_t *byte);
uint16_t uart1_rx_read(uint8_t *destination, uint16_t limit);
void uart1_rx_discard(void);

// Cheap check, safe to call per byte: true if receive errors are pending.
bool uart1_rx_error_pending(void);
// Consumes the error flags atomically. If any were set, queued RX is
// discarded, because lost or damaged bytes invalidate any partial frame.
// Returns the consumed flags (0 if none).
uint16_t uart1_rx_recover(void);

uint16_t uart1_tx_free(void);
uint16_t uart1_tx_pending(void);
// All-or-nothing: queues every byte or none (and counts a rejection).
bool uart1_tx_write(const uint8_t *source, uint16_t length);
// Queues as many bytes as fit, possibly none. Returns the number queued.
uint16_t uart1_tx_write_some(const uint8_t *source, uint16_t length);
/* Moves queued bytes into the hardware FIFO from the foreground, as the TX
 * interrupt would. For callers that must wait for ring space while the TX
 * interrupt may be held off (inside DISI, or at an equal or higher CPU
 * priority). Never waits itself. */
void uart1_tx_poll(void);
// True once the ring is empty and the last stop bit has left the pin.
bool uart1_tx_done(void);

void uart1_stats(Uart1Stats *destination);
void uart1_stats_clear(void);

#endif
