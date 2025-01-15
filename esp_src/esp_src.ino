#include <ezTime.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <esp_task_wdt.h>
#define TWDT_TIMEOUT_MS 15000

#include "enums.h"
#include "serial_proto.h"
#include "esp_pps.h"
#include "serial_console.h"
#include "pic.h"
extern time_t pic;
#include "gnss_pps_esp.h"

#include "scheduler.h"
extern int8_t t1ms;
extern int8_t t1ms0;
extern int8_t t10ms;
extern int8_t t10ms0;
extern int8_t t100ms;
extern int8_t t100ms0;
extern int8_t t100ms1;
extern int8_t t100ms2;
extern int8_t t100ms3;
extern int8_t t1s0;

WiFiManager wm;

#define CLOCK_NTP_SERVER "rubidium.darksky.io"
#define CLOCK_NTP_INTERVAL_DEFAULT 1800
uint32_t ntp_interval = CLOCK_NTP_INTERVAL_DEFAULT;
uint16_t ntp_interval_count = 0;
uint32_t ntp_resync_count = 0;
time_t esp = 0;

int32_t tz_offset = 0;
bool dst_active = 0;
int32_t dst_offset = 0;

uint32_t esp_micros = 0;
#define STATUS_LED_PIN 23

void startNTPqueries(void)
{
  updateNTP();
  ntp_resync_count++;
  ntp_interval_count = 0;
}

uint32_t wifi_disconnect_millis = 0;
uint32_t wifi_disconnect_last_millis = 0;
uint32_t wifi_disconnect_count = 0;
#define WIFI_RECONNECT_INTERVAL_MILLIS 30000
#define WIFI_RECONNECT_INTERVAL_WARN   120

void reconnect_wifi(void)
{
  if(wm.getWiFiIsSaved() && !wm.getConfigPortalActive())
  {
    if(wifi_disconnect_count == 0)
    {
      Serial.println("Reconnecting to WiFi...");
    }
    WiFi.disconnect();
    WiFi.reconnect();
  }
}

void setup()
{
  serial_console_init();

  /*
  // TO DO:
  // Fix TWDT in esp32 v3.0.x
  //
  ESP_ERROR_CHECK(esp_task_wdt_deinit());
  esp_task_wdt_config_t twdt_config = {
        .timeout_ms = TWDT_TIMEOUT_MS,
        .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,    // Bitmask of all cores
        .trigger_panic = true,
    };
  ESP_ERROR_CHECK(esp_task_wdt_init(&twdt_config));
  printf("TWDT initialized\n");
  ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
  */
  
  uint32_t id = 0;
  for(int i=0; i<17; i=i+8) {
    id |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
  }
  id &= 0xFFFFFF;
  char id_buf[6] = {0};
  sprintf(id_buf,"%06X", id);
  String wifi_id = id_buf;
  String wifi_hostname  = String("SMOL_CLOCK_" + wifi_id);
  WiFi.setHostname(wifi_hostname.c_str());
  Serial.println(wifi_hostname);

  pic_uart_init();
  gnss_uart_init();

  setServer(CLOCK_NTP_SERVER);
  setInterval(ntp_interval);

  WiFi.mode(WIFI_STA);
  wm.setConfigPortalBlocking(false);
  wm.setConfigPortalTimeout(900);
  wm.setConnectTimeout(5);
  if(wm.autoConnect(wifi_hostname.c_str()))
  {
    Serial.println(WiFi.localIP());
    startNTPqueries();
  }
  else
  {
    Serial.println("Configportal running");
    Serial.println(WiFi.localIP());
    wm.setSaveConfigCallback(startNTPqueries);
  }

  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, 0);

  pic_pps_init();
  gnss_pps_init();
  esp_pps_init();
  scheduler_init();
}

void loop()
{
  if(secondChanged())
  {
    //esp_task_wdt_reset();
    esp_micros = micros();
    if(!esp_pps_is_sync())
    {
      if(timeStatus() != timeNotSet && !UTC.ms())
      {
        esp_pps_reset_sync();
      }
    }
    if(!scheduler_is_sync())
    {
      if(timeStatus() != timeNotSet && !UTC.ms())
      {
        scheduler_reset_sync();
      }
    }
    if(timeStatus() == timeSync && WiFi.status() == WL_CONNECTED)
    {
      digitalWrite(STATUS_LED_PIN, 1);
    }
    pic_uart_tx_timedata();

    serial_console_second_changed(millis());
    
    esp = UTC.now();
  }

  if(serial_console_char_available()) serial_console_task();
  if(pic_uart_char_available()) pic_uart_rx();

  wm.process();
  events();

  if(WiFi.status() != WL_CONNECTED)
  {
    wifi_disconnect_millis = millis();
    if(wifi_disconnect_millis - wifi_disconnect_last_millis > WIFI_RECONNECT_INTERVAL_MILLIS)
    {
      wifi_disconnect_last_millis = wifi_disconnect_millis;
      reconnect_wifi();
    }
  }

  if(t1ms0)
  {
    t1ms0=0;
    if(esp_pps_out_state())
    {
      if(UTC.ms()) 
      {
        esp_pps_out_clear();
      }
    }
    if(digitalRead(STATUS_LED_PIN))
    {
      if(UTC.ms()>=100) 
      {
        digitalWrite(STATUS_LED_PIN, 0);
      }
    }
    if(serial_console_print_local_available())
    {
      serial_console_print_local();
    }
  }
  if(t10ms0)
  {
    t10ms0=0;
    if(gnss_is_detected())
    {
      gnss_timeout_incr();
    }
    if(pic_is_detected())
    {
      pic_timeout_incr();
      pic_data_task();
    }
  }
  if(t100ms0>=1)
  {
    t100ms0=-4;
  }
  if(t100ms1>=1)
  {
    t100ms1=-9;
    //pic_uart_tx_displaydata();
  }
  if(t100ms2>=1)
  {
    t100ms2=-9;
    pic_uart_tx_netdata();
  }
  if(t100ms3>=87)
  {
    t100ms3=-13;
    //pic_uart_tx_rtcdata();
  }
  if(t1s0)
  {
    t1s0=0;
    ntp_interval_count++;
    if(ntp_interval_count>ntp_interval)
    {
      updateNTP();
      Serial.printf("NTP RESYNC\n");
      ntp_resync_count++;
      ntp_interval_count = 0;
      scheduler_unsync();
      esp_pps_unsync();
    }
  }
}
