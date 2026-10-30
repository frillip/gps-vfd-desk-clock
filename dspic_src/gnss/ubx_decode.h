/*
 * UBX payload decoders. Each decoder checks the payload length, reads every
 * field explicitly as little-endian from the offsets defined by the wire
 * layouts in ubx_proto.h, and fills a naturally aligned host struct.
 *
 * Host structs are deliberately separate from the packed wire structs:
 * packed members force XC16 into byte-wise access everywhere they are used.
 */
#ifndef UBX_DECODE_H
#define UBX_DECODE_H
#include <stdint.h>
#include <stdbool.h>
#include "ubx_proto.h"

typedef struct
{
    uint32_t time_of_week_ms;
    int32_t longitude_e7;
    int32_t latitude_e7;
    int32_t height_ellipsoid_mm;
    int32_t height_msl_mm;
    uint32_t horizontal_accuracy_mm;
    uint32_t vertical_accuracy_mm;
} UbxNavPosllh;

typedef struct
{
    uint32_t time_of_week_ms;
    uint32_t time_to_first_fix_ms;
    uint32_t receiver_uptime_ms;
    uint8_t fix_type;     // UBX_FIX_*
    uint8_t flags;        // UBX_STATUS_*
    uint8_t fix_status;   // UBX_FIXSTAT_*
    uint8_t status_flags; // UBX_STATUS_PSM/SPOOF/CARR_SOLN fields
} UbxNavStatus;

typedef struct
{
    uint32_t time_of_week_ms;
    uint32_t time_accuracy_ns;
    int32_t fractional_second_ns; // signed; time = calendar + this
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;         // 60 during a leap second
    uint8_t validity_flags; // UBX_TIMEUTC_*
} UbxNavTimeutc;

typedef struct
{
    uint32_t time_of_week_ms;
    int32_t clock_bias_ns;
    int32_t clock_drift_ns_per_s;
    uint32_t time_accuracy_ns;
    uint32_t frequency_accuracy_ps_per_s;
} UbxNavClock;

typedef struct
{
    uint32_t time_of_week_ms;
} UbxNavEoe;

typedef struct
{
    uint32_t time_of_week_ms;
    int32_t time_to_event_s;
    uint16_t event_gps_week;
    uint16_t event_gps_day;
    uint8_t version;
    uint8_t current_source;
    int8_t current_leap_seconds; // GPS - UTC, s
    uint8_t change_source;
    int8_t leap_second_change; // -1, 0, +1
    uint8_t validity_flags;    // UBX_TIMELS_*
} UbxNavTimels;

typedef struct
{
    uint32_t time_of_week_ms;     // of the pulse described
    uint32_t time_of_week_sub_ms; // 2^-32 ms
    int32_t quantisation_error_ps;
    uint16_t week;
    uint8_t flags;          // UBX_TP_FLAGS_*
    uint8_t reference_info; // UBX_TP_REFINFO_*
} UbxTimTp;

typedef struct
{
    uint32_t rising_time_of_week_ms;
    uint32_t rising_sub_ms_ns;
    uint32_t falling_time_of_week_ms;
    uint32_t falling_sub_ms_ns;
    uint32_t accuracy_ns;
    uint16_t rising_edge_count;
    uint16_t rising_week;
    uint16_t falling_week;
    uint8_t channel;
    uint8_t flags; // UBX_TM2_*
} UbxTimTm2;

typedef struct
{
    uint8_t class_id;   // class of the message being acknowledged
    uint8_t message_id; // id of the message being acknowledged
} UbxAck;

// MON-VER strings, always NUL-terminated. Selected extensions are copied out;
// raw extensions are only available during the message callback.
typedef struct
{
    char software_version[UBX_MON_VER_SW_VERSION_LENGTH + 1U];
    char hardware_version[UBX_MON_VER_HW_VERSION_LENGTH + 1U];
    char module_name[UBX_MON_VER_EXTENSION_LENGTH + 1U];      // "MOD="
    char firmware_version[UBX_MON_VER_EXTENSION_LENGTH + 1U]; // "FWVER="
    char protocol_version[UBX_MON_VER_EXTENSION_LENGTH + 1U]; // "PROTVER="
    uint16_t extension_count;
} UbxMonVer;

bool ubx_decode_nav_posllh(const uint8_t *payload, uint16_t length, UbxNavPosllh *destination);
bool ubx_decode_nav_status(const uint8_t *payload, uint16_t length, UbxNavStatus *destination);
bool ubx_decode_nav_timeutc(const uint8_t *payload, uint16_t length, UbxNavTimeutc *destination);
bool ubx_decode_nav_clock(const uint8_t *payload, uint16_t length, UbxNavClock *destination);
bool ubx_decode_nav_timels(const uint8_t *payload, uint16_t length, UbxNavTimels *destination);
bool ubx_decode_nav_eoe(const uint8_t *payload, uint16_t length, UbxNavEoe *destination);
bool ubx_decode_tim_tp(const uint8_t *payload, uint16_t length, UbxTimTp *destination);
bool ubx_decode_tim_tm2(const uint8_t *payload, uint16_t length, UbxTimTm2 *destination);
bool ubx_decode_ack(const uint8_t *payload, uint16_t length, UbxAck *destination);
bool ubx_decode_mon_ver(const uint8_t *payload, uint16_t length, UbxMonVer *destination);

/* ---- CFG-VALGET response reader ----------------------------------------
 * Usage:
 *   UbxCfgValueReader reader;
 *   UbxCfgValue item;
 *   if (ubx_decode_valget_open(payload, length, &reader))
 *       while (ubx_decode_valget_next(&reader, &item) == UBX_CFG_READ_ITEM) ...
 */
typedef struct
{
    const uint8_t *cursor;
    const uint8_t *end;
    uint8_t layer;     // layer number the values were read from
    uint16_t position; // index of the first key in this response
} UbxCfgValueReader;

typedef struct
{
    uint32_t key;
    uint32_t value; // zero-extended; for 8-byte values, the low 32 bits
    uint8_t size;   // value size in bytes: 1, 2, 4 or 8
} UbxCfgValue;

typedef enum
{
    UBX_CFG_READ_ITEM,
    UBX_CFG_READ_END,
    UBX_CFG_READ_MALFORMED // unknown size code or truncated value
} UbxCfgReadResult;

bool ubx_decode_valget_open(const uint8_t *payload, uint16_t length, UbxCfgValueReader *reader);
UbxCfgReadResult ubx_decode_valget_next(UbxCfgValueReader *reader, UbxCfgValue *item);

#endif
