/*
 * Outgoing UBX frames. Pure functions: they build frames in caller-owned
 * memory and never touch the UART, so callers decide when and whether a
 * frame is queued (uart2_tx_write is all-or-nothing).
 */
#ifndef UBX_ENCODE_H
#define UBX_ENCODE_H
#include <stdint.h>
#include <stdbool.h>
#include "ubx_proto.h"

// A poll request is a frame with an empty payload.
#define UBX_POLL_FRAME_LENGTH UBX_FRAME_OVERHEAD

// Builds a complete frame. Returns its length, or 0 if it does not fit.
uint16_t ubx_encode_frame(uint8_t class_id, uint8_t message_id, const uint8_t *payload,
                          uint16_t payload_length, uint8_t *frame, uint16_t frame_capacity);
void ubx_encode_poll(uint8_t class_id, uint8_t message_id, uint8_t frame[UBX_POLL_FRAME_LENGTH]);

/* ---- CFG-VALSET / CFG-VALGET builder ------------------------------------
 *   UbxCfgMessage message;
 *   ubx_encode_valset_begin(&message, UBX_CFG_LAYER_RAM | UBX_CFG_LAYER_BBR);
 *   ubx_encode_valset_add(&message, UBX_CFG_RATE_MEAS, 1000);
 *   if (ubx_encode_cfg_finish(&message)) uart2_tx_write(message.frame, message.frame_length);
 *
 * Any failed Add marks the message as overflowed and Finish then fails, so
 * a partially built configuration can never be sent. */
#define UBX_CFG_MESSAGE_CAPACITY 256U

typedef struct
{
    uint8_t frame[UBX_CFG_MESSAGE_CAPACITY];
    uint16_t frame_length; // valid after a successful ubx_encode_cfg_finish()
    uint16_t payload_length;
    uint8_t message_id; // UBX_ID_CFG_VALSET or UBX_ID_CFG_VALGET
    uint8_t key_count;
    bool overflowed;
} UbxCfgMessage;

// layers: bitmask of UBX_CFG_LAYER_*
void ubx_encode_valset_begin(UbxCfgMessage *message, uint8_t layers);
// value is truncated to the key's size; 8-byte keys are not supported.
bool ubx_encode_valset_add(UbxCfgMessage *message, uint32_t key, uint32_t value);

// layer_number: one of UBX_CFG_GET_LAYER_*
void ubx_encode_valget_begin(UbxCfgMessage *message, uint8_t layer_number);
bool ubx_encode_valget_add(UbxCfgMessage *message, uint32_t key);

// Writes header and checksum. Returns false if the message overflowed or
// holds no keys.
bool ubx_encode_cfg_finish(UbxCfgMessage *message);

#endif
