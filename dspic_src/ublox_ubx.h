/* 
 * File:   ublox_ubx.h
 * Author: Frillip
 *
 * Created on 09 April 2023, 17:05
 */

#ifndef UBLOX_UBX_H
#define	UBLOX_UBX_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "mcc_generated_files/uart2.h"
#include "mcc_generated_files/uart1.h"
#include "../common/enums.h"
#include "gnss.h"
#include "ui.h"
    
#define UBX_HEADER_LENGTH               sizeof(struct _ubx_header)
#define UBX_CHECKSUM_LENGTH             sizeof(struct _ubx_checksum)
    
#define UBX_HEADER_PREAMBLE_1           0xB5
#define UBX_HEADER_PREAMBLE_2           0x62

#define UBX_HEADER_CLASS_NAV            0x01
#define UBX_HEADER_ID_NAV_CLOCK         0x22
#define UBX_HEADER_ID_NAV_POSLLH        0x02
#define UBX_HEADER_ID_NAV_STATUS        0x03
#define UBX_HEADER_ID_NAV_TIMEUTC       0x21

#define UBX_HEADER_CLASS_TIM            0x0D
#define UBX_HEADER_ID_TIM_TM2           0x03
    
#define UBX_NAV_CLOCK_PAYLOAD_LENGTH    sizeof(struct _ubx_nav_clock_payload)
#define UBX_NAV_POSLLH_PAYLOAD_LENGTH   sizeof(struct _ubx_nav_posllh_payload)
#define UBX_NAV_STATUS_PAYLOAD_LENGTH   sizeof(struct _ubx_nav_status_payload)
#define UBX_NAV_TIMEUTC_PAYLOAD_LENGTH  sizeof(struct _ubx_nav_timeutc_payload)
#define UBX_TIM_TM2_PAYLOAD_LENGTH      sizeof(struct _ubx_tim_tm2_payload)


typedef union
{
  struct __attribute__ ((packed)) _ubx_header
  {
    uint8_t preamble_sync_1;
    uint8_t preamble_sync_2;
    uint8_t class;
    uint8_t id;
    uint16_t length;
  } fields;
  uint8_t raw[sizeof(struct _ubx_header)];
} UBX_HEADER;

struct __attribute__ ((packed)) _ubx_checksum
{
    uint8_t ck_a;
    uint8_t ck_b;
};


struct __attribute__ ((packed)) _ubx_nav_clock_payload
{
    uint32_t itow;
    int32_t clkb;
    int32_t clkd;
    uint32_t tacc;
    uint32_t facc;
};

typedef union
{
  struct __attribute__ ((packed)) _ubx_nav_clock
  {
    UBX_HEADER header;
    struct _ubx_nav_clock_payload payload;
    struct _ubx_checksum checksum;
  } fields;
  uint8_t raw[sizeof(struct _ubx_nav_clock)];
} UBX_NAV_CLOCK;


struct __attribute__ ((packed)) _ubx_nav_posllh_payload
{
    uint32_t itow;
    int32_t lon;
    int32_t lat;
    int32_t height;
    int32_t hmsl;
    uint32_t hacc;
    uint32_t vacc;
};

typedef union
{
  struct __attribute__ ((packed)) _ubx_nav_posllh
  {
    UBX_HEADER header;
    struct _ubx_nav_posllh_payload payload;
    struct _ubx_checksum checksum;
  } fields;
  uint8_t raw[sizeof(struct _ubx_nav_posllh)];
} UBX_NAV_POSLLH;

struct __attribute__ ((packed)) _ubx_nav_status_payload
{
    uint32_t itow;
    UBX_NAV_STATUS_GPSFIX gpsfix : 8;
    struct __attribute__ ((packed))
    {
        uint8_t gpsfixok : 1;
        uint8_t diffsoln : 1;
        uint8_t wknset: 1;
        uint8_t towset: 1;
    } flags;
    struct __attribute__ ((packed))
    {
        uint8_t diffcorr : 1;
        uint8_t carrsolnvalid : 1;
        uint8_t padding: 4;
        uint8_t mapmatching: 2;
    } fixstat;
    struct __attribute__ ((packed))
    {
        uint8_t psmstate : 2;
        uint8_t padding : 1;
        uint8_t spoofdetstate: 2;
        uint8_t padding2 : 1;
        uint8_t carrsoln: 2;
    } flags2;
    uint32_t ttff;
    uint32_t msss;
};

typedef union
{
  struct __attribute__ ((packed)) _ubx_nav_status
  {
    UBX_HEADER header;
    struct _ubx_nav_status_payload payload;
    struct _ubx_checksum checksum;
  } fields;
  uint8_t raw[sizeof(struct _ubx_nav_status)];
} UBX_NAV_STATUS;


struct __attribute__ ((packed)) _ubx_nav_timeutc_payload
{
    uint32_t itow;
    uint32_t tacc;
    int32_t nano;
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t min;
    uint8_t sec;
    struct __attribute__ ((packed))
    {
        uint8_t validtow : 1;
        uint8_t validwkn : 1;
        uint8_t validutc: 1;
        uint8_t authstatus: 1;
        UBX_NAV_TIMEUTC_VALID_UTCSTANDARD utcstandard: 4;
    } valid;
};

typedef union
{
  struct __attribute__ ((packed)) _ubx_nav_timeutc
  {
    UBX_HEADER header;
    struct _ubx_nav_timeutc_payload payload;
    struct _ubx_checksum checksum;
  } fields;
  uint8_t raw[sizeof(struct _ubx_nav_timeutc)];
} UBX_NAV_TIMEUTC;


struct __attribute__ ((packed)) _ubx_tim_tm2_payload
{
    uint8_t ch;
    struct __attribute__ ((packed))
    {
        uint8_t mode : 1;
        uint8_t run : 1;
        uint8_t newfallingedge: 1;
        UBX_TIM_TM2_FLAGS_TIMEBASE timebase: 2;
        uint8_t utc : 1;
        uint8_t time : 1;
        uint8_t newrisingedge: 1;
    } flags;
    uint16_t count;
    uint16_t wnr;
    uint16_t wnf;
    uint32_t towmsr;
    uint32_t towsubmsr;
    uint32_t towmsf;
    uint32_t towsubmsf;
    uint32_t accest;
};

typedef union
{
  struct __attribute__ ((packed)) _ubx_tim_tm2
  {
    UBX_HEADER header;
    struct _ubx_tim_tm2_payload payload;
    struct _ubx_checksum checksum;
  } fields;
  uint8_t raw[sizeof(struct _ubx_tim_tm2)];
} UBX_TIM_TM2;

void process_ubx_nav_clock(void);
void print_ubx_nav_clock_data(void);

void process_ubx_nav_posllh(void);
void print_ubx_nav_posllh_data(void);

void process_ubx_nav_status(void);
void print_ubx_nav_status_data(void);

time_t process_ubx_nav_timeutc(void);
void print_ubx_nav_timeutc_data(void);

void process_ubx_tim_tm2(void);
void print_ubx_tim_tm2_data(void);

void print_ubx_data(void);

bool ubx_gnss_available(void);
void ubx_update_gnss_time(void);
bool ubx_gnss_time_valid(void);

bool ubx_timemark_waiting(void);
void ubx_update_timemark(void);

void ubx_data_task(void);
void ubx_invalidate_data(void);

#ifdef	__cplusplus
}
#endif

#endif	/* UBLOX_UBX_H */

