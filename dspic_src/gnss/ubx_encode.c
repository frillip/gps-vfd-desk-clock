#include "ubx_encode.h"
#include "ubx_parser.h" // ubx_checksum
#include <stddef.h>
#include <string.h>

static void write_header(uint8_t *frame, uint8_t class_id, uint8_t message_id,
                         uint16_t payload_length)
{
    frame[0] = UBX_SYNC_CHAR_1;
    frame[1] = UBX_SYNC_CHAR_2;
    frame[UBX_HEADER_OFFSET_CLASS] = class_id;
    frame[UBX_HEADER_OFFSET_ID] = message_id;
    frame[UBX_HEADER_OFFSET_LENGTH] = (uint8_t)(payload_length & 0xFFU);
    frame[UBX_HEADER_OFFSET_LENGTH + 1U] = (uint8_t)(payload_length >> 8);
}


// Header must already be in place; payload follows it.
static void write_checksum(uint8_t *frame, uint16_t payload_length)
{
    uint16_t covered = (uint16_t)(payload_length + UBX_HEADER_LENGTH - UBX_HEADER_OFFSET_CLASS);
    uint8_t *checksum = frame + UBX_HEADER_LENGTH + payload_length;
    ubx_checksum(frame + UBX_HEADER_OFFSET_CLASS, covered, &checksum[0], &checksum[1]);
}


uint16_t ubx_encode_frame(uint8_t class_id, uint8_t message_id, const uint8_t *payload,
                          uint16_t payload_length, uint8_t *frame, uint16_t frame_capacity)
{
    uint32_t frame_length = (uint32_t)payload_length + UBX_FRAME_OVERHEAD;

    if (frame == NULL || frame_length > frame_capacity || (payload == NULL && payload_length != 0U))
    {
        return 0;
    }
    write_header(frame, class_id, message_id, payload_length);
    if (payload_length != 0U)
    {
        memcpy(frame + UBX_HEADER_LENGTH, payload, payload_length);
    }
    write_checksum(frame, payload_length);
    return (uint16_t)frame_length;
}


void ubx_encode_poll(uint8_t class_id, uint8_t message_id, uint8_t frame[UBX_POLL_FRAME_LENGTH])
{
    (void)ubx_encode_frame(class_id, message_id, NULL, 0U, frame, UBX_POLL_FRAME_LENGTH);
}


// ---- CFG builder ----------------------------------------------------------
static void cfg_begin(UbxCfgMessage *message, uint8_t message_id, uint8_t layer)
{
    uint8_t *header = message->frame + UBX_HEADER_LENGTH;

    memset(message, 0, sizeof(*message));
    message->message_id = message_id;
    header[offsetof(UbxCfgValHeaderWire, version)] = 0U; // request version
    header[offsetof(UbxCfgValHeaderWire, layer)] = layer;
    // position (VALGET) / reserved (VALSET) bytes stay zero
    message->payload_length = UBX_CFG_VAL_HEADER_LENGTH;
}


// Appends size bytes of value, little-endian.
static bool cfg_append(UbxCfgMessage *message, uint32_t value, uint8_t size)
{
    uint8_t *cursor = message->frame + UBX_HEADER_LENGTH + message->payload_length;
    uint8_t index;

    if (message->overflowed ||
        (uint32_t)message->payload_length + size + UBX_FRAME_OVERHEAD > UBX_CFG_MESSAGE_CAPACITY)
    {
        message->overflowed = true;
        return false;
    }
    for (index = 0; index < size; index++)
    {
        cursor[index] = (uint8_t)(value >> (8U * index));
    }
    message->payload_length = (uint16_t)(message->payload_length + size);
    return true;
}


static bool cfg_append_key(UbxCfgMessage *message, uint32_t key)
{
    if (message->key_count >= UBX_CFG_VAL_MAX_KEYS)
    {
        message->overflowed = true;
        return false;
    }
    if (!cfg_append(message, key, 4U))
    {
        return false;
    }
    message->key_count++;
    return true;
}


void ubx_encode_valset_begin(UbxCfgMessage *message, uint8_t layers)
{
    if (message != NULL)
    {
        cfg_begin(message, UBX_ID_CFG_VALSET, layers);
    }
}


bool ubx_encode_valset_add(UbxCfgMessage *message, uint32_t key, uint32_t value)
{
    uint8_t size = (uint8_t)UBX_CFG_KEY_VALUE_SIZE(key);

    if (message == NULL || message->message_id != UBX_ID_CFG_VALSET)
    {
        return false;
    }
    if (size == 0U || size > 4U)
    {
        message->overflowed = true; // poison: unsupported key size
        return false;
    }
    // Check space for key and value together so a pair is never split.
    if ((uint32_t)message->payload_length + 4U + size + UBX_FRAME_OVERHEAD >
        UBX_CFG_MESSAGE_CAPACITY)
    {
        message->overflowed = true;
        return false;
    }
    return cfg_append_key(message, key) && cfg_append(message, value, size);
}


void ubx_encode_valget_begin(UbxCfgMessage *message, uint8_t layer_number)
{
    if (message != NULL)
    {
        cfg_begin(message, UBX_ID_CFG_VALGET, layer_number);
    }
}


bool ubx_encode_valget_add(UbxCfgMessage *message, uint32_t key)
{
    if (message == NULL || message->message_id != UBX_ID_CFG_VALGET)
    {
        return false;
    }
    return cfg_append_key(message, key);
}


bool ubx_encode_cfg_finish(UbxCfgMessage *message)
{
    if (message == NULL || message->overflowed || message->key_count == 0U)
    {
        return false;
    }
    write_header(message->frame, UBX_CLASS_CFG, message->message_id, message->payload_length);
    write_checksum(message->frame, message->payload_length);
    message->frame_length = (uint16_t)(message->payload_length + UBX_FRAME_OVERHEAD);
    return true;
}
