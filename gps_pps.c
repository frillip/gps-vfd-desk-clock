#include "gps_pps.h"

uint16_t ic1_val = 0;
uint16_t ic1_val_old = 0;
uint16_t ic2_val = 0;
uint16_t ic2_val_old = 0;
uint32_t pps_count = 0;
uint32_t pps_count_diff = 0;
uint32_t pps_count_old = 0;
uint16_t pps_seq_count = 0;

void gps_pps_init(void)
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