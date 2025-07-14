#include <ezTime.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <esp_task_wdt.h>
#define TWDT_TIMEOUT_MS 15000

#include "enums.h"
#include "serial_proto.h"
#include "esp_pps.h"
#include "serial_console.h"
#include "user_uart.h"
#include "telnet.h"
extern ESPTelnetStream telnet;
#include "pic.h"
#include "pic_bootloader.h"
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

#include "updater.h"

WiFiManager wm;

#define CLOCK_NTP_SERVER_DEFAULT "rubidium.darksky.io"
#define CLOCK_NTP_INTERVAL_DEFAULT 1800
String ntp_server = CLOCK_NTP_SERVER_DEFAULT;
uint32_t ntp_interval = CLOCK_NTP_INTERVAL_DEFAULT;
uint16_t ntp_interval_count = 0;
uint32_t ntp_resync_count = 0;
time_t esp = 0;

int32_t tz_offset = 0;
bool dst_active = 0;
int32_t dst_offset = 0;

uint32_t esp_micros = 0;
#define STATUS_LED_PIN 23

String wifi_hostname;
String wifi_id;

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
  user_uart_init();

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
  wifi_id = id_buf;
  wifi_hostname  = String("SMOL_CLOCK_" + wifi_id);
  WiFi.setHostname(wifi_hostname.c_str());
  Serial.println(wifi_hostname);

  pic_uart_init();
  gnss_uart_init();

  setServer(ntp_server);
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

  telnet_init();

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
    esp = UTC.now();
    if(timeStatus() != timeNotSet)
    {
      if(!esp_pps_check_sync())
      {
        esp_pps_reset_sync();
      }
      if(!scheduler_is_sync())
      {
        scheduler_reset_sync();
      }
    }
    if(timeStatus() == timeSync && WiFi.status() == WL_CONNECTED)
    {
      digitalWrite(STATUS_LED_PIN, 1);
    }
    pic_uart_tx_timedata();

    //serial_console_second_changed(millis());
  }

  if(user_uart_char_available()) user_uart_task();
  if(pic_uart_char_available()) pic_uart_rx();

  //if(updater_push_running()) ArduinoOTA.handle();
  wm.process();
  events();
  telnet.loop();
  if(telnet_char_available()) telnet_console_task();

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
      if(micros() - esp_micros > 1000) 
      {
        esp_pps_out_clear();
      }
    }
    if(digitalRead(STATUS_LED_PIN))
    {
      if(micros() - esp_micros > 100000)
      {
        digitalWrite(STATUS_LED_PIN, 0);
      }
    }
    //if(serial_console_print_local_available())
    //{
    //  serial_console_print_local();
    //}
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
  if(t100ms0>=2)
  {
    t100ms0=0;
    if(!pic_output_buffer_empty() && pic_uart_output_finished())
    {
      pic_flush_output_buffer();
    }
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
    pic_uart_tx_tzinfodata();
  }
  if(t1s0)
  {
    t1s0=0;

    extern bool remote_tzinfo_enabled;
    if(remote_tzinfo_enabled)
    {
      extern uint32_t remote_tzinfo_interval_count;
      extern uint32_t remote_tzinfo_interval;
      remote_tzinfo_interval_count++;
      if(remote_tzinfo_interval_count > remote_tzinfo_interval)
      {
        remote_tzinfo_check(&Serial);
        remote_tzinfo_interval_count = 0;
      }
    }

    ntp_interval_count++;
    if(ntp_interval_count > ntp_interval)
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
