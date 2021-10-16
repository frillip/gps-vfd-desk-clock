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
bool scheduler_adjust_in_progress = 0;

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

void scheduler_align()
{
    TMR2_Counter16BitSet(0x9C4);
    STATUS_LED_SetHigh();
    scheduler_adjust_in_progress = 1;
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
    t1s1=0;
    scheduler_adjust_in_progress = 0;
    scheduler_sync = 1;
}