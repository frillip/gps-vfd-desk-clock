/* 
 * File:   pdo.h
 * Author: Frillip
 *
 * Created on October 24, 2021, 11:22 PM
 */

#ifndef PDO_H
#define	PDO_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <xc.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

void pdo_init(void);
float pdo_calculate_mv(uint16_t adc_val);
float pdo_calculate_ns(float mv);

typedef enum 
{
    PDO,//Channel Name:AN1   Assigned to:Shared Channel
    CTMU_Temp,//Channel Name:CTMU Temp   Assigned to:Shared Channel
    CTMU,//Channel Name:CTMU   Assigned to:Shared Channel
} ADC1_CHANNEL;

inline static void ADC1_ChannelSelect( ADC1_CHANNEL channel )
{
    switch(channel)
    {
        case PDO:
                AD1CHS0bits.CH0SA= 0x1;
                break;
        case CTMU_Temp:
                AD1CHS0bits.CH0SA= 0x1E;
                break;
        case CTMU:
                AD1CHS0bits.CH0SA= 0x1F;
                break;
        default:
                break;
    }
}

#ifdef	__cplusplus
}
#endif

#endif	/* PDO_H */

