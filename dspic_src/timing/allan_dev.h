/* 
 * File:   allan_dev.h
 * Author: Frillip
 *
 * Created on 06 April 2025, 19:45
 */

#ifndef ALLAN_DEV_H
#define	ALLAN_DEV_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h>
#include <xc.h>
#include <math.h>
    
#define ALLAN_FIXED_POINT_SCALE (1UL << 16)  // Scale factor = 65536
#define MAX_PPS_SAMPLES 1000  // Buffer size for Allan deviation analysis
float compute_allan_deviation(uint16_t tau);

#ifdef	__cplusplus
}
#endif

#endif	/* ALLAN_DEV_H */

