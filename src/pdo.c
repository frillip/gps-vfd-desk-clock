#include "pdo.h"

void pdo_init(void)
{
    // ASAM disabled; ADDMABM disabled; ADSIDL disabled; DONE disabled; SIMSAM Sequential; FORM Absolute decimal result, unsigned, right-justified; SAMP disabled; SSRC Clearing sample bit ends sampling and starts conversion; AD12B 12-bit; ADON enabled; SSRCG disabled; 
    AD1CON1 = 0x8400;
    // CSCNA disabled; VCFG0 AVDD; VCFG1 AVSS; ALTS disabled; BUFM disabled; SMPI Generates interrupt after completion of every sample/conversion operation; CHPS 1 Channel; 
    AD1CON2 = 0x00;
    // SAMC 1; ADRC FOSC/2; ADCS 9; 
    AD1CON3 = 0x109;
    // CH0SA AN0; CH0SB AN0; CH0NB AVSS; CH0NA AVSS; 
    AD1CHS0 = 0x00;
    // CSS25 disabled; CSS24 disabled; CSS31 disabled; CSS30 disabled; 
    AD1CSSH = 0x00;
    // CSS2 disabled; CSS1 disabled; CSS0 disabled; CSS5 disabled; CSS4 disabled; CSS3 disabled; 
    AD1CSSL = 0x00;
    // DMABL Allocates 1 word of buffer to each analog input; ADDMAEN disabled; 
    AD1CON4 = 0x00;
    // CH123SA disabled; CH123SB CH1=OA2/AN0,CH2=AN1,CH3=AN2; CH123NA disabled; CH123NB CH1=VREF-,CH2=VREF-,CH3=VREF-; 
    AD1CHS123 = 0x00;
}

float pdo_calculate_mv(uint16_t adc_val)
{
    return (adc_val * 16) / 19.859;
}

float pdo_calculate_ns(float mv)
{
    return (mv * mv * 0.000051) + (0.28 * mv);
}