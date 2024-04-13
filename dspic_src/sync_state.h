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
#include "../common/enums.h"

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

