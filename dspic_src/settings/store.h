/*
 * The settings store: a journal of records in the two flash pages reserved
 * for it (drivers/flash.h), apart from the application, so it survives
 * reprogramming and updates.
 *
 * A record is a type (1 to STORE_TYPES - 1) and up to STORE_RECORD_MAX_BYTES
 * of data; the store keeps the latest of each type. What the data means is
 * up to its owner (settings/settings.c): the store only keeps it intact.
 *
 * Layout, in 16-bit data words (one per instruction), each page:
 *
 *   header   STORE_MAGIC, STORE_FORMAT, generation, CRC of those three
 *   records  one after another from word 4, each:
 *              STORE_RECORD_MARK | type, length in bytes,
 *              the data, two bytes a word (low byte first),
 *              zero padding to an even number of words,
 *              CRC of everything before it
 *   free     erased (0xFFFF) to the end of the page
 *
 * The page in use is the one with a good header and the later generation.
 * A new record is appended to it, a double word at a time, the CRC last, so
 * a record cut short by a reset fails its CRC and is passed over: the one
 * before it stands. When the page is full (or holds something unreadable),
 * the other page is erased and the latest record of each type is written to
 * it, the new one included, and only then its header, with the next
 * generation: until that header is written the old page is still the one
 * in use, whole. A page is erased only when the other fills up, so with the
 * records written a few times a year each page lasts far beyond the flash's
 * 10,000 erase cycles.
 *
 * Records of a type this firmware does not know are passed over when read,
 * and dropped at the next change of page.
 *
 * Erasing and writing stop the CPU (drivers/flash.h): call store_write() at
 * a quiet moment. Everything else only reads.
 */
#ifndef STORE_H
#define STORE_H

#include <stdbool.h>
#include <stdint.h>

#include "../drivers/flash.h"

#define STORE_MAGIC            0x5354U // "ST"
#define STORE_FORMAT           1U
#define STORE_HEADER_WORDS     4U
#define STORE_RECORD_MARK      0xA500U
#define STORE_RECORD_MAX_BYTES 128U
#define STORE_TYPES            4U // types 1-3

typedef enum
{
    STORE_OK = 0,
    STORE_BAD_ARGUMENT, // type or length out of range
    STORE_FLASH_FAILED, // erasing or writing failed (FlashResult)
    STORE_VERIFY,       // written, but did not read back the same
} StoreResult;

typedef struct
{
    bool active;  // a page is in use (false: nothing stored yet)
    uint8_t page; // 0 or 1
    uint16_t generation;
    uint16_t used_words; // header and records, of FLASH_PAGE_WORDS
    bool full;           // the next write changes page
    // Found when the page was read at start-up.
    uint16_t torn;   // records cut short, passed over
    bool unreadable; // something that is not a record: page treated as full
    // Since start-up.
    uint32_t appends;
    uint32_t page_changes;
    uint32_t failures;
    StoreResult last_result;
    FlashResult last_flash; // what the flash said to the last erase or write
} StoreStatus;

// Finds the page in use and the latest record of each type. Reads only.
void store_init(void);

// The latest record of a type: false if there is none, or it is longer than
// capacity.
bool store_read(uint8_t type, uint8_t *data, uint16_t capacity, uint16_t *length);

// Stores a record, replacing the type's last. May erase a page.
StoreResult store_write(uint8_t type, const uint8_t *data, uint16_t length);

// Whether writing a record of this length would change page: an erase.
bool store_write_erases(uint16_t length);

const StoreStatus *store_status(void);
const char *store_result_name(StoreResult result);
// One line: the page in use, its generation, space, records and counts.
void store_print(void);

#endif // STORE_H
