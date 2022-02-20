#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "fe5680a_58.h"

long double calc_rb_acc(int32_t slipped_cycles, time_t time_delta)
{
    long double accuracy;
    accuracy = slipped_cycles;
    accuracy /= time_delta;
    accuracy /= FCYCLE;
    
    return accuracy;
}

long double calc_rb_freq(long double accuracy)
{
    long double freq;
    freq = (accuracy + 1) * FRBSET;
    
    return freq;
}

long double calc_rb_r_val(long double freq, uint32_t f_val)
{
    long double r_val;
    r_val = freq / f_val;
    r_val *= ADRES;
    
    return r_val;
}

uint32_t calc_rb_f_val(long double freq, uint32_t f_val)
{
    long double r_res;
    r_res = freq / f_val;
    long double dbl_f_val;
    dbl_f_val = FRBSET+0.01;
    dbl_f_val /= r_res;
    //printf("\r\ndbl_f: %.3lf\r\n",dbl_f_val);
    uint32_t new_f_val = dbl_f_val;
    
    return new_f_val;
}
