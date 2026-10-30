#include "ubx_parser.h"
#include "../drivers/uart2.h"
#include "../platform/time_ms.h"
#include <stddef.h>
#include <string.h>

// ---- Expected messages --------------------------------------------------
// Every message the clock enables or polls for. A fixed-length message has
// length_step 0; a variable-length one is length_base + n * length_step.
typedef struct
{
    uint8_t class_id;
    uint8_t message_id;
    uint16_t length_base;
    uint16_t length_step;
} UbxMessageRule;

static const UbxMessageRule expected_messages[] = {
    {UBX_CLASS_NAV, UBX_ID_NAV_POSLLH, UBX_NAV_POSLLH_LENGTH, 0U},
    {UBX_CLASS_NAV, UBX_ID_NAV_STATUS, UBX_NAV_STATUS_LENGTH, 0U},
    {UBX_CLASS_NAV, UBX_ID_NAV_TIMEUTC, UBX_NAV_TIMEUTC_LENGTH, 0U},
    {UBX_CLASS_NAV, UBX_ID_NAV_CLOCK, UBX_NAV_CLOCK_LENGTH, 0U},
    {UBX_CLASS_NAV, UBX_ID_NAV_TIMELS, UBX_NAV_TIMELS_LENGTH, 0U},
    {UBX_CLASS_NAV, UBX_ID_NAV_EOE, UBX_NAV_EOE_LENGTH, 0U},
    {UBX_CLASS_TIM, UBX_ID_TIM_TP, UBX_TIM_TP_LENGTH, 0U},
    {UBX_CLASS_TIM, UBX_ID_TIM_TM2, UBX_TIM_TM2_LENGTH, 0U},
    {UBX_CLASS_ACK, UBX_ID_ACK_ACK, UBX_ACK_LENGTH, 0U},
    {UBX_CLASS_ACK, UBX_ID_ACK_NAK, UBX_ACK_LENGTH, 0U},
    {UBX_CLASS_MON, UBX_ID_MON_VER, UBX_MON_VER_BASE_LENGTH, UBX_MON_VER_EXTENSION_LENGTH},
    // Key/value pairs of 5, 6, 8 or 12 bytes follow the header: any length.
    {UBX_CLASS_CFG, UBX_ID_CFG_VALGET, UBX_CFG_VAL_HEADER_LENGTH, 1U},
};

#define EXPECTED_MESSAGE_COUNT (sizeof(expected_messages) / sizeof(expected_messages[0]))

static const UbxMessageRule *find_rule(uint8_t class_id, uint8_t message_id)
{
    uint16_t index;

    for (index = 0; index < EXPECTED_MESSAGE_COUNT; index++)
    {
        if (expected_messages[index].class_id == class_id &&
            expected_messages[index].message_id == message_id)
        {
            return &expected_messages[index];
        }
    }
    return NULL;
}


static bool length_matches(const UbxMessageRule *rule, uint16_t length)
{
    if (rule->length_step == 0U)
    {
        return length == rule->length_base;
    }
    return length >= rule->length_base && (length - rule->length_base) % rule->length_step == 0U;
}


// A header can start a frame if the frame fits the buffer and, for an
// expected message, has a length that message can have.
static bool header_is_plausible(const UbxMessageRule *rule, uint16_t length)
{
    return length <= UBX_PARSER_MAX_PAYLOAD && (rule == NULL || length_matches(rule, length));
}


// ---- Buffer management --------------------------------------------------
static uint16_t buffered_bytes(const UbxParser *parser)
{
    return (uint16_t)(parser->end - parser->start);
}


/* Consuming only moves the start index; bytes are moved back to the front
 * lazily, when an append reaches the end of the buffer. Each byte is moved
 * at most once per buffer length appended, so resynchronising after a
 * corrupt frame costs O(n) copying rather than O(n^2). */
static void consume(UbxParser *parser, uint16_t count)
{
    parser->start = (uint16_t)(parser->start + count);
    if (parser->start >= parser->end)
    {
        parser->start = 0;
        parser->end = 0;
    }
}


static void append(UbxParser *parser, uint8_t byte)
{
    if (parser->end == UBX_PARSER_BUFFER_BYTES)
    {
        uint16_t count = buffered_bytes(parser);
        memmove(parser->buffer, parser->buffer + parser->start, count);
        parser->start = 0;
        parser->end = count;
    }
    parser->buffer[parser->end++] = byte;
}


// ---- Statistics ---------------------------------------------------------
static void count_false_sync(UbxParser *parser, const uint8_t *frame, uint16_t length)
{
    parser->stats.false_syncs++;
    parser->stats.last_false_sync_class = frame[UBX_HEADER_OFFSET_CLASS];
    parser->stats.last_false_sync_id = frame[UBX_HEADER_OFFSET_ID];
    parser->stats.last_false_sync_length = length;
}


static void count_unexpected(UbxParser *parser, uint8_t class_id, uint8_t message_id,
                             uint16_t length)
{
    UbxUnexpectedMessage *entry;
    uint16_t index;

    parser->stats.unexpected_frames++;
    for (index = 0; index < UBX_PARSER_UNEXPECTED_KINDS; index++)
    {
        entry = &parser->stats.unexpected[index];
        if (entry->count == 0U || (entry->class_id == class_id && entry->message_id == message_id))
        {
            entry->class_id = class_id;
            entry->message_id = message_id;
            entry->length = length;
            entry->count++;
            return;
        }
    }
    parser->stats.unexpected_other++;
}


// ---- Frame processing ---------------------------------------------------
void ubx_checksum(const uint8_t *source, uint16_t length, uint8_t *ck_a, uint8_t *ck_b)
{
    uint8_t a = 0U;
    uint8_t b = 0U;
    uint16_t index;

    for (index = 0; index < length; index++)
    {
        a = (uint8_t)(a + source[index]);
        b = (uint8_t)(b + a);
    }
    *ck_a = a;
    *ck_b = b;
}


static uint16_t decode_length(const uint8_t *frame)
{
    return (uint16_t)(frame[UBX_HEADER_OFFSET_LENGTH] |
                      ((uint16_t)frame[UBX_HEADER_OFFSET_LENGTH + 1U] << 8));
}


/* Processes as many complete frames as are buffered. On any rejection one
 * byte is dropped and the retained suffix is rescanned, so a frame that
 * starts inside a corrupt candidate is still found. Sync pairs inside a valid
 * frame's payload are never examined because valid frames are consumed whole.
 * flush: the stream has gone quiet, so an incomplete candidate will never
 * complete; reject it instead of waiting for it. */
static void process_frames(UbxParser *parser, uint32_t now_ms, bool flush)
{
    while (buffered_bytes(parser) > 0U)
    {
        const uint8_t *frame = parser->buffer + parser->start;
        uint16_t available = buffered_bytes(parser);
        const UbxMessageRule *rule;
        uint8_t class_id;
        uint8_t message_id;
        uint16_t length;
        uint16_t frame_length;
        uint8_t ck_a;
        uint8_t ck_b;

        if (frame[0] != UBX_SYNC_CHAR_1)
        {
            consume(parser, 1U);
            continue;
        }
        if (available < 2U)
        {
            if (flush)
            {
                consume(parser, 1U);
                continue;
            }
            return;
        }
        if (frame[1] != UBX_SYNC_CHAR_2)
        {
            consume(parser, 1U);
            continue;
        }
        if (available < UBX_HEADER_LENGTH)
        {
            if (flush)
            {
                consume(parser, 1U);
                continue;
            }
            return;
        }

        class_id = frame[UBX_HEADER_OFFSET_CLASS];
        message_id = frame[UBX_HEADER_OFFSET_ID];
        length = decode_length(frame);
        rule = find_rule(class_id, message_id);
        if (!header_is_plausible(rule, length))
        {
            count_false_sync(parser, frame, length);
            consume(parser, 1U);
            continue;
        }

        frame_length = (uint16_t)(length + UBX_FRAME_OVERHEAD);
        if (available < frame_length)
        {
            if (flush)
            {
                consume(parser, 1U); // truncated: counted as the timeout
                continue;
            }
            return;
        }
        ubx_checksum(frame + UBX_HEADER_OFFSET_CLASS,
                     (uint16_t)(length + UBX_HEADER_LENGTH - UBX_HEADER_OFFSET_CLASS), &ck_a,
                     &ck_b);
        if (ck_a != frame[frame_length - 2U] || ck_b != frame[frame_length - 1U])
        {
            // A real frame of a message we expect, corrupted; or noise.
            if (rule != NULL)
            {
                parser->stats.checksum_errors++;
            }
            else
            {
                count_false_sync(parser, frame, length);
            }
            consume(parser, 1U);
            continue;
        }
        if (rule == NULL)
        {
            count_unexpected(parser, class_id, message_id, length);
            consume(parser, frame_length);
            continue;
        }

        parser->stats.valid_frames++;
        if (parser->handler != NULL)
        {
            parser->handler(class_id, message_id, frame + UBX_HEADER_LENGTH, length, now_ms,
                            parser->handler_context);
        }
        consume(parser, frame_length);
    }
}


// ---- Public interface ---------------------------------------------------
void ubx_parser_init(UbxParser *parser, UbxMessageHandler handler, void *handler_context)
{
    if (parser == NULL)
    {
        return;
    }
    memset(parser, 0, sizeof(*parser));
    parser->handler = handler;
    parser->handler_context = handler_context;
}


void ubx_parser_reset(UbxParser *parser)
{
    if (parser == NULL)
    {
        return;
    }
    parser->start = 0;
    parser->end = 0;
}


void ubx_parser_discontinuity(UbxParser *parser)
{
    if (parser == NULL)
    {
        return;
    }
    parser->stats.discontinuities++;
    ubx_parser_reset(parser);
}


void ubx_parser_feed(UbxParser *parser, uint8_t byte, uint32_t now_ms)
{
    if (parser == NULL)
    {
        return;
    }
    parser->stats.bytes_received++;
    parser->last_byte_ms = now_ms;
    // Never full here: a held candidate frame is at most the buffer size and
    // is consumed (or rejected) as soon as it is complete.
    append(parser, byte);
    process_frames(parser, now_ms, false);
}


void ubx_parser_check_timeout(UbxParser *parser, uint32_t now_ms)
{
    if (parser == NULL)
    {
        return;
    }
    if (buffered_bytes(parser) > 0U &&
        time_ms_has_elapsed(now_ms, parser->last_byte_ms, UBX_PARSER_TIMEOUT_MS))
    {
        parser->stats.timeouts++;
        process_frames(parser, now_ms, true);
    }
}


uint16_t ubx_parser_service(UbxParser *parser, uint32_t now_ms, uint16_t byte_budget)
{
    uint16_t bytes_consumed = 0;
    uint8_t byte;

    if (parser == NULL)
    {
        return 0;
    }
    while (byte_budget == 0U || bytes_consumed < byte_budget)
    {
        // Errors are checked before every byte: a byte popped after an error
        // may belong to a frame that has lost bytes. Recovery empties the
        // ring, so the loop ends naturally afterwards.
        if (uart2_rx_error_pending() && uart2_rx_recover() != 0U)
        {
            ubx_parser_discontinuity(parser);
        }
        if (!uart2_rx_pop(&byte))
        {
            break;
        }
        ubx_parser_feed(parser, byte, now_ms);
        bytes_consumed++;
    }
    if (uart2_rx_available() == 0U)
    {
        ubx_parser_check_timeout(parser, now_ms);
    }
    return bytes_consumed;
}
