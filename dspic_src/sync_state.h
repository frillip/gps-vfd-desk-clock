/* 
 * File:   sync_state.h
 * Author: Frillip
 *
 * Created on 30 December 2023, 14:15
 */

#ifndef SYNC_STATE_H
#define	SYNC_STATE_H

#ifdef	__cplusplus
extern "C" {
#endif

#include"mcc_generated_files/system.h"
#include"mcc_generated_files/clock.h"
#include"mcc_generated_files/pin_manager.h"
#include"mcc_generated_files/interrupt_manager.h"
#include"mcc_generated_files/i2c1.h"
#include"mcc_generated_files/spi2.h"
#include"mcc_generated_files/uart1.h"
#include"mcc_generated_files/uart2.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <xc.h>

#include "esp32.h"
#include "freq.h"
#include "gnss.h"
#include "gnss_pps.h"
#include "pcf8563.h"
#include "rtc.h"
#include "pic_pps.h"
#include "scheduler.h"
#include "sht30.h"
#include "sync_state.h"
#include "tubes.h"
#include "ublox_ubx.h"
#include "ui.h"
    
typedef enum
{
    SYNC_POWER_ON,
    SYNC_RTC_DETECT,
    SYNC_NTP_DETECT,
    SYNC_GNSS_DETECT,
    SYNC_GNSS_WAIT_FOR_FIX,
    SYNC_STARTUP,
    SYNC_NOSYNC,
    SYNC_ADJUST_STAGE_1,
    SYNC_ADJUST_STAGE_2,
    SYNC_PPS_SYNC,
    SYNC_SCHED_SYNC,
    SYNC_MIN_INTERVAL,
    SYNC_INTERVAL,
    SYNC_SYNC,
    SYNC_NOSYNC_MINOR,
    SYNC_NOSYNC_MINOR_OC,
    SYNC_NOSYNC_MAJOR,
    SYNC_NOSYNC_MAJOR_OC,
    SYNC_NOSYNC_FREQ,
    SYNC_NOSYNC_FREQ_ONLY,
    SYNC_NOSYNC_GNSS,
    SYNC_NOSYNC_MANUAL,
    SYNC_NO_CLOCK,
    SYNC_RTC,
    SYNC_NTP,
    SYNC_NTP_ADJUST,
    SYNC_NTP_NO_NETWORK,
} CLOCK_SYNC_STATUS;

typedef enum
{
    CLOCK_SOURCE_NONE,
    CLOCK_SOURCE_RTC,
    CLOCK_SOURCE_ESP,
    CLOCK_SOURCE_NTP,
    CLOCK_SOURCE_GNSS,
} CLOCK_SOURCE;

void sync_state_machine(void);
void sync_state_machine_set_state(CLOCK_SYNC_STATUS state);
void print_sync_state_machine(void);
CLOCK_SYNC_STATUS sync_select_best_clock(void);
void sync_state_print(CLOCK_SYNC_STATUS sync_state);
CLOCK_SYNC_STATUS pic_pps_evaluate_sync(void);
void sync_state_print_stats(void);
#define EVAL_TIME_DELAY_INTERVAL    5
void sync_state_eval_time(void);
void print_clocks(void);
void sync_set_clock_source(CLOCK_SOURCE source);
void print_clock_source(CLOCK_SOURCE source);


#ifdef	__cplusplus
}
#endif

#endif	/* SYNC_STATE_H */

