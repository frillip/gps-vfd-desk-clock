/* 
 * File:   pps.h
 * Author: Frillip
 *
 * Created on October 23, 2021, 7:22 PM
 */

#ifndef PPS_H
#define	PPS_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <xc.h>
#include <stdio.h>
    
#include "freq.h"
#include "scheduler.h"
#include "ui.h"

void pic_pps_init(void);
void OC1_Initialize(void);
void OC2_Initialize(void);
void IC3_Initialize(void);
void IC4_Initialize(void);

void pic_pps_set_latch_cycles(uint32_t cycles);
void pic_pps_calculate_oc_stats(void);

void pic_pps_evaluate_sync(void);
bool pic_pps_manual_resync_available(void);
bool pic_pps_resync_required(void);
void pic_pps_resync(void);

void pic_pps_print_stats(void);
void pic_pps_reset_sync(void);
    
#ifdef	__cplusplus
}
#endif

#endif	/* PPS_H */

