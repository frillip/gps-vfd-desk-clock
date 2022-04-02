/* 
 * File:   gps_pps.h
 * Author: Frillip
 *
 * Created on October 23, 2021, 7:36 PM
 */

#ifndef GPS_PPS_H
#define	GPS_PPS_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <xc.h>
    
void gps_pps_init(void);
void IC1_Initialize (void);
void IC2_Initialize (void);

#ifdef	__cplusplus
}
#endif

#endif	/* GPS_PPS_H */

