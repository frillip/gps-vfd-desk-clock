/*
 * UBX protocol definitions for the GNSS UART. Wire format only: no
 * application policy (freshness, validity rules) belongs in this file.
 *
 * Frame: B5 62 CLASS ID LENGTH_L LENGTH_H PAYLOAD... CK_A CK_B
 * All multibyte fields are little-endian; signed fields are two's complement.
 *
 * The packed *_WIRE structs document the byte layout and are size-checked
 * below. They are never overlaid on receive buffers: decoders read each
 * field explicitly at offsetof(<struct>, <field>), so the layout here is the
 * single source of truth for both documentation and decoding.
 *
 * Checksum: CK_A = CK_B = 0; for each byte from CLASS to the final payload
 * byte, CK_A += byte; CK_B += CK_A (both modulo 256). The sync bytes and the
 * checksum itself are excluded. This is not the SML CRC.
 *
 * Field availability depends on receiver firmware/protocol version.
 * Unsupported bits are reserved and must be ignored.
 *
 * References: u-blox 8/M8 Receiver Description UBX-13003221;
 * u-blox M9 SPG 4.04 Interface Description UBX-21022436;
 * u-blox F9 TIM 2.24 Interface Description UBXDOC-963802114-13046.
 * TIM-TP: u-blox M9 SPG 4.04 Interface Description, section UBX-TIM-TP.
 * CFG key IDs below were cross-checked against a CFG-VALGET dump from a
 * NEO-M9N running SPG 4.04 (etc/gnss_config.txt).
 */
#ifndef UBX_PROTO_H
#define UBX_PROTO_H
#include <stdint.h>

// ---- Framing -----------------------------------------------------------
#define UBX_SYNC_CHAR_1          0xB5U
#define UBX_SYNC_CHAR_2          0x62U
#define UBX_HEADER_LENGTH        6U // sync x2, class, id, length x2
#define UBX_CHECKSUM_LENGTH      2U
#define UBX_FRAME_OVERHEAD       (UBX_HEADER_LENGTH + UBX_CHECKSUM_LENGTH)
#define UBX_HEADER_OFFSET_CLASS  2U
#define UBX_HEADER_OFFSET_ID     3U
#define UBX_HEADER_OFFSET_LENGTH 4U

// ---- Classes and message IDs ------------------------------------------
#define UBX_CLASS_NAV 0x01U
#define UBX_CLASS_ACK 0x05U
#define UBX_CLASS_CFG 0x06U
#define UBX_CLASS_MON 0x0AU
#define UBX_CLASS_TIM 0x0DU

#define UBX_ID_NAV_POSLLH  0x02U
#define UBX_ID_NAV_STATUS  0x03U
#define UBX_ID_NAV_TIMEUTC 0x21U
#define UBX_ID_NAV_CLOCK   0x22U
#define UBX_ID_NAV_TIMELS  0x26U
#define UBX_ID_NAV_EOE     0x61U
#define UBX_ID_ACK_NAK     0x00U
#define UBX_ID_ACK_ACK     0x01U
#define UBX_ID_CFG_VALSET  0x8AU
#define UBX_ID_CFG_VALGET  0x8BU
#define UBX_ID_MON_VER     0x04U
#define UBX_ID_TIM_TP      0x01U
#define UBX_ID_TIM_TM2     0x03U

#define UBX_WEEK_MS 604800000UL // time of week wraps here

// ---- Fixed payload lengths --------------------------------------------
#define UBX_NAV_POSLLH_LENGTH  28U
#define UBX_NAV_STATUS_LENGTH  16U
#define UBX_NAV_TIMEUTC_LENGTH 20U
#define UBX_NAV_CLOCK_LENGTH   20U
#define UBX_NAV_TIMELS_LENGTH  24U
#define UBX_NAV_EOE_LENGTH     4U
#define UBX_TIM_TP_LENGTH      16U
#define UBX_TIM_TM2_LENGTH     28U
#define UBX_ACK_LENGTH         2U
// Variable-length payloads.
#define UBX_MON_VER_BASE_LENGTH       40U // swVersion[30] + hwVersion[10]
#define UBX_MON_VER_EXTENSION_LENGTH  30U // each extension string
#define UBX_MON_VER_SW_VERSION_LENGTH 30U
#define UBX_MON_VER_HW_VERSION_LENGTH 10U
#define UBX_CFG_VAL_HEADER_LENGTH     4U  // version, layer(s), 2 bytes
#define UBX_CFG_VAL_MAX_KEYS          64U // per VALSET/VALGET message

// ---- NAV-STATUS bits ---------------------------------------------------
#define UBX_STATUS_FIX_OK              0x01U
#define UBX_STATUS_DIFF_SOLN           0x02U
#define UBX_STATUS_WEEK_SET            0x04U
#define UBX_STATUS_TOW_SET             0x08U
#define UBX_FIXSTAT_DIFF_CORR          0x01U
#define UBX_FIXSTAT_CARR_SOLN_VALID    0x02U
#define UBX_FIXSTAT_MAP_MATCHING_MASK  0xC0U
#define UBX_FIXSTAT_MAP_MATCHING_SHIFT 6U
#define UBX_STATUS_PSM_MASK            0x03U
#define UBX_STATUS_PSM_SHIFT           0U
#define UBX_STATUS_SPOOF_MASK          0x18U
#define UBX_STATUS_SPOOF_SHIFT         3U
#define UBX_STATUS_CARR_SOLN_MASK      0xC0U
#define UBX_STATUS_CARR_SOLN_SHIFT     6U

#define UBX_FIX_NONE      0U
#define UBX_FIX_DR_ONLY   1U
#define UBX_FIX_2D        2U
#define UBX_FIX_3D        3U
#define UBX_FIX_GNSS_DR   4U
#define UBX_FIX_TIME_ONLY 5U

// ---- NAV-TIMEUTC bits --------------------------------------------------
#define UBX_TIMEUTC_VALID_TOW      0x01U
#define UBX_TIMEUTC_VALID_WEEK     0x02U
#define UBX_TIMEUTC_VALID_UTC      0x04U
#define UBX_TIMEUTC_AUTHENTICATED  0x08U // reserved on older firmware
#define UBX_TIMEUTC_STANDARD_MASK  0xF0U
#define UBX_TIMEUTC_STANDARD_SHIFT 4U

#define UBX_UTC_UNAVAILABLE 0U
#define UBX_UTC_CRL         1U
#define UBX_UTC_NIST        2U
#define UBX_UTC_USNO        3U
#define UBX_UTC_BIPM        4U
#define UBX_UTC_EU          5U
#define UBX_UTC_SU          6U
#define UBX_UTC_NTSC        7U
#define UBX_UTC_NPLI        8U
#define UBX_UTC_UNKNOWN     15U

// ---- NAV-TIMELS bits ---------------------------------------------------
#define UBX_TIMELS_VALID_CURRENT_LS    0x01U
#define UBX_TIMELS_VALID_TIME_TO_EVENT 0x02U

// ---- TIM-TP bits -------------------------------------------------------
#define UBX_TP_FLAGS_TIMEBASE_UTC         0x01U // 0: tow/week in GNSS time, 1: UTC
#define UBX_TP_FLAGS_UTC_AVAILABLE        0x02U
#define UBX_TP_FLAGS_RAIM_MASK            0x0CU // 0 n/a, 1 inactive, 2 active
#define UBX_TP_FLAGS_RAIM_SHIFT           2U
#define UBX_TP_FLAGS_QERR_INVALID         0x10U // protocol 32+; reserved (0) before
#define UBX_TP_REFINFO_GNSS_MASK          0x0FU // time reference GNSS when GNSS-based
#define UBX_TP_REFINFO_UTC_STANDARD_MASK  0xF0U
#define UBX_TP_REFINFO_UTC_STANDARD_SHIFT 4U

// ---- TIM-TM2 bits ------------------------------------------------------
#define UBX_TM2_MODE_RUNNING     0x01U
#define UBX_TM2_STOPPED          0x02U
#define UBX_TM2_NEW_FALLING_EDGE 0x04U
#define UBX_TM2_TIMEBASE_MASK    0x18U
#define UBX_TM2_TIMEBASE_SHIFT   3U
#define UBX_TM2_UTC_AVAILABLE    0x20U
#define UBX_TM2_TIME_VALID       0x40U
#define UBX_TM2_NEW_RISING_EDGE  0x80U

#define UBX_TIMEBASE_RECEIVER 0U
#define UBX_TIMEBASE_GNSS     1U
#define UBX_TIMEBASE_UTC      2U

// ---- CFG configuration interface (generation 9 and later) --------------
// Key ID bits 28..30 encode the value size; see UBX_CFG_KEY_VALUE_SIZE().
#define UBX_CFG_LAYER_RAM   0x01U
#define UBX_CFG_LAYER_BBR   0x02U
#define UBX_CFG_LAYER_FLASH 0x04U
// CFG-VALGET selects a single layer by number, not by bitmask.
#define UBX_CFG_GET_LAYER_RAM     0U
#define UBX_CFG_GET_LAYER_BBR     1U
#define UBX_CFG_GET_LAYER_FLASH   2U
#define UBX_CFG_GET_LAYER_DEFAULT 7U

#define UBX_CFG_KEY_SIZE_SHIFT 28U
#define UBX_CFG_KEY_SIZE_MASK  0x7UL
// Returns the value size in bytes, or 0 for an invalid size code.
#define UBX_CFG_KEY_VALUE_SIZE(key)                                           \
    ((((key) >> UBX_CFG_KEY_SIZE_SHIFT) & UBX_CFG_KEY_SIZE_MASK) == 1U   ? 1U \
     : (((key) >> UBX_CFG_KEY_SIZE_SHIFT) & UBX_CFG_KEY_SIZE_MASK) == 2U ? 1U \
     : (((key) >> UBX_CFG_KEY_SIZE_SHIFT) & UBX_CFG_KEY_SIZE_MASK) == 3U ? 2U \
     : (((key) >> UBX_CFG_KEY_SIZE_SHIFT) & UBX_CFG_KEY_SIZE_MASK) == 4U ? 4U \
     : (((key) >> UBX_CFG_KEY_SIZE_SHIFT) & UBX_CFG_KEY_SIZE_MASK) == 5U ? 8U \
                                                                         : 0U)

// Port and protocol selection.
#define UBX_CFG_UART1_BAUDRATE     0x40520001UL // U4, baud
#define UBX_CFG_UART1INPROT_UBX    0x10730001UL // L
#define UBX_CFG_UART1INPROT_NMEA   0x10730002UL // L
#define UBX_CFG_UART1INPROT_RTCM3X 0x10730004UL // L
#define UBX_CFG_UART1OUTPROT_UBX   0x10740001UL // L
#define UBX_CFG_UART1OUTPROT_NMEA  0x10740002UL // L
// Message output rates on UART1, in navigation epochs (0 = off).
#define UBX_CFG_MSGOUT_NAV_POSLLH_UART1  0x2091002AUL // U1
#define UBX_CFG_MSGOUT_NAV_STATUS_UART1  0x2091001BUL // U1
#define UBX_CFG_MSGOUT_NAV_TIMEUTC_UART1 0x2091005CUL // U1
#define UBX_CFG_MSGOUT_NAV_CLOCK_UART1   0x20910066UL // U1
#define UBX_CFG_MSGOUT_NAV_TIMELS_UART1  0x20910061UL // U1
#define UBX_CFG_MSGOUT_NAV_EOE_UART1 \
    0x20910160UL // U1; from the M9 interface description, not in the dump
#define UBX_CFG_MSGOUT_TIM_TM2_UART1 0x20910179UL // U1
#define UBX_CFG_MSGOUT_TIM_TP_UART1 \
    0x2091017EUL // U1; from the M9 interface description, not in the dump
// Navigation rate and solution model.
#define UBX_CFG_RATE_MEAS       0x30210001UL // U2, ms
#define UBX_CFG_RATE_NAV        0x30210002UL // U2, measurements per solution
#define UBX_CFG_RATE_TIMEREF    0x20210003UL // E1, 0 UTC, 1 GPS, ...
#define UBX_CFG_NAVSPG_DYNMODEL 0x20110021UL // E1, 0 portable, 2 stationary
// Time pulse 1 (PPS).
#define UBX_CFG_TP_ANT_CABLEDELAY   0x30050001UL // I2, ns
#define UBX_CFG_TP_PERIOD_TP1       0x40050002UL // U4, us, while unlocked
#define UBX_CFG_TP_PERIOD_LOCK_TP1  0x40050003UL // U4, us, once locked
#define UBX_CFG_TP_LEN_TP1          0x40050004UL // U4, us, while unlocked
#define UBX_CFG_TP_LEN_LOCK_TP1     0x40050005UL // U4, us, once locked
#define UBX_CFG_TP_TP1_ENA          0x10050007UL // L
#define UBX_CFG_TP_SYNC_GNSS_TP1    0x10050008UL // L
#define UBX_CFG_TP_USE_LOCKED_TP1   0x10050009UL // L
#define UBX_CFG_TP_ALIGN_TO_TOW_TP1 0x1005000AUL // L
#define UBX_CFG_TP_POL_TP1          0x1005000BUL // L, 1 = rising edge on the second
#define UBX_CFG_TP_TIMEGRID_TP1     0x2005000CUL // E1, 0 UTC, 1 GPS, ...
#define UBX_CFG_TP_PULSE_DEF        0x20050023UL // E1, 0 period, 1 frequency
#define UBX_CFG_TP_PULSE_LENGTH_DEF 0x20050030UL // E1, 0 ratio, 1 length

#define UBX_DYNMODEL_PORTABLE          0U
#define UBX_DYNMODEL_STATIONARY        2U
#define UBX_TIMEREF_UTC                0U
#define UBX_TIMEREF_GPS                1U
#define UBX_TIMEGRID_UTC               0U
#define UBX_TP_PULSE_DEF_PERIOD        0U
#define UBX_TP_PULSE_LENGTH_DEF_LENGTH 1U

// ---- Wire layouts ------------------------------------------------------
#pragma pack(push, 1)
typedef struct
{
    uint8_t sync_char_1;
    uint8_t sync_char_2;
    uint8_t class_id;
    uint8_t message_id;
    uint16_t payload_length;
} UbxHeaderWire;

// UBX-NAV-POSLLH (01 02). WGS84 position.
typedef struct
{
    uint32_t time_of_week_ms;
    int32_t longitude_e7; // 1e-7 degrees
    int32_t latitude_e7;  // 1e-7 degrees
    int32_t height_ellipsoid_mm;
    int32_t height_msl_mm;
    uint32_t horizontal_accuracy_mm;
    uint32_t vertical_accuracy_mm;
} UbxNavPosllhWire;

// UBX-NAV-STATUS (01 03).
typedef struct
{
    uint32_t time_of_week_ms;
    uint8_t fix_type;     // UBX_FIX_*; qualify with UBX_STATUS_FIX_OK
    uint8_t flags;        // UBX_STATUS_*
    uint8_t fix_status;   // UBX_FIXSTAT_*
    uint8_t status_flags; // power-save, spoofing, carrier solution
    uint32_t time_to_first_fix_ms;
    uint32_t receiver_uptime_ms;
} UbxNavStatusWire;

// UBX-NAV-TIMEUTC (01 21). The time is calendar + fractional_second_ns,
// where fractional_second_ns is signed (-1e9..1e9). second may be 60
// during a leap second.
typedef struct
{
    uint32_t time_of_week_ms;
    uint32_t time_accuracy_ns;
    int32_t fractional_second_ns;
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t validity_flags; // UBX_TIMEUTC_*
} UbxNavTimeutcWire;

// UBX-NAV-CLOCK (01 22).
typedef struct
{
    uint32_t time_of_week_ms;
    int32_t clock_bias_ns;
    int32_t clock_drift_ns_per_s;
    uint32_t time_accuracy_ns;
    uint32_t frequency_accuracy_ps_per_s;
} UbxNavClockWire;

// UBX-NAV-TIMELS (01 26). Leap second state and the next scheduled event.
typedef struct
{
    uint32_t time_of_week_ms;
    uint8_t version; // 0 for this layout
    uint8_t reserved_0[3];
    uint8_t current_source;      // source of current_leap_seconds
    int8_t current_leap_seconds; // GPS-UTC, s
    uint8_t change_source;       // source of upcoming change information
    int8_t leap_second_change;   // -1, 0 or +1 at the next event
    int32_t time_to_event_s;     // negative once the event has passed
    uint16_t event_gps_week;
    uint16_t event_gps_day;
    uint8_t reserved_1[3];
    uint8_t validity_flags; // UBX_TIMELS_*
} UbxNavTimelsWire;

// UBX-NAV-EOE (01 61). End of epoch: the last navigation message the
// receiver sends for this time of week.
typedef struct
{
    uint32_t time_of_week_ms;
} UbxNavEoeWire;

// UBX-TIM-TP (0D 01). Sent once per pulse, ahead of it: describes the NEXT
// time pulse. qErr is that pulse's quantisation error: the receiver can only
// place its edge on its own clock grid, and says by how much it missed.
typedef struct
{
    uint32_t time_of_week_ms;     // of the next pulse, in the flags' time base
    uint32_t time_of_week_sub_ms; // 2^-32 ms
    int32_t quantisation_error_ps;
    uint16_t week;
    uint8_t flags;          // UBX_TP_FLAGS_*
    uint8_t reference_info; // UBX_TP_REFINFO_*
} UbxTimTpWire;

// UBX-TIM-TM2 (0D 03). External time-mark measurements. Week and TOW use
// the time base indicated by flags, not always UTC.
typedef struct
{
    uint8_t channel;
    uint8_t flags; // UBX_TM2_*
    uint16_t rising_edge_count;
    uint16_t rising_week;
    uint16_t falling_week;
    uint32_t rising_time_of_week_ms;
    uint32_t rising_sub_ms_ns;
    uint32_t falling_time_of_week_ms;
    uint32_t falling_sub_ms_ns;
    uint32_t accuracy_ns;
} UbxTimTm2Wire;

// UBX-ACK-ACK / UBX-ACK-NAK (05 01 / 05 00).
typedef struct
{
    uint8_t class_id;   // class of the acknowledged message
    uint8_t message_id; // id of the acknowledged message
} UbxAckWire;

// Header shared by CFG-VALSET (version 0) and CFG-VALGET (request version 0,
// response version 1). Key/value pairs follow.
typedef struct
{
    uint8_t version;
    uint8_t layer;     // VALSET: bitmask; VALGET: layer number
    uint16_t position; // VALGET only; VALSET: reserved, send zero
} UbxCfgValHeaderWire;
#pragma pack(pop)

#define UBX_SIZE_CHECK(type, n) typedef char ubx_size_check_##type[(sizeof(type) == (n)) ? 1 : -1]
UBX_SIZE_CHECK(UbxHeaderWire, UBX_HEADER_LENGTH);
UBX_SIZE_CHECK(UbxNavPosllhWire, UBX_NAV_POSLLH_LENGTH);
UBX_SIZE_CHECK(UbxNavStatusWire, UBX_NAV_STATUS_LENGTH);
UBX_SIZE_CHECK(UbxNavTimeutcWire, UBX_NAV_TIMEUTC_LENGTH);
UBX_SIZE_CHECK(UbxNavClockWire, UBX_NAV_CLOCK_LENGTH);
UBX_SIZE_CHECK(UbxNavTimelsWire, UBX_NAV_TIMELS_LENGTH);
UBX_SIZE_CHECK(UbxNavEoeWire, UBX_NAV_EOE_LENGTH);
UBX_SIZE_CHECK(UbxTimTpWire, UBX_TIM_TP_LENGTH);
UBX_SIZE_CHECK(UbxTimTm2Wire, UBX_TIM_TM2_LENGTH);
UBX_SIZE_CHECK(UbxAckWire, UBX_ACK_LENGTH);
UBX_SIZE_CHECK(UbxCfgValHeaderWire, UBX_CFG_VAL_HEADER_LENGTH);
#undef UBX_SIZE_CHECK

#endif
