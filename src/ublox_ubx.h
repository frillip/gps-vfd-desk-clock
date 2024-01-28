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
#include "gnss.h"
#include "ui.h"
    
#define UBX_HEADER_LENGTH 6
#define UBX_CHECKSUM_LENGTH 2

#define UBX_TIM_TM2_PAYLOAD_LENGTH 28
#define UBX_NAV_TIMEUTC_PAYLOAD_LENGTH 20
#define UBX_NAV_CLOCK_PAYLOAD_LENGTH 20
#define UBX_NAV_STATUS_PAYLOAD_LENGTH 16
#define UBX_NAV_POSLLH_PAYLOAD_LENGTH 28

#define UBX_TIM_TM2_LENGTH UBX_HEADER_LENGTH + UBX_TIM_TM2_PAYLOAD_LENGTH + UBX_CHECKSUM_LENGTH
#define UBX_NAV_TIMEUTC_LENGTH UBX_HEADER_LENGTH + UBX_NAV_TIMEUTC_PAYLOAD_LENGTH + UBX_CHECKSUM_LENGTH
#define UBX_NAV_CLOCK_LENGTH UBX_HEADER_LENGTH + UBX_NAV_CLOCK_PAYLOAD_LENGTH + UBX_CHECKSUM_LENGTH
#define UBX_NAV_STATUS_LENGTH UBX_HEADER_LENGTH + UBX_NAV_STATUS_PAYLOAD_LENGTH + UBX_CHECKSUM_LENGTH
#define UBX_NAV_POSLLH_LENGTH UBX_HEADER_LENGTH + UBX_NAV_POSLLH_PAYLOAD_LENGTH + UBX_CHECKSUM_LENGTH
    
void process_ubx_tim_tm2(void);
void print_ubx_tim_tm2_data(void);

time_t process_ubx_nav_timeutc(void);
void print_ubx_nav_timeutc_data(void);

void process_ubx_nav_clock(void);
void print_ubx_nav_clock_data(void);

void process_ubx_nav_status(void);
void print_ubx_nav_status_data(void);

void process_ubx_nav_posllh(void);
void print_ubx_nav_posllh_data(void);

bool ubx_gnss_available(void);
void ubx_update_gnss_time(void);
bool ubx_gnss_time_valid(void);

bool ubx_timemark_waiting(void);
void ubx_update_timemark(void);

#ifdef	__cplusplus
}
#endif

#endif	/* UBLOX_UBX_H */

