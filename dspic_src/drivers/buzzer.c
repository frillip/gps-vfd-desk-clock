#include "buzzer.h"

#include <stddef.h>
#include <xc.h>

#include "../platform/board.h"
#include "../platform/clock.h"

// Timer5 at FCY / 8: 5000 counts a millisecond.
#define T5_PRESCALE_8 1U
#define T5_PERIOD     ((uint16_t)(FCY / 8UL / 1000UL * BUZZER_TICK_MS) - 1U)

const BuzzerPattern buzzer_button = {60U, 0U, 0U, 0U, false};
const BuzzerPattern buzzer_chime = {100U, 100U, 4U, 300U, false};
const BuzzerPattern buzzer_alarm = {100U, 100U, 4U, 500U, true};

typedef enum
{
    STEP_ON = 0,
    STEP_GAP,
    STEP_PAUSE
} Step;

// Shared with the Timer5 interrupt: changed by the foreground only with it
// held off.
static const BuzzerPattern *volatile pattern;
static volatile BuzzerPriority playing;
static volatile uint8_t count;
static volatile uint8_t done; // beeps finished this round
static volatile Step step;
static volatile uint16_t remaining; // ms left in this step
static BuzzerStats stats;

static void timer_run(bool on)
{
    IEC1bits.T5IE = 0;
    T5CONbits.TON = 0;
    IFS1bits.T5IF = 0;
    if (on)
    {
        TMR5 = 0;
        T5CONbits.TON = 1;
        IEC1bits.T5IE = 1;
    }
}


void buzzer_init(void)
{
    T5CON = 0;
    T5CONbits.TCKPS = T5_PRESCALE_8;
    PR5 = T5_PERIOD;
    IPC7bits.T5IP = BUZZER_INTERRUPT_PRIORITY;
    timer_run(false);
    UI_BUZZER_SET_LOW();
    pattern = NULL;
    playing = BUZZER_PRIORITY_NONE;
}


static void silence(void)
{
    UI_BUZZER_SET_LOW();
    playing = BUZZER_PRIORITY_NONE;
    pattern = NULL;
}


// The step after the one just ended.
static void advance(void)
{
    if (step != STEP_ON)
    {
        step = STEP_ON;
        remaining = pattern->on_ms;
        UI_BUZZER_SET_HIGH();
        return;
    }
    UI_BUZZER_SET_LOW();
    done++;
    if (done >= count)
    {
        if (!pattern->repeat)
        {
            silence();
            return;
        }
        done = 0;
        step = STEP_PAUSE;
        remaining = pattern->pause_ms;
    }
    else if (pattern->group != 0U && done % pattern->group == 0U)
    {
        step = STEP_PAUSE;
        remaining = pattern->pause_ms;
    }
    else
    {
        step = STEP_GAP;
        remaining = pattern->gap_ms;
    }
    if (remaining == 0U)
    {
        advance(); // no gap: straight to the next beep
    }
}


void __attribute__((interrupt, no_auto_psv)) _T5Interrupt(void)
{
    IFS1bits.T5IF = 0;
    if (playing == BUZZER_PRIORITY_NONE)
    {
        T5CONbits.TON = 0;
        IEC1bits.T5IE = 0;
        return;
    }
    if (remaining > BUZZER_TICK_MS)
    {
        remaining = (uint16_t)(remaining - BUZZER_TICK_MS);
        return;
    }
    advance();
}


bool buzzer_play(const BuzzerPattern *new_pattern, uint8_t new_count, BuzzerPriority priority)
{
    if (new_pattern == NULL || new_count == 0U || new_pattern->on_ms == 0U)
    {
        return false;
    }
    if (playing > priority)
    {
        stats.refused++;
        return false;
    }
    timer_run(false);
    pattern = new_pattern;
    count = new_count;
    done = 0;
    step = STEP_ON;
    remaining = new_pattern->on_ms;
    playing = priority;
    UI_BUZZER_SET_HIGH();
    stats.played++;
    timer_run(true);
    return true;
}


void buzzer_stop(BuzzerPriority priority)
{
    if (playing == BUZZER_PRIORITY_NONE || playing > priority)
    {
        return;
    }
    timer_run(false);
    silence();
    stats.stopped++;
}


BuzzerPriority buzzer_playing(void)
{
    return playing;
}


const BuzzerStats *buzzer_stats(void)
{
    return &stats;
}
