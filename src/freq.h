/* 
 * File:   freq.h
 * Author: Frillip
 *
 * Created on 11 July 2023, 22:41
 */

#ifndef FREQ_H
#define	FREQ_H

#ifdef	__cplusplus
extern "C" {
#endif

#define FCYCLE 40000000UL
#define FCYCLE_LOWER_LIM 36000000UL
#define FCYCLE_UPPER_LIM 44000000UL
#define FCYCLE_POSITIVE_SUM 30000000L
#define FCYCLE_NEGATIVE_SUM -30000000L
    
#define FCYCLE_ACC_LIM_POSITIVE 2000
#define FCYCLE_ACC_LIM_NEGATIVE -2000
#define FCYCLE_ACC_RESET_POSITIVE 20000L
#define FCYCLE_ACC_RESET_NEGATIVE -20000L
    
#define FCYCLE_ACC_INTERVAL_MIN 120
    
#define OC_OFFSET_CONST -107
#define OC_OFFSET_MARGIN 15
#define OC_OFFSET_MIN (OC_OFFSET_CONST - OC_OFFSET_MARGIN)
#define OC_OFFSET_MAX (OC_OFFSET_CONST + OC_OFFSET_MARGIN)
#define PPS_SYNC_MIN 10

#ifdef	__cplusplus
}
#endif

#endif	/* FREQ_H */

