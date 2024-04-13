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
#include"mcc_generated_files/system.h"
#include"mcc_generated_files/clock.h"
#include "ui.h"
#include "ublox_ubx.h"
    
#include "../common/enums.h"
    
#define GNSS_STRING_BUFFER_SIZE 100
#define GNSS_CHECK_BUFFER_SIZE 6
    
#define GNSS_DETECT_LIMIT 300
#define GNSS_FIX_LIMIT 3000

void gnss_init(void);

void gnss_rx(void);
void gnss_copy_buffer(GNSS_MESSAGE_TYPE gnss_waiting);
GNSS_MESSAGE_TYPE gnss_check_incoming(void);

void gnss_set_calendar(void);
bool gnss_is_calendar_sync(time_t utc);
void gnss_reset_calendar_sync(void);

time_t gnss_process_rmc(void);
void gnss_invalidate_data(void);

#ifdef	__cplusplus
}
#endif

#endif	/* GNSS_H */

