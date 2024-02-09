#include "scheduler.h"

uint8_t t1ms=0;
uint8_t t1ms0=0;
uint8_t t1ms1=0;
uint8_t t10ms=0;
uint8_t t10ms0=0;
uint8_t t10ms1=0;
uint8_t t100ms=0;
uint8_t t100ms0=0;
uint8_t t100ms1=0;
uint8_t t1s0=0;
uint8_t t1s1=0;

bool scheduler_sync = 0;
extern bool print_data;

void scheduler_init(void)
{
    //TMR2 0; 
    TMR2 = 0x00;
    //Period = 0.001 s; Frequency = 40000000 Hz; PR2 4999; 
    PR2 = 0x1387;
    //TCKPS 1:8; T32 16 Bit; TON enabled; TSIDL disabled; TCS FOSC/2; TGATE disabled; 
    T2CON = 0x8010;
    // Clear T2 interrupt flag
    IFS0bits.T2IF = false;
    // Enable T2 interrupts
    IEC0bits.T2IE = true;
    // Set interrupt priority
    IPC1bits.T2IP = 1;
}


// Sync the scheduler when called
void scheduler_align(uint32_t fosc)
{
    uint16_t new_tmr2_pr = (((fosc + SCHEDULER_PRECISION)>>3)/1000);
    PR2 = new_tmr2_pr-1; // Readjust our scheduler timer
    TMR2 = new_tmr2_pr/2; // Set the timer counter to expire in 500us
    STATUS_LED_SetHigh(); // Drive the LED high for no reason
    t1ms=0;
    t1ms0=0;
    t1ms1=0;
    t10ms=0;
    t10ms0=0;
    t10ms1=0;
    t100ms=0;
    t100ms0=0;
    t100ms1=0;
    t1s0=0;
    t1s1=0; // Reset the scheduler variables
    scheduler_sync = 1; // Say that we're done
    print_data = 1;
}

void __attribute__ ( ( interrupt, no_auto_psv ) ) _T2Interrupt (  )
{
    t1ms++;
    t1ms0++;
    t1ms1++;
    if(t1ms==10)
    {
        t1ms=0;
        t10ms++;
        t10ms0++;
        t10ms1++;
        if(t10ms==10)
        {
            t10ms=0;
            t100ms++;
            t100ms0++;
            t100ms1++;
            if(t100ms==10)
            {
                t100ms=0;
                t1s0++;
                t1s1++;
            }
        }
    }
    IFS0bits.T2IF = false;
}