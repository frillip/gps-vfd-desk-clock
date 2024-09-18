#include "esp_pps.h"

hw_timer_t *esp_pps_timer = NULL;

bool esp_pps_sync = 0;

void esp_pps_init(void)
{
  pinMode(ESP_PPS_OUT_PIN, OUTPUT);
  esp_pps_out_clear();

  esp_pps_timer = timerBegin(1000000);
  timerAttachInterrupt(esp_pps_timer, &esp_pps_out_intr);
  timerAlarm(esp_pps_timer, 1000000, true, 0);
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

void ARDUINO_ISR_ATTR esp_pps_out_intr(void)
{
  esp_pps_out_set();
}