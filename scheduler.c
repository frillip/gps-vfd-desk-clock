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

void scheduler_run(void)
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
}


// Sync the scheduler when called
void scheduler_align()
{
    TMR2_Counter16BitSet(0x9C4); // Set the timer counter to expire in 500us
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