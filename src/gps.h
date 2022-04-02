/* 
 * File:   gps.h
 * Author: Frillip
 *
 * Created on October 2, 2021, 9:14 PM
 */

#ifndef GPS_H
#define	GPS_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
    
void rx_gps(void);
void process_ubx(void);
void print_ubx_data(void);
time_t process_rmc(void);

#ifdef	__cplusplus
}
#endif

#endif	/* GPS_H */

