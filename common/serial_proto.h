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

#define SERIAL_PROTO_HEADER_MAGIC 0x83
#define SERIAL_PROTO_TYPE_ESP_TX 0xC6
#define SERIAL_PROTO_TYPE_PIC_TX 0xB6
#define SERIAL_PROTO_DATATYPE_TIMEDATA 0xF0
#define SERIAL_PROTO_DATATYPE_GNSSDATA 0xF1
#define SERIAL_PROTO_DATATYPE_OFFSETDATA 0xF2
#define SERIAL_PROTO_DATATYPE_NETDATA 0xF3
#define SERIAL_PROTO_DATATYPE_RTCDATA 0xF4
#define SERIAL_PROTO_DATATYPE_SENSORDATA 0xF5
#define SERIAL_PROTO_DATATYPE_DISPLAYDATA 0xF6
#define SERIAL_PROTO_DATATYPE_MISCDATA 0xF7
#define SERIAL_PROTO_DATATYPE_USERDATA 0xF8

#pragma pack(push, 1)

// Structs for PIC data

typedef struct __attribute__((packed)) _serial_proto_header
{
    uint8_t magic;
    uint8_t type;
    uint8_t datatype;
} SERIAL_PROTO_HEADER;

typedef union
{
  struct __attribute__((packed)) _pic_time_struct
  {
    SERIAL_PROTO_HEADER header;
    
    CLOCK_SOURCE utc_source : 8;
    time_t utc : 32;
    
    struct __attribute__((packed))
    {
        uint8_t tz_set : 1;
        int8_t tz_offset: 7;
    } tz_flags;
    struct __attribute__((packed))
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
  struct __attribute__((packed)) _pic_gnss_struct
  {
    SERIAL_PROTO_HEADER header;
    
    struct __attribute__((packed))
    {
        uint8_t gnss_detected : 1;
        uint8_t gnss_fix : 1;
        uint8_t fix_ok : 1;
        uint8_t utc_valid : 1;
        uint8_t timemark_valid : 1;
        UBX_NAV_STATUS_GPSFIX fix_status : 4;
    } flags;
    time_t gnss : 32;
    uint32_t gnss_pps_count;
    int32_t posllh_lat;
    int32_t posllh_lon;
    int16_t posllh_height;
    int16_t posllh_hmsl;
  } fields;
  uint8_t raw[sizeof(struct _pic_gnss_struct)];
} SERIAL_PROTO_DATA_PIC_GNSS;


typedef union
{
  struct __attribute__((packed)) _pic_offset_struct
  {
    SERIAL_PROTO_HEADER header;
    
    CLOCK_SYNC_STATUS sync_state : 8;
    CLOCK_SYNC_STATUS sync_state_last : 8;
    CLOCK_SYNC_STATUS last_sync_cause : 8;
    uint32_t fosc_freq;
    int32_t oc_offset;
    int32_t accumulated_clocks;
    time_t accumulation_delta : 32;
    uint32_t total_oc_seq_count;
    uint32_t pps_seq_count;
    uint32_t pps_missing_count;
    uint32_t sync_events;
  } fields;
  uint8_t raw[sizeof(struct _pic_offset_struct)];
} SERIAL_PROTO_DATA_PIC_OFFSET;


typedef union
{
  struct __attribute__((packed)) _pic_rtc_struct
  {
    SERIAL_PROTO_HEADER header;
    
    struct __attribute__((packed))
    {
        uint8_t rtc_detected : 1;
        uint8_t rtc_valid : 1;
        uint8_t rtc_sync : 1;
        PIC_RTC_TYPE rtc_type : 4;
    } flags;
    time_t rtc : 32;
  } fields;
  uint8_t raw[sizeof(struct _pic_rtc_struct)];
} SERIAL_PROTO_DATA_PIC_RTC;


typedef union
{
  struct __attribute__((packed)) _pic_net_struct
  {
    SERIAL_PROTO_HEADER header;
    
    struct __attribute__((packed))
    {
        uint8_t reset_config: 1;
    } flags;
  } fields;
  uint8_t raw[sizeof(struct _pic_net_struct)];
} SERIAL_PROTO_DATA_PIC_NET;


typedef union
{
  struct __attribute__((packed)) _pic_sensor_struct
  {
    SERIAL_PROTO_HEADER header;
    
    struct __attribute__((packed))
    {
        uint8_t veml6040_detected : 1;
        uint8_t bme280_detected : 1;
    } flags;
    uint16_t lux;
    uint16_t temp;
    uint16_t temp_raw;
    uint16_t pres;
    uint16_t hum;
  } fields;
  uint8_t raw[sizeof(struct _pic_sensor_struct)];
} SERIAL_PROTO_DATA_PIC_SENSOR;


typedef union
{
  struct __attribute__((packed)) _pic_display_struct
  {
    SERIAL_PROTO_HEADER header;
    
    struct __attribute__((packed))
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
    struct __attribute__((packed))
    {
        UI_DISPLAY_STATE current : 4;
        UI_DISPLAY_STATE selected : 4;
    } display_state;
    UI_MENU_STATE menu_state : 8;
  } fields;
  uint8_t raw[sizeof(struct _pic_display_struct)];
} SERIAL_PROTO_DATA_PIC_DISPLAY;


typedef union
{
  struct __attribute__((packed)) _pic_user_struct
  {
    SERIAL_PROTO_HEADER header;
    
    USER_CMD cmd : 8;
    uint8_t padding;
    uint32_t arg;
  } fields;
  uint8_t raw[sizeof(struct _pic_user_struct)];
} SERIAL_PROTO_DATA_PIC_USER;



// Structs for ESP data

typedef union
{
  struct __attribute__((packed)) _esp_time_struct
  {
    SERIAL_PROTO_HEADER header;
    
    struct __attribute__((packed))
    {
        uint8_t wifi_status : 1;
        uint8_t ntp_status : 1;
        uint8_t pps_sync : 1;
        uint8_t scheduler_sync : 1;
    } flags;
    time_t utc : 32;
    struct __attribute__((packed))
    {
        uint8_t tz_set : 1;
        int8_t tz_offset: 7;
    } tz_flags;
    struct __attribute__((packed))
    {
        uint8_t dst_set : 1;
        uint8_t dst_active : 1;
        int8_t dst_offset: 4;
    } dst_flags;
  } fields;
  uint8_t raw[sizeof(struct _esp_time_struct)];
} SERIAL_PROTO_DATA_ESP_TIME;


typedef union
{
  struct __attribute__((packed)) _esp_net_struct
  {
    SERIAL_PROTO_HEADER header;
    
    struct __attribute__((packed))
    {
        uint8_t wifi_status : 1;
        uint8_t ntp_status : 1;
        uint8_t pps_sync : 1;
        uint8_t scheduler_sync : 1;
    } flags;
    time_t lastUpdate : 32;
    uint16_t ntpInterval;
    uint8_t dstFlags;
  } fields;
  uint8_t raw[sizeof(struct _esp_net_struct)];
} SERIAL_PROTO_DATA_ESP_NET;


typedef union
{
  struct __attribute__((packed)) _esp_rtc_struct
  {
    SERIAL_PROTO_HEADER header;
    
    struct __attribute__((packed))
    {
        uint8_t rtc_detected : 1;
        uint8_t rtc_valid : 1;
        uint8_t rtc_sync : 1;
    } flags;
    time_t rtc : 32;
  } fields;
  uint8_t raw[sizeof(struct _esp_rtc_struct)];
} SERIAL_PROTO_DATA_ESP_RTC;


typedef union
{
  struct __attribute__((packed)) _esp_sensor_struct
  {
    SERIAL_PROTO_HEADER header;
    
    struct __attribute__((packed))
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
} SERIAL_PROTO_DATA_ESP_SENSOR;


typedef union
{
  struct __attribute__((packed)) _esp_display_struct
  {
    SERIAL_PROTO_HEADER header;
    
    struct __attribute__((packed))
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
} SERIAL_PROTO_DATA_ESP_DISPLAY;


typedef union
{
  struct __attribute__((packed)) _esp_user_struct
  {
    SERIAL_PROTO_HEADER header;
    
    USER_CMD cmd : 8;
    uint8_t padding;
    uint32_t arg;
  } fields;
  uint8_t raw[sizeof(struct _esp_user_struct)];
} SERIAL_PROTO_DATA_ESP_USER;

#pragma pack(pop)

#ifdef	__cplusplus
}
#endif

#endif	/* ENUMS_H */

