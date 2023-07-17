#include "gnss_pps.h"
#include <math.h>

uint16_t ic1_val = 0;
uint16_t ic1_val_old = 0;
uint16_t ic2_val = 0;
uint16_t ic2_val_old = 0;
uint32_t pps_count = 0;
uint32_t pps_count_diff = 0;
uint32_t pps_count_old = 0;
uint32_t pps_seq_count = 0;
int32_t accumulated_clocks = 0;
time_t accumulation_start = 0;
time_t accumulation_delta = 0;

extern time_t utc;
extern uint16_t counter;
extern uint32_t fosc_freq;

void gnss_pps_init(void)
{
    // Init IC2 first
    IC2_Initialize();
    // Then IC1
    IC1_Initialize();
}

void IC1_Initialize (void)
{
    // ICSIDL disabled; ICM Simple Capture mode: Rising; ICTSEL FOSC/2; ICI Every; 
    IC1CON1 = 0x1C03;
    // SYNCSEL PTGO8; TRIGSTAT disabled; IC32 enabled; ICTRIG Sync; 
    IC1CON2 = 0x10A;
    IFS0bits.IC1IF = false; // Clear the interrupt flag
    IEC0bits.IC1IE = true; // Enable interrupt
    IPC0bits.IC1IP = 1; // Set interrupt priority
}

void IC2_Initialize (void)
{
    // ICSIDL disabled; ICM Simple Capture mode: Rising; ICTSEL FOSC/2; ICI Every; 
    IC2CON1 = 0x1C03;
    // SYNCSEL PTGO9; TRIGSTAT disabled; IC32 enabled; ICTRIG Sync; 
    IC2CON2 = 0x10A;
    IFS0bits.IC2IF = false; // Clear the interrupt flag
    IEC0bits.IC2IE = true; // Enable interrupt
    IPC1bits.IC2IP = 1; // Set interrupt priority
}

void calculate_pps_stats(void)
{
    // Calculate our PPS and OC stats
    pps_count = (((uint32_t)ic2_val)<<16) + ic1_val; // Raw timer
    pps_count_diff = pps_count-pps_count_old; // Difference from last
    pps_count_old = pps_count; // Store the new value as old
    accumulated_clocks += pps_count_diff;
    while(accumulated_clocks>FCYCLE_POSITIVE_SUM) accumulated_clocks -= fosc_freq;
    while(accumulated_clocks<FCYCLE_NEGATIVE_SUM) accumulated_clocks += fosc_freq; // Should not happen, but if GPS is evaluated to be significantly 'early' we end up here.
    accumulation_delta = utc - accumulation_start;
}

void reset_pps_stats(void)
{
    pps_seq_count = 0;
    accumulated_clocks = 0;
    accumulation_start = 0;
    accumulation_delta = 0;
}

uint32_t calculate_fosc_freq(uint32_t fosc_freq)
{
    long double new_fosc_freq_f = fosc_freq;
    uint32_t new_fosc_freq = 0;
    new_fosc_freq_f = new_fosc_freq_f * accumulation_delta;
    new_fosc_freq_f = new_fosc_freq_f + accumulated_clocks;
    new_fosc_freq_f = new_fosc_freq_f / accumulation_delta;
    new_fosc_freq = (new_fosc_freq_f + 0.5); //DIRTY ROUNDL() EQUIVALENT
    return new_fosc_freq;
}

// IC1 ISR
void __attribute__ ( ( interrupt, no_auto_psv ) ) _ISR _IC1Interrupt( void )
{	
    if(IFS0bits.IC1IF)
    {
        pps_seq_count++; // Increment our PPS counter
        ic1_val = IC1BUF; // Read the IC1 timer
        IFS0bits.IC1IF = 0; // Clear the interrupt flag
    }
}

// IC2 ISR
void __attribute__ ( ( interrupt, no_auto_psv ) ) _ISR _IC2Interrupt( void )
{	
    if(IFS0bits.IC2IF)
    {
        ic2_val = IC2BUF; // Read the IC2 timer
        IFS0bits.IC2IF = 0; // Clear the interrupt flag
    }
}