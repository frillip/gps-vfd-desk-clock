#include "ubx_decode.h"
#include <limits.h>
#include <stddef.h>
#include <string.h>

// ---- Little-endian field readers ----------------------------------------
// AT(wire_type, field) is the byte offset of a field in the wire layout.
#define AT(wire_type, field) offsetof(wire_type, field)

static uint8_t read_u8(const uint8_t *source)
{
    return source[0];
}


static int8_t read_i8(const uint8_t *source)
{
    return source[0] <= INT8_MAX ? (int8_t)source[0] : (int8_t)((int)source[0] - 256);
}


static uint16_t read_u16(const uint8_t *source)
{
    return (uint16_t)((uint16_t)source[0] | ((uint16_t)source[1] << 8));
}


static uint32_t read_u32(const uint8_t *source)
{
    return (uint32_t)source[0] | ((uint32_t)source[1] << 8) | ((uint32_t)source[2] << 16) |
           ((uint32_t)source[3] << 24);
}


// Avoids the implementation-defined unsigned-to-signed conversion.
static int32_t read_i32(const uint8_t *source)
{
    uint32_t value = read_u32(source);
    return value <= INT32_MAX ? (int32_t)value : -1 - (int32_t)(UINT32_MAX - value);
}


static bool length_is(const uint8_t *payload, const void *destination, uint16_t length,
                      uint16_t expected)
{
    return payload != NULL && destination != NULL && length == expected;
}


// ---- Navigation and timing messages -------------------------------------
bool ubx_decode_nav_posllh(const uint8_t *payload, uint16_t length, UbxNavPosllh *destination)
{
    if (!length_is(payload, destination, length, UBX_NAV_POSLLH_LENGTH))
    {
        return false;
    }
    destination->time_of_week_ms = read_u32(payload + AT(UbxNavPosllhWire, time_of_week_ms));
    destination->longitude_e7 = read_i32(payload + AT(UbxNavPosllhWire, longitude_e7));
    destination->latitude_e7 = read_i32(payload + AT(UbxNavPosllhWire, latitude_e7));
    destination->height_ellipsoid_mm =
        read_i32(payload + AT(UbxNavPosllhWire, height_ellipsoid_mm));
    destination->height_msl_mm = read_i32(payload + AT(UbxNavPosllhWire, height_msl_mm));
    destination->horizontal_accuracy_mm =
        read_u32(payload + AT(UbxNavPosllhWire, horizontal_accuracy_mm));
    destination->vertical_accuracy_mm =
        read_u32(payload + AT(UbxNavPosllhWire, vertical_accuracy_mm));
    return true;
}


bool ubx_decode_nav_status(const uint8_t *payload, uint16_t length, UbxNavStatus *destination)
{
    if (!length_is(payload, destination, length, UBX_NAV_STATUS_LENGTH))
    {
        return false;
    }
    destination->time_of_week_ms = read_u32(payload + AT(UbxNavStatusWire, time_of_week_ms));
    destination->fix_type = read_u8(payload + AT(UbxNavStatusWire, fix_type));
    destination->flags = read_u8(payload + AT(UbxNavStatusWire, flags));
    destination->fix_status = read_u8(payload + AT(UbxNavStatusWire, fix_status));
    destination->status_flags = read_u8(payload + AT(UbxNavStatusWire, status_flags));
    destination->time_to_first_fix_ms =
        read_u32(payload + AT(UbxNavStatusWire, time_to_first_fix_ms));
    destination->receiver_uptime_ms = read_u32(payload + AT(UbxNavStatusWire, receiver_uptime_ms));
    return true;
}


bool ubx_decode_nav_timeutc(const uint8_t *payload, uint16_t length, UbxNavTimeutc *destination)
{
    if (!length_is(payload, destination, length, UBX_NAV_TIMEUTC_LENGTH))
    {
        return false;
    }
    destination->time_of_week_ms = read_u32(payload + AT(UbxNavTimeutcWire, time_of_week_ms));
    destination->time_accuracy_ns = read_u32(payload + AT(UbxNavTimeutcWire, time_accuracy_ns));
    destination->fractional_second_ns =
        read_i32(payload + AT(UbxNavTimeutcWire, fractional_second_ns));
    destination->year = read_u16(payload + AT(UbxNavTimeutcWire, year));
    destination->month = read_u8(payload + AT(UbxNavTimeutcWire, month));
    destination->day = read_u8(payload + AT(UbxNavTimeutcWire, day));
    destination->hour = read_u8(payload + AT(UbxNavTimeutcWire, hour));
    destination->minute = read_u8(payload + AT(UbxNavTimeutcWire, minute));
    destination->second = read_u8(payload + AT(UbxNavTimeutcWire, second));
    destination->validity_flags = read_u8(payload + AT(UbxNavTimeutcWire, validity_flags));
    return true;
}


bool ubx_decode_nav_clock(const uint8_t *payload, uint16_t length, UbxNavClock *destination)
{
    if (!length_is(payload, destination, length, UBX_NAV_CLOCK_LENGTH))
    {
        return false;
    }
    destination->time_of_week_ms = read_u32(payload + AT(UbxNavClockWire, time_of_week_ms));
    destination->clock_bias_ns = read_i32(payload + AT(UbxNavClockWire, clock_bias_ns));
    destination->clock_drift_ns_per_s =
        read_i32(payload + AT(UbxNavClockWire, clock_drift_ns_per_s));
    destination->time_accuracy_ns = read_u32(payload + AT(UbxNavClockWire, time_accuracy_ns));
    destination->frequency_accuracy_ps_per_s =
        read_u32(payload + AT(UbxNavClockWire, frequency_accuracy_ps_per_s));
    return true;
}


bool ubx_decode_nav_eoe(const uint8_t *payload, uint16_t length, UbxNavEoe *destination)
{
    if (!length_is(payload, destination, length, UBX_NAV_EOE_LENGTH))
    {
        return false;
    }
    destination->time_of_week_ms = read_u32(payload + AT(UbxNavEoeWire, time_of_week_ms));
    return true;
}


bool ubx_decode_nav_timels(const uint8_t *payload, uint16_t length, UbxNavTimels *destination)
{
    if (!length_is(payload, destination, length, UBX_NAV_TIMELS_LENGTH))
    {
        return false;
    }
    destination->time_of_week_ms = read_u32(payload + AT(UbxNavTimelsWire, time_of_week_ms));
    destination->version = read_u8(payload + AT(UbxNavTimelsWire, version));
    destination->current_source = read_u8(payload + AT(UbxNavTimelsWire, current_source));
    destination->current_leap_seconds =
        read_i8(payload + AT(UbxNavTimelsWire, current_leap_seconds));
    destination->change_source = read_u8(payload + AT(UbxNavTimelsWire, change_source));
    destination->leap_second_change = read_i8(payload + AT(UbxNavTimelsWire, leap_second_change));
    destination->time_to_event_s = read_i32(payload + AT(UbxNavTimelsWire, time_to_event_s));
    destination->event_gps_week = read_u16(payload + AT(UbxNavTimelsWire, event_gps_week));
    destination->event_gps_day = read_u16(payload + AT(UbxNavTimelsWire, event_gps_day));
    destination->validity_flags = read_u8(payload + AT(UbxNavTimelsWire, validity_flags));
    return true;
}


bool ubx_decode_tim_tp(const uint8_t *payload, uint16_t length, UbxTimTp *destination)
{
    if (!length_is(payload, destination, length, UBX_TIM_TP_LENGTH))
    {
        return false;
    }
    destination->time_of_week_ms = read_u32(payload + AT(UbxTimTpWire, time_of_week_ms));
    destination->time_of_week_sub_ms = read_u32(payload + AT(UbxTimTpWire, time_of_week_sub_ms));
    destination->quantisation_error_ps =
        read_i32(payload + AT(UbxTimTpWire, quantisation_error_ps));
    destination->week = read_u16(payload + AT(UbxTimTpWire, week));
    destination->flags = read_u8(payload + AT(UbxTimTpWire, flags));
    destination->reference_info = read_u8(payload + AT(UbxTimTpWire, reference_info));
    return true;
}


bool ubx_decode_tim_tm2(const uint8_t *payload, uint16_t length, UbxTimTm2 *destination)
{
    if (!length_is(payload, destination, length, UBX_TIM_TM2_LENGTH))
    {
        return false;
    }
    destination->channel = read_u8(payload + AT(UbxTimTm2Wire, channel));
    destination->flags = read_u8(payload + AT(UbxTimTm2Wire, flags));
    destination->rising_edge_count = read_u16(payload + AT(UbxTimTm2Wire, rising_edge_count));
    destination->rising_week = read_u16(payload + AT(UbxTimTm2Wire, rising_week));
    destination->falling_week = read_u16(payload + AT(UbxTimTm2Wire, falling_week));
    destination->rising_time_of_week_ms =
        read_u32(payload + AT(UbxTimTm2Wire, rising_time_of_week_ms));
    destination->rising_sub_ms_ns = read_u32(payload + AT(UbxTimTm2Wire, rising_sub_ms_ns));
    destination->falling_time_of_week_ms =
        read_u32(payload + AT(UbxTimTm2Wire, falling_time_of_week_ms));
    destination->falling_sub_ms_ns = read_u32(payload + AT(UbxTimTm2Wire, falling_sub_ms_ns));
    destination->accuracy_ns = read_u32(payload + AT(UbxTimTm2Wire, accuracy_ns));
    return true;
}


bool ubx_decode_ack(const uint8_t *payload, uint16_t length, UbxAck *destination)
{
    if (!length_is(payload, destination, length, UBX_ACK_LENGTH))
    {
        return false;
    }
    destination->class_id = read_u8(payload + AT(UbxAckWire, class_id));
    destination->message_id = read_u8(payload + AT(UbxAckWire, message_id));
    return true;
}


// ---- MON-VER --------------------------------------------------------------

// Copies a fixed-width wire string, stopping at its NUL, and always
// terminates the destination (which holds width + 1 bytes).
static void copy_string(char *destination, const uint8_t *source, uint16_t width)
{
    uint16_t index;

    for (index = 0; index < width && source[index] != 0U; index++)
    {
        destination[index] = (char)source[index];
    }
    destination[index] = '\0';
}


// If extension starts with prefix, copies the remainder of the extension.
static void copy_extension_value(char *destination, const uint8_t *extension, const char *prefix)
{
    uint16_t prefix_length = (uint16_t)strlen(prefix);

    if (memcmp(extension, prefix, prefix_length) == 0)
    {
        copy_string(destination, extension + prefix_length,
                    (uint16_t)(UBX_MON_VER_EXTENSION_LENGTH - prefix_length));
    }
}


bool ubx_decode_mon_ver(const uint8_t *payload, uint16_t length, UbxMonVer *destination)
{
    uint16_t offset;

    if (payload == NULL || destination == NULL || length < UBX_MON_VER_BASE_LENGTH ||
        (length - UBX_MON_VER_BASE_LENGTH) % UBX_MON_VER_EXTENSION_LENGTH != 0U)
    {
        return false;
    }
    memset(destination, 0, sizeof(*destination));
    copy_string(destination->software_version, payload, UBX_MON_VER_SW_VERSION_LENGTH);
    copy_string(destination->hardware_version, payload + UBX_MON_VER_SW_VERSION_LENGTH,
                UBX_MON_VER_HW_VERSION_LENGTH);
    destination->extension_count =
        (uint16_t)((length - UBX_MON_VER_BASE_LENGTH) / UBX_MON_VER_EXTENSION_LENGTH);

    for (offset = UBX_MON_VER_BASE_LENGTH; offset < length;
         offset = (uint16_t)(offset + UBX_MON_VER_EXTENSION_LENGTH))
    {
        const uint8_t *extension = payload + offset;
        copy_extension_value(destination->module_name, extension, "MOD=");
        copy_extension_value(destination->firmware_version, extension, "FWVER=");
        copy_extension_value(destination->protocol_version, extension, "PROTVER=");
    }
    return true;
}


// ---- CFG-VALGET -----------------------------------------------------------
bool ubx_decode_valget_open(const uint8_t *payload, uint16_t length, UbxCfgValueReader *reader)
{
    if (payload == NULL || reader == NULL || length < UBX_CFG_VAL_HEADER_LENGTH)
    {
        return false;
    }
    reader->layer = read_u8(payload + AT(UbxCfgValHeaderWire, layer));
    reader->position = read_u16(payload + AT(UbxCfgValHeaderWire, position));
    reader->cursor = payload + UBX_CFG_VAL_HEADER_LENGTH;
    reader->end = payload + length;
    return true;
}


UbxCfgReadResult ubx_decode_valget_next(UbxCfgValueReader *reader, UbxCfgValue *item)
{
    uint32_t key;
    uint8_t size;
    uint8_t index;

    if (reader == NULL || item == NULL)
    {
        return UBX_CFG_READ_MALFORMED;
    }
    if (reader->cursor == reader->end)
    {
        return UBX_CFG_READ_END;
    }
    if (reader->end - reader->cursor < 4)
    {
        return UBX_CFG_READ_MALFORMED;
    }
    key = read_u32(reader->cursor);
    size = (uint8_t)UBX_CFG_KEY_VALUE_SIZE(key);
    if (size == 0U || reader->end - reader->cursor < 4 + size)
    {
        return UBX_CFG_READ_MALFORMED;
    }
    reader->cursor += 4;

    item->key = key;
    item->size = size;
    item->value = 0U;
    for (index = 0; index < size && index < 4U; index++)
    {
        item->value |= (uint32_t)reader->cursor[index] << (8U * index);
    }
    reader->cursor += size;
    return UBX_CFG_READ_ITEM;
}
