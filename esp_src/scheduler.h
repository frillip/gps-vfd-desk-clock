#include <esp32-hal-timer.h>
#define SCHEDULER_HW_TIMER 0

void scheduler_init(void);
bool scheduler_is_sync(void);
void scheduler_reset_sync(void);
void scheduler_unsync(void);
void IRAM_ATTR scheduler_1ms(void);
void scheduler_reset(void);
