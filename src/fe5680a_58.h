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
    
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "sc16is7x0.h"

#define FCYCLE 40000000UL
#define FRBSET 10000000UL
#define ADRES 4294967296
#define TRILLION 1000000000000

enum fe5680_state {FE5680_INIT, FE5680_READ_CONFIG, FE5680_GOT_CONFIG, FE5680_ADJ_FVAL, FE5680_ADJ_RVAL, FE5680_RESP_WAIT, FE5680_RESP_OK};

long double fe5680_calc_rb_acc(int32_t slipped_cycles, time_t time_delta);
long double fe5680_calc_rb_freq(long double accuracy);
long double fe5680_calc_rb_r_val(long double freq, uint32_t f_val);
uint32_t fe5680_calc_rb_f_val(long double freq, uint32_t f_val);
void fe5680_get_config_string(void);
bool fe5680_process_config_string(char *config);
long double fe5680_get_r_val(char *config);
uint32_t fe5680_get_f_val(char *config);
bool fe5680_set_r_val(long double r_val);
bool fe5680_set_f_val(uint32_t f_val);
bool fe5680_get_response(void);




#ifdef	__cplusplus
}
#endif

#endif	/* FE5680A_58_H */

