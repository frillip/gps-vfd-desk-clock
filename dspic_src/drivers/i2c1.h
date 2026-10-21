/*
 * I2C1 master for the sensor and RTC bus, 400 kHz: the alternate pins
 * ASCL1 (RB8) and ASDA1 (RB9), selected by ALTI2C1 = ON (platform/system.c).
 *
 * Nothing here waits. A caller fills in an I2cTransfer it owns and queues
 * it; the master interrupt (MI2C1) carries it through byte by byte, a few
 * microseconds of work per byte, and the caller looks at the result later:
 *
 *   static uint8_t reg = 0x02;
 *   static uint8_t data[7];
 *   static I2cTransfer read = I2C_TRANSFER(0x51, &reg, 1, data, 7);
 *
 *   i2c1_submit(&read);            // false if it is still queued
 *   ...
 *   if (i2c1_done(&read) && read.result == I2C_OK) { ... }
 *
 * A transfer writes write_length bytes, then, if read_length is not zero,
 * sends a repeated start and reads read_length bytes, then stops. With both
 * lengths zero it only addresses the device: a probe. Transfers queue in
 * the order submitted; the next starts as soon as the last has stopped, from
 * the interrupt, so the bus stays busy without the main loop.
 *
 * i2c1_service() runs from the main loop: it ends a transfer that has taken
 * longer than I2C1_TIMEOUT_MS (a slave holding SCL, a lost interrupt) and
 * recovers the bus after that or after a collision, then starts the next.
 * Recovery takes the module off the pins, clocks SCL until a slave that was
 * part-way through a byte lets go of SDA, and makes a stop: about 100 us of
 * busy-waiting, only after a fault and once at i2c1_init(). A PIC reset in
 * the middle of a read leaves the slave holding SDA low; that is why init
 * recovers first.
 *
 * Concurrency: the queue is shared with the interrupt. i2c1_submit() and
 * i2c1_service() mask only MI2C1 while they change it, so the timing
 * interrupts stay live. A transfer belongs to the driver from i2c1_submit()
 * until i2c1_done() is true: the caller must not change it, its buffers or
 * the memory it lives in until then. Foreground only.
 */
#ifndef I2C1_H
#define I2C1_H

#include <stdbool.h>
#include <stdint.h>

#ifndef I2C1_SCL_HZ
#define I2C1_SCL_HZ 400000UL // as V2
#endif
// The longest a transfer may take, start to stop. A 10-byte transfer takes
// about 0.25 ms at 400 kHz; the service runs every 10 ms.
#define I2C1_TIMEOUT_MS 20U
// Lowest: the bus has no deadline that a few microseconds matter to.
#define I2C1_INTERRUPT_PRIORITY 1

typedef enum
{
    I2C_IDLE = 0,     // never submitted
    I2C_QUEUED,       // waiting for the bus
    I2C_BUSY,         // on the bus
    I2C_OK,           // done
    I2C_NACK_ADDRESS, // no device answered: absent, or busy (an EEPROM writing)
    I2C_NACK_DATA,    // the device refused a byte written to it
    I2C_BUS_ERROR,    // collision (another master, a glitch, SDA held low)
    I2C_TIMEOUT,      // did not finish in I2C1_TIMEOUT_MS
    I2C_RESULT_COUNT
} I2cResult;

typedef struct I2cTransfer
{
    uint8_t address;      // 7-bit
    const uint8_t *write; // written first: usually the register address
    uint8_t write_length;
    uint8_t *read; // then read into here after a repeated start
    uint8_t read_length;
    volatile I2cResult result;
    struct I2cTransfer *volatile next; // the queue, the driver's
    // When its start condition completed, by the clock given to
    // i2c1_set_stamp_clock() (0 without one): just before the address goes
    // out, so when a device that latches its registers on being addressed
    // (an RTC) took them, give or take a byte.
    volatile uint32_t start_stamp;
} I2cTransfer;

#define I2C_TRANSFER(address, write, write_length, read, read_length)             \
    {                                                                             \
        (address), (write), (write_length), (read), (read_length), I2C_IDLE, 0, 0 \
    }

typedef struct
{
    uint32_t transfers; // finished, any result
    uint32_t results[I2C_RESULT_COUNT];
    uint32_t recoveries;      // bus recoveries, init's included
    uint32_t stuck;           // recoveries that found SDA still low after them
    uint16_t recovery_clocks; // SCL pulses the last recovery needed
} I2c1Stats;

// Recovers the bus, configures the module and its interrupt. Call once,
// before the first submit; interrupts may already be on.
void i2c1_init(void);

// Queues a transfer. False, and nothing changes, if it is already queued or
// on the bus, or has nothing to address (an address over 0x7F, or a length
// with no buffer).
bool i2c1_submit(I2cTransfer *transfer);

// True once a submitted transfer has finished (its result is then final),
// and for one never submitted.
bool i2c1_done(const I2cTransfer *transfer);

// A free-running clock to stamp each transfer's start with (start_stamp),
// read from the interrupt: the capture timebase (pps_capture_timebase), so
// an RTC's second can be placed against the epoch. NULL: none.
void i2c1_set_stamp_clock(uint32_t (*clock)(void));

// From the main loop, every few ms: timeouts and recovery. now_ms is any
// millisecond clock (epoch_timer_ms()).
void i2c1_service(uint32_t now_ms);

const I2c1Stats *i2c1_stats(void);
const char *i2c1_result_name(I2cResult result);
// One line: transfers, failures by kind, recoveries.
void i2c1_print(void);

#endif // I2C1_H
