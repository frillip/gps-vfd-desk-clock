/* 
 * File:   sht30.h
 * Author: Frillip
 *
 * Created on October 17, 2021, 11:26 PM
 */

#ifndef SHT30_H
#define	SHT30_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include "mcc_generated_files/i2c1.h"
    
#define SHT30_ADDRESS 0x44

void sht30_start_meas(void);
void sht30_read_data(void);
void sht30_start_periodic_meas(void);
void sht30_read_periodic_data(void);
double sht30_convert_temp(uint16_t val);
double sht30_convert_humidity(uint16_t val);
void print_sht30_data(void);

#ifdef	__cplusplus
}
#endif

#endif	/* SHT30_H */

