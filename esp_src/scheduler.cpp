#include "scheduler.h"

hw_timer_t *scheduler_timer = NULL;

int8_t t1ms = 0;
int8_t t1ms0 = 0;
int8_t t10ms = 0;
int8_t t10ms0 = 0;
int8_t t100ms = 0;
int8_t t100ms0 = 0;
int8_t t100ms1 = 0;
int8_t t100ms2 = 0;
int8_t t100ms3 = 0;
int8_t t1s0 = 0;

bool scheduler_sync = 0;

void scheduler_init(void)
{
  scheduler_timer = timerBegin(SCHEDULER_HW_TIMER, 80, true);
  timerAttachInterrupt(scheduler_timer, &scheduler_1ms, true);
  timerAlarmWrite(scheduler_timer, 1000, true);
  timerAlarmEnable(scheduler_timer);
  scheduler_reset();
}

bool scheduler_is_sync(void)
{
  return scheduler_sync;
}

void scheduler_unsync(void)
{
  scheduler_sync = 0;;
}

void scheduler_reset_sync(void)
{
  timerRestart(scheduler_timer);
  scheduler_reset();
  scheduler_sync = 1;
}

void IRAM_ATTR scheduler_1ms()
{
  t1ms++;
  t1ms0++;
  if(t1ms==10)
  {
    t1ms=0;
    t10ms++;
    t10ms0++;
    if(t10ms==10)
    {
      t10ms=0;
      t100ms++;
      t100ms0++;
      t100ms1++;
      t100ms2++;
      t100ms3++;
      if(t100ms==10)
      {
        t100ms=0;
        t1s0++;
      }
    }
  }
}

void scheduler_reset()
{
  t1ms = 0;
  t1ms0 = 0;
  t10ms = 0;
  t10ms0 = 0;
  t100ms = 0;
  t100ms0 = 0;
  t100ms1 = 0;
  t100ms2 = 0;
  t100ms3 = 0;
  t1s0 = 0;
}