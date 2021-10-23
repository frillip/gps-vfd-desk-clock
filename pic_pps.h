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
#include <xc.h>

void pic_pps_init(void);
void OC1_Initialize(void);
void OC2_Initialize(void);
void IC3_Initialize(void);
void IC4_Initialize(void);
void set_latch_cycles(uint32_t cycles);
    
#ifdef	__cplusplus
}
#endif

#endif	/* PPS_H */

