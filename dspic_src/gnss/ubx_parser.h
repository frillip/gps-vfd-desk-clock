/*
 * UBX frame assembly: turns a byte stream into checksum-verified frames.
 *
 * Every header that could start a frame of a size the buffer holds is
 * buffered and checksum-verified. Only the messages the clock expects, at
 * the lengths they can have, are dispatched; a valid frame of any other
 * message is counted by class and id, so a message the receiver sends but
 * the clock does not use shows up by name instead of as noise.
 *
 *   Expected message, its length, checksum good -> dispatched
 *   Expected message, its length, checksum bad  -> checksum error, drop one byte
 *   Other message, checksum good                -> counted as unexpected, skipped
 *   Other message, checksum bad                 -> false sync, drop one byte
 *   Expected message at a wrong length, or any
 *   length beyond UBX_PARSER_MAX_PAYLOAD        -> false sync, drop one byte
 *
 * A rejected candidate costs one byte and the bytes behind it are rescanned,
 * so a stray B5 62 in noise can delay the frames behind it (by at most one
 * buffer of bytes, 45 ms at 115200) but never lose them. If the stream goes
 * quiet behind such a candidate, the timeout rescans what is buffered rather
 * than dropping it.
 *
 * Payload decoding lives in ubx_decode.h; outgoing frames in ubx_encode.h.
 */
#ifndef UBX_PARSER_H
#define UBX_PARSER_H
#include <stdint.h>
#include <stdbool.h>
#include "ubx_proto.h"

// Largest payload framed. Sized for MON-VER (40 + 30 per extension: 220
// bytes on a NEO-M9N) and the provisioning CFG-VALGET response (about 150
// bytes). Anything declaring more is treated as a false sync.
#define UBX_PARSER_MAX_PAYLOAD  512U
#define UBX_PARSER_BUFFER_BYTES (UBX_PARSER_MAX_PAYLOAD + UBX_FRAME_OVERHEAD)
// A partial frame with no new bytes for this long is abandoned.
#define UBX_PARSER_TIMEOUT_MS 50U

// Kinds of unexpected message counted individually.
#define UBX_PARSER_UNEXPECTED_KINDS 4U

typedef struct
{
    uint8_t class_id;
    uint8_t message_id;
    uint16_t length; // of the latest one
    uint32_t count;  // 0: entry unused
} UbxUnexpectedMessage;

typedef struct
{
    uint32_t bytes_received;
    uint32_t valid_frames; // expected messages, checksum correct: dispatched
    uint32_t checksum_errors;
    uint32_t false_syncs; // header that did not start a valid frame: noise
    uint8_t last_false_sync_class;
    uint8_t last_false_sync_id;
    uint16_t last_false_sync_length;
    uint32_t timeouts;
    uint32_t discontinuities; // UART errors that invalidated the stream

    // Valid frames of messages the clock does not use, by class and id. Each
    // is a message enabled on the receiver for nothing: disable it there, or
    // add it to the expected list in ubx_parser.c.
    uint32_t unexpected_frames;
    UbxUnexpectedMessage unexpected[UBX_PARSER_UNEXPECTED_KINDS];
    uint32_t unexpected_other; // of kinds beyond the table
} UbxParserStats;

// Called synchronously from ubx_parser_service() in the foreground, after the
// checksum and length checks. The payload pointer is only valid during the
// call: decode or copy before returning. Do not re-enter the parser.
typedef void (*UbxMessageHandler)(uint8_t class_id, uint8_t message_id, const uint8_t *payload,
                                  uint16_t length, uint32_t observed_ms, void *context);

typedef struct
{
    // Buffered, unconsumed bytes are buffer[start..end).
    uint8_t buffer[UBX_PARSER_BUFFER_BYTES];
    uint16_t start;
    uint16_t end;
    uint32_t last_byte_ms;
    UbxMessageHandler handler;
    void *handler_context;
    UbxParserStats stats;
} UbxParser;

void ubx_parser_init(UbxParser *parser, UbxMessageHandler handler, void *handler_context);
// Drops any partial frame; keeps the handler and statistics.
void ubx_parser_reset(UbxParser *parser);
// As reset, and counts a discontinuity (bytes were lost upstream).
void ubx_parser_discontinuity(UbxParser *parser);
void ubx_parser_feed(UbxParser *parser, uint8_t byte, uint32_t now_ms);
// A partial frame with no new bytes for UBX_PARSER_TIMEOUT_MS is rejected,
// and any complete frames buffered behind it are processed. now_ms is a
// wrapping millisecond tick, read atomically.
void ubx_parser_check_timeout(UbxParser *parser, uint32_t now_ms);

// Drains UART2 into the parser. Call from the foreground every 10 ms.
// byte_budget = 0 drains until empty; a finite budget leaves the remainder
// queued and defers the idle timeout until the ring is empty.
uint16_t ubx_parser_service(UbxParser *parser, uint32_t now_ms, uint16_t byte_budget);

// Checksum over class, id, length and payload; also used by ubx_encode.
void ubx_checksum(const uint8_t *source, uint16_t length, uint8_t *ck_a, uint8_t *ck_b);

#endif
