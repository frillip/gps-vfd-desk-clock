/* 
 * File:   gnss_pps.h
 * Author: Frillip
 *
 * Created on October 23, 2021, 7:36 PM
 */

#ifndef GNSS_PPS_H
#define	GNSS_PPS_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <xc.h>
    
#include "freq.h"

void gnss_pps_init(void);
void IC1_Initialize (void);
void IC2_Initialize (void);
void calculate_pps_stats(void);
void reset_pps_stats(void);
uint32_t calculate_fosc_freq(uint32_t fosc_freq);

#ifdef	__cplusplus
}
#endif

#endif	/* GNSS_PPS_H */

