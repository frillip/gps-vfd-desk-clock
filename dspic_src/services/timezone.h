/* 
 * File:   ui.h
 * Author: Frillip
 *
 * Created on 12 October 2026, 05:59
 */

#ifndef TIMEZONE_H
#define	TIMEZONE_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#include "../settings/eeprom.h"
    
#define TIMEZONE_TRANSITIONS_MAX    5U

#define TZ_FLAGS_DST_ACTIVE UINT16_C(0x0001)
#define TZ_DATA_FLAGS_VALID UINT16_C(0x0001)
    
typedef enum
{
    TIMEZONE_SOURCE_NONE,
    TIMEZONE_SOURCE_MANUAL,
    TIMEZONE_SOURCE_AUTO_EU,
    TIMEZONE_SOURCE_AUTO_US,
    TIMEZONE_SOURCE_TZINFO_CACHE,
    TIMEZONE_SOURCE_TZINFO_GEOIP,
    TIMEZONE_SOURCE_TZINFO_GNSS,
    TIMEZONE_SOURCE_INVALID
} TIMEZONE_SOURCE;

typedef struct
{
    /* flags:
     * bit 0: TZ_DST_ACTIVE
     * All other bits reserved; transmit zero. */
    uint16_t flags;
    uint32_t transition_utc;
    int32_t base_offset_s;
    int32_t dst_offset_s;
} TIMEZONE_TRANSITION;

typedef struct {
    /* flags:
     * bit 0: TZ_DATA_VALID
     * All other bits reserved; transmit zero. */
    uint16_t flags;
    uint8_t source;
    uint8_t transition_count;
    uint32_t update_utc;
    TIMEZONE_TRANSITION     current;
    TIMEZONE_TRANSITION     next[TIMEZONE_TRANSITIONS_MAX];
} TIMEZONE_DATA;

bool isDST(const time_t *tod);
time_t get_local_time(time_t utc);

#ifdef	__cplusplus
}
#endif

#endif	/* UI_H */

