#include <esp32-hal-timer.h>

#define ESP_PPS_HW_TIMER 1
#define ESP_PPS_OUT_PIN 5

#define esp_pps_out_set() digitalWrite(ESP_PPS_OUT_PIN, 1)
#define esp_pps_out_clear() digitalWrite(ESP_PPS_OUT_PIN, 0)
#define esp_pps_out_state() digitalRead(ESP_PPS_OUT_PIN)

void esp_pps_init(void);
bool esp_pps_is_sync(void);
void esp_pps_reset_sync(void);
void esp_pps_unsync(void);
void esp_pps_out_intr(void);