#include "pps.h"

#include <Arduino.h>
#include <driver/gptimer.h>
#include <esp_timer.h>
#include <soc/gpio_reg.h>

#include "../board.h"

#define SECOND_US   1000000LL
#define MIN_LEAD_US 2000LL // an alarm closer than this is too late to arm

typedef struct
{
    portMUX_TYPE lock;
    volatile int64_t local_us;
    volatile uint32_t sequence;
    uint32_t taken;
} EdgeSlot;

static gptimer_handle_t timer;
static int64_t count_offset_us; // local time - timer count
static portMUX_TYPE output_lock = portMUX_INITIALIZER_UNLOCKED;
static bool running;
static bool high;
static uint64_t last_rise_count;
static bool plan_fresh;
static uint64_t plan_rise_count;
static int32_t period_adjust_q16;
static int32_t adjust_remainder_q16;
static PpsOutputStats output_stats;

static EdgeSlot output_edge = {portMUX_INITIALIZER_UNLOCKED, 0, 0, 0};
static EdgeSlot pic_edge = {portMUX_INITIALIZER_UNLOCKED, 0, 0, 0};
static EdgeSlot gnss_edge = {portMUX_INITIALIZER_UNLOCKED, 0, 0, 0};

static void IRAM_ATTR record(EdgeSlot *slot, int64_t local_us)
{
    portENTER_CRITICAL_ISR(&slot->lock);
    slot->local_us = local_us;
    slot->sequence = slot->sequence + 1;
    portEXIT_CRITICAL_ISR(&slot->lock);
}


static bool take(EdgeSlot *slot, PpsEdge *edge, bool only_new)
{
    portENTER_CRITICAL(&slot->lock);
    edge->local_us = slot->local_us;
    edge->sequence = slot->sequence;
    portEXIT_CRITICAL(&slot->lock);
    if (edge->sequence == 0)
    {
        return false;
    }
    if (only_new)
    {
        if (edge->sequence == slot->taken)
        {
            return false;
        }
        slot->taken = edge->sequence;
    }
    return true;
}


static void IRAM_ATTR arm(uint64_t count)
{
    gptimer_alarm_config_t alarm = {};

    alarm.alarm_count = count;
    gptimer_set_alarm_action(timer, &alarm);
}


// The next rising edge after a falling one at now_count.
static uint64_t IRAM_ATTR next_rise(uint64_t now_count)
{
    uint64_t rise;

    if (plan_fresh)
    {
        rise = plan_rise_count;
        plan_fresh = false;
        output_stats.planned++;
    }
    else
    {
        int32_t whole;

        adjust_remainder_q16 += period_adjust_q16;
        whole = adjust_remainder_q16 >> 16;
        adjust_remainder_q16 -= whole * 65536;
        rise = last_rise_count + SECOND_US + whole;
        output_stats.extrapolated++;
    }
    while (rise < now_count + MIN_LEAD_US)
    {
        rise += SECOND_US;
        output_stats.late_plans++;
    }
    return rise;
}


static bool IRAM_ATTR on_alarm(gptimer_handle_t handle, const gptimer_alarm_event_data_t *event,
                               void *context)
{
    (void)handle;
    (void)context;
    if (!high)
    {
        REG_WRITE(GPIO_OUT_W1TS_REG, 1UL << BOARD_ESP_PPS_PIN);
        int64_t now = esp_timer_get_time();
        int32_t latency = (int32_t)(now - (int64_t)event->alarm_value - count_offset_us);

        record(&output_edge, now);
        high = true;
        last_rise_count = event->alarm_value;
        output_stats.latency_us = latency;
        if (latency > output_stats.latency_max_us)
        {
            output_stats.latency_max_us = latency;
        }
        arm(event->alarm_value + PPS_PULSE_US);
    }
    else
    {
        REG_WRITE(GPIO_OUT_W1TC_REG, 1UL << BOARD_ESP_PPS_PIN);
        high = false;
        portENTER_CRITICAL_ISR(&output_lock);
        arm(next_rise(event->count_value));
        portEXIT_CRITICAL_ISR(&output_lock);
    }
    return false;
}


static void IRAM_ATTR on_pic_epoch(void)
{
    record(&pic_edge, esp_timer_get_time());
}


static void IRAM_ATTR on_gnss_pps(void)
{
    record(&gnss_edge, esp_timer_get_time());
}


// local - count, from reads either side of the count.
static void measure_offset(void)
{
    uint64_t count;
    int64_t before;
    int64_t after;

    portENTER_CRITICAL(&output_lock);
    before = esp_timer_get_time();
    gptimer_get_raw_count(timer, &count);
    after = esp_timer_get_time();
    portEXIT_CRITICAL(&output_lock);
    count_offset_us = before + (after - before) / 2 - (int64_t)count;
}


void pps_init(void)
{
    gptimer_config_t config = {};
    gptimer_event_callbacks_t callbacks = {};

    pinMode(BOARD_ESP_PPS_PIN, OUTPUT);
    digitalWrite(BOARD_ESP_PPS_PIN, LOW);
    pinMode(BOARD_PIC_EPOCH_PIN, INPUT);
    attachInterrupt(BOARD_PIC_EPOCH_PIN, on_pic_epoch, RISING);
    pinMode(BOARD_GNSS_PPS_PIN, INPUT);
    attachInterrupt(BOARD_GNSS_PPS_PIN, on_gnss_pps, RISING);

    config.clk_src = GPTIMER_CLK_SRC_DEFAULT;
    config.direction = GPTIMER_COUNT_UP;
    config.resolution_hz = 1000000;
    ESP_ERROR_CHECK(gptimer_new_timer(&config, &timer));
    callbacks.on_alarm = on_alarm;
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(timer, &callbacks, NULL));
    ESP_ERROR_CHECK(gptimer_enable(timer));
    ESP_ERROR_CHECK(gptimer_start(timer));
    measure_offset();
}


void pps_output_start(int64_t first_rise_local_us, int32_t adjust_q16)
{
    uint64_t now;
    uint64_t rise = (uint64_t)(first_rise_local_us - count_offset_us);

    if (running)
    {
        pps_output_plan(first_rise_local_us, adjust_q16);
        return;
    }
    gptimer_get_raw_count(timer, &now);
    while (rise < now + MIN_LEAD_US)
    {
        rise += SECOND_US;
    }
    portENTER_CRITICAL(&output_lock);
    period_adjust_q16 = adjust_q16;
    adjust_remainder_q16 = 0;
    plan_fresh = false;
    high = false;
    running = true;
    REG_WRITE(GPIO_OUT_W1TC_REG, 1UL << BOARD_ESP_PPS_PIN);
    arm(rise);
    portEXIT_CRITICAL(&output_lock);
}


void pps_output_plan(int64_t next_rise_local_us, int32_t adjust_q16)
{
    portENTER_CRITICAL(&output_lock);
    plan_rise_count = (uint64_t)(next_rise_local_us - count_offset_us);
    plan_fresh = true;
    period_adjust_q16 = adjust_q16;
    portEXIT_CRITICAL(&output_lock);
}


void pps_output_stop(void)
{
    portENTER_CRITICAL(&output_lock);
    gptimer_set_alarm_action(timer, NULL);
    running = false;
    high = false;
    REG_WRITE(GPIO_OUT_W1TC_REG, 1UL << BOARD_ESP_PPS_PIN);
    portEXIT_CRITICAL(&output_lock);
}


bool pps_output_running(void)
{
    return running;
}


bool pps_output_take(PpsEdge *edge)
{
    return take(&output_edge, edge, true);
}


const PpsOutputStats *pps_output_stats(void)
{
    return &output_stats;
}


bool pps_pic_take(PpsEdge *edge)
{
    return take(&pic_edge, edge, true);
}


bool pps_pic_latest(PpsEdge *edge)
{
    return take(&pic_edge, edge, false);
}


bool pps_gnss_take(PpsEdge *edge)
{
    return take(&gnss_edge, edge, true);
}


bool pps_gnss_latest(PpsEdge *edge)
{
    return take(&gnss_edge, edge, false);
}
