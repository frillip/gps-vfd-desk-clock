/*
 * NXP PCF8563 real-time clock on I2C1 (address 0x51), as fitted to V2.
 * Ported from V2's pcf8563.c onto drivers/i2c1.h: nothing waits, the
 * DS1307 driver is retired, and the time is UTC seconds (platform/utc.h)
 * rather than a time_t from mktime().
 *
 * The RTC always holds UTC, never local time, as on V2: time zones and DST
 * are applied only for the display (ui/timezone.h), so a DST change never
 * touches the RTC. It cannot hold a leap second: across an inserted
 * 23:59:60 it simply counts on and is a second ahead until it is next set.
 *
 * One request at a time, started by pcf8563_read() or pcf8563_write() and
 * finished by pcf8563_service() from the main loop. A write is read back
 * once it is done, as V2 did, so the status always shows what the chip
 * holds. Reading the seconds register does not disturb the chip's own
 * second (the time registers are frozen during an access, which must end
 * within a second).
 *
 * What the chip says, beyond the time:
 *   - VL (bit 7 of the seconds register): the supply dropped below what the
 *     oscillator needs, so the time is not to be trusted. Set at power-on
 *     without a battery; cleared only by writing the seconds.
 *   - STOP (control/status 1): the clock is stopped and its prescaler held
 *     in reset. This driver clears it on every write. (The plan for an
 *     aligned write sets it on purpose: see the architecture notes.)
 *   - The century bit: 0 for 2000-2099 (what V2 wrote), 1 for 2100-2106.
 * The time is valid only with VL and STOP clear and a real calendar date.
 *
 * Aligned setting (sync/source_rtc.c drives it): pcf8563_write_stopped()
 * loads a time with STOP set, which holds the prescaler in reset, and
 * pcf8563_release() clears STOP; the chip's first seconds increment then
 * follows the release by PCF8563_START_DELAY_NS. Timing that release puts
 * the RTC's second where we want it.
 *
 * Every read is stamped with when its I2C start completed (read_stamp, on
 * the clock given to i2c1_set_stamp_clock()), so a run of reads can place
 * the RTC's seconds increment.
 *
 * Nothing here sets the clock's own time from the RTC, or the RTC from the
 * clock: that is the RTC source's job (sync/source_rtc.c).
 */
#ifndef PCF8563_H
#define PCF8563_H

#include <stdbool.h>
#include <stdint.h>

#include "i2c1.h"

#define PCF8563_ADDRESS 0x51U

// Registers (data sheet section 8). Only the first nine are used.
#define PCF8563_CONTROL_STATUS_1 0x00U
#define PCF8563_CONTROL_STATUS_2 0x01U
#define PCF8563_VL_SECONDS       0x02U
#define PCF8563_MINUTES          0x03U
#define PCF8563_HOURS            0x04U
#define PCF8563_DAYS             0x05U
#define PCF8563_WEEKDAYS         0x06U
#define PCF8563_CENTURY_MONTHS   0x07U
#define PCF8563_YEARS            0x08U
#define PCF8563_CLKOUT_CONTROL   0x0DU

#define PCF8563_STOP    0x20U // control/status 1
#define PCF8563_VL      0x80U // seconds
#define PCF8563_CENTURY 0x80U // century/months

/* From the release of STOP to the first seconds increment. Measured on two
 * V2 boards (2026-10-21, aligned writes timed against GNSS and NTP): about
 * 492.12 ms with the 70 us of the release itself, i.e. 63/128 s, where
 * NXP's PCA85063A data sheet gives 0.507813 to 0.507935 s (65/128) for its
 * prescaler. The RTC source measures the result of every aligned write and
 * corrects the next one, so this is only where it starts. */
#ifndef PCF8563_START_DELAY_NS
#define PCF8563_START_DELAY_NS 492187500UL // 63/128 s
#endif

// 2000-01-01T00:00:00Z: the chip's calendar starts here.
#define PCF8563_FIRST_UTC 946684800UL

// Retry a chip that did not answer this often (V2 gave up after 30 tries).
#define PCF8563_DETECT_INTERVAL_MS 10000UL
// Read a present chip this often, so what is printed is recent.
#define PCF8563_READ_INTERVAL_MS 60000UL

typedef enum
{
    PCF8563_NONE = 0, // no request yet
    PCF8563_READING,
    PCF8563_WRITING,
    PCF8563_WRITING_STOPPED, // pcf8563_write_stopped()
    PCF8563_RELEASING,       // pcf8563_release()
    PCF8563_DONE,            // the last request worked
    PCF8563_FAILED,          // the last request failed: see last_result
} Pcf8563Request;

typedef struct
{
    bool present;         // answered at its address on the last attempt
    bool calendar_valid;  // the last read held a real date and time (utc_seconds)
    bool time_valid;      // ... and VL and STOP were clear: the time can be used
    bool voltage_low;     // VL at the last read
    bool stopped;         // STOP at the last read
    bool century;         // the century bit at the last read
    uint32_t utc_seconds; // as last read, if calendar_valid
    uint32_t read_ms;     // the millisecond clock when that read finished
    uint32_t read_stamp;  // ... and when its I2C start completed (i2c1 stamp clock)
    uint8_t registers[9]; // control/status 1 to years, as last read
    Pcf8563Request request;
    I2cResult last_result;
    uint32_t reads;
    uint32_t writes;
    uint32_t releases; // STOP cleared by pcf8563_release()
    uint32_t failures;
} Pcf8563Status;

// Starts the first read. After i2c1_init().
void pcf8563_init(uint32_t now_ms);

// Starts reading the time. False if a request is still in progress.
bool pcf8563_read(void);
// Starts setting the time to utc_seconds (2000 to 2106), STOP and the
// control registers cleared; read back once done. False if a request is
// in progress or the time is out of range.
bool pcf8563_write(uint32_t utc_seconds);
// As pcf8563_write(), but with STOP set: the chip holds that time, its
// prescaler in reset, until pcf8563_release(). Not read back.
bool pcf8563_write_stopped(uint32_t utc_seconds);
// Clears STOP: a two-byte write, which takes effect at the end of its data
// byte, about 70 us after it starts on an idle bus. False if a request is in
// progress or the transfer could not be queued.
bool pcf8563_release(void);
bool pcf8563_busy(void);

// The clock's own time, for comparison in what is printed: the UTC second
// in progress, if known. The driver never changes either clock.
typedef struct
{
    uint32_t utc_seconds;
    bool known;
} Pcf8563Reference;

// Every pass of the main loop (cheap when there is nothing to do): finishes
// the request in progress, reads the chip every PCF8563_READ_INTERVAL_MS,
// and retries an absent one every PCF8563_DETECT_INTERVAL_MS. True once
// each time a read (or a write's read-back) completes, successfully or not.
bool pcf8563_service(uint32_t now_ms);

// True once after the first read and after each write's read-back has
// completed: worth a pcf8563_print, if the caller wants one.
bool pcf8563_report_due(void);

// The periodic reads, on by default: held off while the RTC source has the
// chip to itself (a search, an aligned write).
void pcf8563_set_auto_read(bool enabled);

const Pcf8563Status *pcf8563_status(void);

// The RTC's time now, from its last read and the time since: the last
// read plus the milliseconds since, rounded. The registers only give whole
// seconds and the read lands anywhere in the RTC's second, so this is the
// RTC's second to within one. False without a readable date.
bool pcf8563_now(uint32_t now_ms, uint32_t *utc_seconds);

// One line, or why not:
//   RTC: PCF8563, 2026-10-21T17:33:41Z, +0 s
// The offset is RTC minus the clock, to within a second (+ = RTC ahead).
// now_ms: the clock read_ms is on. verbose: when it was read, the counts,
// and the raw registers of a date that is not valid.
void pcf8563_print(uint32_t now_ms, Pcf8563Reference reference, bool verbose);

// For the tests.
uint8_t pcf8563_bcd_to_binary(uint8_t bcd);
uint8_t pcf8563_binary_to_bcd(uint8_t binary);

#endif // PCF8563_H
