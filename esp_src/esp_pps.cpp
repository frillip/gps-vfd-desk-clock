#include "esp_pps.h"

hw_timer_t *esp_pps_timer = NULL;

bool esp_pps_sync = 0;
extern uint32_t esp_micros;
uint32_t esp_pps_micros = 0;

uint8_t esp_pps_sync_ignore_counter = 0; // Used for loooooooong operations that disrupt the loop()

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

bool esp_pps_check_sync(void)
{
  if(esp_pps_sync_ignore_counter > 0)
  {
    esp_pps_sync_ignore_counter--;
  }
  else
  {
    if(esp_pps_sync)
    {
      int32_t pps_diff = (int32_t)esp_micros - esp_pps_micros;
      while(pps_diff < -500000) pps_diff += 1000000;
      while(pps_diff > 500000) pps_diff -= 1000000;
      if((pps_diff > 10000) || (pps_diff < -10000))
      {
        esp_pps_sync = 0;
      }
    }
  }
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
  esp_pps_micros = micros();
}