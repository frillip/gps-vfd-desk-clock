#include "fe5680a_58.h"

char fe5680_config_string[64] = {0};
enum fe5680_state rb_state = FE5680_INIT;

long double fe5680_calc_rb_acc(int32_t slipped_cycles, time_t time_delta)
{
    long double accuracy;
    accuracy = slipped_cycles;
    accuracy /= time_delta;
    accuracy /= FCYCLE;
    
    return accuracy;
}

long double fe5680_calc_rb_freq(long double accuracy)
{
    long double freq;
    freq = (accuracy + 1) * FRBSET;
    
    return freq;
}

long double fe5680_calc_rb_r_val(long double freq, uint32_t f_val)
{
    long double r_val;
    r_val = freq / f_val;
    r_val *= ADRES;
    
    return r_val;
}

uint32_t fe5680_calc_rb_f_val(long double freq, uint32_t f_val)
{
    long double r_res;
    r_res = freq / f_val;
    long double dbl_f_val;
    dbl_f_val = FRBSET+0.01;
    dbl_f_val /= r_res;
    uint32_t new_f_val = dbl_f_val;
    
    return new_f_val;
}

void fe5680_get_config_string(void)
{
    char buf[2] = {'S', '\r'};
    sc16is7x0_nwrite(&buf, 2);
}

bool fe5680_process_config_string(char *config)
{
    if(sc16is7x0_rx_lvl() == 42)
    {
        sc16is7x0_nread(config, 42);
        return 1;
    }
    return 0;
}

long double fe5680_get_r_val(char *config)
{
    char r_val_string[16] = {0};
    memcpy(r_val_string, config+2, 15);
    char r_val_i_string[9] = {0};
    memcpy(r_val_i_string, config+2, 8);
    char r_val_i1_string[5] = {0};
    memcpy(r_val_i1_string, config+2, 4);
    char r_val_i2_string[5] = {0};
    memcpy(r_val_i2_string, config+6, 4);
    char r_val_f1_string[4] = {0};
    memcpy(r_val_f1_string, config+11, 3);
    char r_val_f2_string[4] = {0};
    memcpy(r_val_f2_string, config+14, 3);
    
    uint32_t r_val_i1 = atol(r_val_i1_string);
    uint32_t r_val_i2 = atol(r_val_i2_string);
    uint32_t r_val_i = (r_val_i1 * 10000) + r_val_i2;
    
    long double r_val_f = atol(r_val_f2_string);
    r_val_f /= 1000;
    r_val_f += atol(r_val_f1_string);
    r_val_f /= 1000;
    long double r_val = r_val_i + r_val_f;

    return r_val;
}

uint32_t fe5680_get_f_val(char *config)
{
    char f_val_string[9] = {0};
    memcpy(f_val_string, config+22, 8);
    uint32_t f_val;
    
    sscanf(f_val_string, "%0lX", &f_val);
    
    return f_val;
}

bool fe5680_set_r_val(long double r_val)
{
    char buf[18] = {0};
    uint32_t r_val_i = r_val;
    float r_val_f = r_val - r_val_i;
    sprintf(buf, "R=%lu.%06.0f\r", r_val_i, r_val_f*1000000);
    
    if(sc16is7x0_nwrite(&buf, 18)) return 1;
    else return 0;
}

bool fe5680_set_f_val(uint32_t f_val)
{
    char buf[19] = {0};
    sprintf(buf, "F=%0lX00000000\r", f_val);
    
    if(sc16is7x0_nwrite(&buf, 19)) return 1;
    else return 0;
}

bool fe5680_get_response(void)
{
    if(sc16is7x0_rx_lvl() == 3)
    {
        char ok_str[3] = "OK\r";
        char buf[3] = {0};
        sc16is7x0_nread(&buf, 3);
        if(memcmp(ok_str, buf, 3))
        {
            return 1;
        }
    }
    return 0;
}