/* 
 * File:   enums.h
 * Author: Frillip
 *
 * Created on 13 April 2024, 19:33
 */

#ifndef SERIAL_PROTO_H
#define	SERIAL_PROTO_H

#ifdef	__cplusplus
extern "C" {
#endif

#define SERIAL_PROTO_STRING_BUFFER_SIZE 100
#define SERIAL_PROTO_CHECK_BUFFER_SIZE 3

#define SERIAL_PROTO_HEADER 0x83
#define SERIAL_PROTO_TYPE_ESP_TX 0x65
#define SERIAL_PROTO_TYPE_PIC_TX 0x70
#define SERIAL_PROTO_DATATYPE_TIMEDATA 0x00
#define SERIAL_PROTO_DATATYPE_GNSSDATA 0x10
#define SERIAL_PROTO_DATATYPE_OFFSETDATA 0x20
#define SERIAL_PROTO_DATATYPE_NETDATA 0x30
#define SERIAL_PROTO_DATATYPE_RTCDATA 0x40
#define SERIAL_PROTO_DATATYPE_SENSORDATA 0x50
#define SERIAL_PROTO_DATATYPE_DISPLAYDATA 0x60
#define SERIAL_PROTO_DATATYPE_MISCDATA 0x70
#define SERIAL_PROTO_DATATYPE_USERDATA 0x80

#define SERIAL_PROTO_ESP_TIME_LENGTH 11
#define SERIAL_PROTO_ESP_GNSS_LENGTH 12
#define SERIAL_PROTO_ESP_OFFSET_LENGTH 20
#define SERIAL_PROTO_ESP_NET_LENGTH 11
#define SERIAL_PROTO_ESP_RTC_LENGTH 7
#define SERIAL_PROTO_ESP_SENSOR_LENGTH 11
#define SERIAL_PROTO_ESP_DISPLAY_LENGTH 7
#define SERIAL_PROTO_ESP_USER_LENGTH 4

#define SERIAL_PROTO_PIC_TIME_LENGTH 10
#define SERIAL_PROTO_PIC_GNSS_LENGTH 12
#define SERIAL_PROTO_PIC_OFFSET_LENGTH 28
#define SERIAL_PROTO_PIC_NET_LENGTH 11
#define SERIAL_PROTO_PIC_RTC_LENGTH 8
#define SERIAL_PROTO_PIC_SENSOR_LENGTH 12
#define SERIAL_PROTO_PIC_DISPLAY_LENGTH 10
#define SERIAL_PROTO_PIC_USER_LENGTH 4

#pragma pack(push, 1)

// Structs for PIC data

typedef union
{
  struct __attribute__ ((packed)) _pic_time_struct
  {
    struct __attribute__ ((packed))
    {
        uint8_t magic;
        uint8_t type;
        uint8_t datatype;
    } header;
    CLOCK_SOURCE utc_source : 8;
    time_t utc;
    struct __attribute__ ((packed))
    {
        uint8_t tz_set : 1;
        int8_t tz_offset: 7;
    } tz_flags;
    struct __attribute__ ((packed))
    {
        uint8_t dst_set : 1;
        uint8_t dst_active : 1;
        int8_t dst_offset: 4;
    } dst_flags;
  } fields;
  uint8_t raw[sizeof(struct _pic_time_struct)];
} SERIAL_PROTO_DATA_PIC_TIME;

typedef union
{
  struct __attribute__ ((packed)) _pic_gnss_struct
  {
    struct __attribute__ ((packed))
    {
        uint8_t magic;
        uint8_t type;
        uint8_t datatype;
    } header;
    struct __attribute__ ((packed))
    {
        uint8_t gnss_fix : 1;
        uint8_t fix_ok : 1;
        uint8_t utc_valid : 1;
        uint8_t timemark_valid : 1;
        uint8_t fix_status : 4;
    } flags;
    int32_t posllh_lat;
    int32_t posllh_lon;
  } fields;
  uint8_t raw[sizeof(struct _pic_gnss_struct)];
} SERIAL_PROTO_DATA_PIC_GNSS;

typedef union
{
  struct __attribute__ ((packed)) _pic_offset_struct
  {
    struct __attribute__ ((packed))
    {
        uint8_t magic;
        uint8_t type;
        uint8_t datatype;
    } header;
    CLOCK_SYNC_STATUS sync_state : 8;
    CLOCK_SYNC_STATUS sync_state_last : 8;
    CLOCK_SYNC_STATUS last_sync_cause : 8;
    uint32_t fosc_freq;
    int32_t oc_offset;
    int32_t accumulated_clocks;
    time_t accumulation_delta;
    uint32_t total_oc_seq_count;
    uint32_t sync_events;
  } fields;
  uint8_t raw[sizeof(struct _pic_offset_struct)];
} SERIAL_PROTO_DATA_PIC_OFFSET;

typedef union
{
  struct __attribute__ ((packed)) _pic_rtc_struct
  {
    struct __attribute__ ((packed))
    {
        uint8_t magic;
        uint8_t type;
        uint8_t datatype;
    } header;
    struct __attribute__ ((packed))
    {
        uint8_t rtc_detected : 1;
        uint8_t rtc_valid : 1;
        uint8_t rtc_sync : 1;
    } flags;
    time_t rtc;
  } fields;
  uint8_t raw[sizeof(struct _pic_rtc_struct)];
} SERIAL_PROTO_DATA_PIC_RTC;

typedef union
{
  struct __attribute__ ((packed)) _pic_net_struct
  {
    struct __attribute__ ((packed))
    {
        uint8_t magic;
        uint8_t type;
        uint8_t datatype;
    } header;
    struct __attribute__ ((packed))
    {
        uint8_t reset_config: 1;
    } flags;
  } fields;
  uint8_t raw[sizeof(struct _pic_net_struct)];
} SERIAL_PROTO_DATA_PIC_NET;

typedef union
{
  struct __attribute__ ((packed)) _pic_sensor_struct
  {
    struct __attribute__ ((packed))
    {
        uint8_t magic;
        uint8_t type;
        uint8_t datatype;
    } header;
    struct __attribute__ ((packed))
    {
        uint8_t veml6040_detected : 1;
        uint8_t bme280_detected : 1;
    } flags;
    uint16_t lux;
    uint16_t temp;
    uint16_t pres;
    uint16_t hum;
  } fields;
  uint8_t raw[sizeof(struct _pic_sensor_struct)];
} SERIAL_PROTO_DATA_PIC_SENSOR;

typedef union
{
  struct __attribute__ ((packed)) _pic_display_struct
  {
    struct __attribute__ ((packed))
    {
        uint8_t magic;
        uint8_t type;
        uint8_t datatype;
    } header;
    struct __attribute__ ((packed))
    {
        uint8_t update_pending : 1;
        uint8_t brightness_manual : 1;
        uint8_t oc_running : 1;
        uint8_t pwr_stat : 1;
        uint8_t switch_state : 1;
        uint8_t button_state : 1;
    } flags;
    uint16_t brightness;
    uint16_t brightness_target;
    UI_DISPLAY_STATE display_state : 8;
    UI_MENU_STATE menu_state : 8;
  } fields;
  uint8_t raw[sizeof(struct _pic_display_struct)];
} SERIAL_PROTO_DATA_PIC_DISPLAY;

typedef union
{
  struct __attribute__ ((packed)) _pic_user_struct
  {
    struct __attribute__ ((packed))
    {
        uint8_t magic;
        uint8_t type;
        uint8_t datatype;
    } header;
    char c;
  } fields;
  uint8_t raw[sizeof(struct _pic_user_struct)];
} PIC_DATA_USER;

// Structs for ESP data

typedef union
{
  struct __attribute__ ((packed)) _esp_time_struct
  {
    struct __attribute__ ((packed))
    {
        uint8_t magic;
        uint8_t type;
        uint8_t datatype;
    } header;
    struct __attribute__ ((packed))
    {
        uint8_t wifi_status : 1;
        uint8_t ntp_status : 1;
        uint8_t pps_sync : 1;
        uint8_t scheduler_sync : 1;
    } flags;
    time_t utc;
    struct __attribute__ ((packed))
    {
        uint8_t tz_set : 1;
        int8_t tz_offset: 7;
    } tz_flags;
    struct __attribute__ ((packed))
    {
        uint8_t dst_set : 1;
        uint8_t dst_active : 1;
        int8_t dst_offset: 4;
    } dst_flags;
  } fields;
  uint8_t raw[sizeof(struct _esp_time_struct)];
} ESP_DATA_TIME;

typedef union
{
  struct __attribute__ ((packed)) _esp_net_struct
  {
    struct __attribute__ ((packed))
    {
        uint8_t magic;
        uint8_t type;
        uint8_t datatype;
    } header;
    struct __attribute__ ((packed))
    {
        uint8_t wifi_status : 1;
        uint8_t ntp_status : 1;
        uint8_t pps_sync : 1;
        uint8_t scheduler_sync : 1;
    } flags;
    time_t lastUpdate;
    uint16_t ntpInterval;
    uint8_t dstFlags;
  } fields;
  uint8_t raw[sizeof(struct _esp_net_struct)];
} ESP_DATA_NET;

typedef union
{
  struct __attribute__ ((packed)) _esp_rtc_struct
  {
    struct __attribute__ ((packed))
    {
        uint8_t magic;
        uint8_t type;
        uint8_t datatype;
    } header;
    struct __attribute__ ((packed))
    {
        uint8_t rtc_detected : 1;
        uint8_t rtc_valid : 1;
        uint8_t rtc_sync : 1;
    } flags;
    time_t rtc;
  } fields;
  uint8_t raw[sizeof(struct _esp_rtc_struct)];
} ESP_DATA_RTC;

typedef union
{
  struct __attribute__ ((packed)) _esp_sensor_struct
  {
    struct __attribute__ ((packed))
    {
        uint8_t magic;
        uint8_t type;
        uint8_t datatype;
    } header;
    struct __attribute__ ((packed))
    {
        uint8_t veml6040_detected : 1;
        uint8_t bme280_detected : 1;
    } flags;
    uint16_t lux;
    uint16_t temp;
    uint16_t pres;
    uint16_t hum;
  } fields;
  uint8_t raw[sizeof(struct _esp_sensor_struct)];
} ESP_DATA_SENSOR;

typedef union
{
  struct __attribute__ ((packed)) _esp_display_struct
  {
    struct __attribute__ ((packed))
    {
        uint8_t magic;
        uint8_t type;
        uint8_t datatype;
    } header;
    struct __attribute__ ((packed))
    {
        uint8_t update_pending : 1;
        uint8_t brightness_manual : 1;
        uint8_t oc_running : 1;
        uint8_t pwr_stat : 1;
        uint8_t switch_state : 1;
        uint8_t button_state : 1;
    } flags;
    uint16_t brightness;
    uint16_t brightness_target;
    UI_DISPLAY_STATE display_state : 8;
    UI_MENU_STATE menu_state : 8;
  } fields;
  uint8_t raw[sizeof(struct _esp_display_struct)];
} ESP_DATA_DISPLAY;

typedef union
{
  struct __attribute__ ((packed)) _esp_user_struct
  {
    struct __attribute__ ((packed))
    {
        uint8_t magic;
        uint8_t type;
        uint8_t datatype;
    } header;
    char c;
  } fields;
  uint8_t raw[sizeof(struct _esp_user_struct)];
} ESP_DATA_USER;

#pragma pack(pop)

#ifdef	__cplusplus
}
#endif

#endif	/* ENUMS_H */
