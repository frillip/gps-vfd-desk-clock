/* 
 * File:   fe5680a_58.h
 * Author: Frillip
 *
 * Created on 19 February 2022, 21:38
 */

#ifndef FE5680A_58_H
#define	FE5680A_58_H

#ifdef	__cplusplus
extern "C" {
#endif
    
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#define FCYCLE 40000000UL
#define FRBSET 10000000UL
#define ADRES 4294967296
#define TRILLION 1000000000000

long double calc_rb_acc(int32_t slipped_cycles, time_t time_delta);
long double calc_rb_freq(long double accuracy);
long double calc_rb_r_val(long double freq, uint32_t f_val);
uint32_t calc_rb_f_val(long double freq, uint32_t f_val);


#ifdef	__cplusplus
}
#endif

#endif	/* FE5680A_58_H */

