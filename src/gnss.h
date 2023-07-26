/* 
 * File:   gnss.h
 * Author: Frillip
 *
 * Created on October 2, 2021, 9:14 PM
 */

#ifndef GNSS_H
#define	GNSS_H

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
#include "ui.h"
#include "ublox_ubx.h"
    
#define GNSS_BUFFER_SIZE 100
#define CHECK_BUFFER_SIZE 6
    
void rx_gnss(void);

void sync_gnss_calendar(void);
bool is_gnss_calendar_sync(time_t utc);
void reset_gnss_calendar_sync(void);

time_t process_rmc(void);

enum gnss_message_type {GNSS_NONE, GNSS_UBX_TIM_TM2, GNSS_UBX_NAV_TIMEUTC, GNSS_UBX_NAV_CLOCK, GNSS_UBX_NAV_STATUS, GNSS_GNRMC};

#ifdef	__cplusplus
}
#endif

#endif	/* GNSS_H */

