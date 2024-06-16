#include "esp_pps.h"

hw_timer_t *esp_pps_timer = NULL;

bool esp_pps_sync = 0;

void esp_pps_init(void)
{
  pinMode(ESP_PPS_OUT_PIN, OUTPUT);
  esp_pps_out_clear();

  esp_pps_timer = timerBegin(ESP_PPS_HW_TIMER, 80, true);
  timerAttachInterrupt(esp_pps_timer, &esp_pps_out_intr, true);
  timerAlarmWrite(esp_pps_timer, 1000000, true);
  timerAlarmEnable(esp_pps_timer);
}

bool esp_pps_is_sync(void)
{
  return esp_pps_sync;
}

void esp_pps_reset_sync(void)
{
  timerRestart(esp_pps_timer);
  esp_pps_out_set();
  esp_pps_sync = 1;
}

void esp_pps_unsync(void)
{
  esp_pps_sync = 0;
}

void esp_pps_out_intr(void)
{
  esp_pps_out_set();
}