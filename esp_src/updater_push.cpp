/*
#include "updater_push.h"

bool updater_push_enabled = 0;
uint16_t updater_push_port = UPDATER_PUSH_PORT_DEFAULT;
const char* updater_push_password = UPDATER_PUSH_PASSWORD_DEFAULT;

bool updater_push_running(void)
{
  return updater_push_enabled;
}

void print_updater_push_info(Stream *output)
{
  if(updater_push_enabled)
  {
    output->printf("ArduinoOTA RUNNING on %s \(%s:%s\)\n", WiFi.getHostname(), WiFi.localIP().toString().c_str(), updater_push_port);
  }
  else output->printf("ArduinoOTA NOT RUNNING\n");
}

void updater_push_disable(void)
{
  ArduinoOTA.end();
  updater_push_enabled = 0;
}

void updater_push_enable(void)
{
  extern String wifi_hostname;
  ArduinoOTA.setPort(updater_push_port);
  ArduinoOTA.setHostname(wifi_hostname.c_str());
  ArduinoOTA.setPassword(updater_push_password);
  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH) {
        type = "sketch";
      } else {  // U_SPIFFS
        type = "filesystem";
      }

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) {
        Serial.println("Auth Failed");
      } else if (error == OTA_BEGIN_ERROR) {
        Serial.println("Begin Failed");
      } else if (error == OTA_CONNECT_ERROR) {
        Serial.println("Connect Failed");
      } else if (error == OTA_RECEIVE_ERROR) {
        Serial.println("Receive Failed");
      } else if (error == OTA_END_ERROR) {
        Serial.println("End Failed");
      }
    });
  ArduinoOTA.begin();
  updater_push_enabled = 1;
}
*/