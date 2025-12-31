#include "remote_tzinfo.h"

String remote_tzinfo_json_https = REMOTE_TZINFO_HTTPS_DEFAULT;
String remote_tzinfo_json_server = REMOTE_TZINFO_SERVER_DEFAULT;
String remote_tzinfo_json_path = REMOTE_TZINFO_PATH_DEFAULT;
String remote_tzinfo_json_url = remote_tzinfo_json_https + remote_tzinfo_json_server + remote_tzinfo_json_path;
bool remote_tzinfo_enabled = REMOTE_TZINFO_ENABLED_DEFAULT;
uint32_t remote_tzinfo_interval = REMOTE_TZINFO_INTERVAL_DEFAULT;
uint32_t remote_tzinfo_interval_count = REMOTE_TZINFO_INTERVAL_MAX + 1;  // Trigger at power on

int32_t remote_tzinfo_gnss_accuracy = REMOTE_TZINFO_GNSS_ACCURACY_DEFAULT;

extern uint8_t esp_pps_sync_ignore_counter; // ignore for 5 seconds

extern bool pic_gnss_detected;
extern bool pic_fix_ok;
extern int32_t pic_posllh_lat;
extern int32_t pic_posllh_lon;

char tzinfo_zone[64];
bool tzinfo_available = 0;
int32_t tzinfo_offset = 0;
int32_t tzinfo_dst_offset = 0;
time_t tzinfo_dst_next = 0;
bool tzinfo_dst_active = 0;
TZINFO_SOURCE tzinfo_source = TZINFO_SOURCE_NONE;

void remote_tzinfo_set_server(const char* new_server)
{
  remote_tzinfo_json_server = new_server;
  remote_tzinfo_regenerate_url();
}

void remote_tzinfo_set_path(const char* new_path)
{
  remote_tzinfo_json_path = new_path;
  if(remote_tzinfo_json_path.charAt(0) != '/') remote_tzinfo_json_path = String("/") + remote_tzinfo_json_path;
  remote_tzinfo_regenerate_url();
}

void remote_tzinfo_regenerate_url(void)
{
  remote_tzinfo_json_url = remote_tzinfo_json_https + remote_tzinfo_json_server + remote_tzinfo_json_path;
}

float remote_tzinfo_round_acc(int32_t value)
{
  float scaled_value = (float)value / 10000000;
  float multiplier = pow(10.0, remote_tzinfo_gnss_accuracy);
  return roundf(scaled_value * multiplier) / multiplier;
}

void remote_tzinfo_check(Stream *output)
{
  tzinfo_available = 0; // Reset available flag
  esp_pps_sync_ignore_counter = 5; // ignore for 5 seconds

  HTTPClient http;
  http.begin(remote_tzinfo_json_url);
  http.addHeader("User-Agent", ESP_USER_AGENT_STRING);

  JsonDocument json_gnss_info;
  if(remote_tzinfo_gnss_accuracy >= 0)
  {
    json_gnss_info["gnss_detected"] = pic_gnss_detected;
    json_gnss_info["gnss_lat"] = remote_tzinfo_round_acc(pic_posllh_lat);
    json_gnss_info["gnss_lon"] = remote_tzinfo_round_acc(pic_posllh_lon);
    json_gnss_info["gnss_fix_valid"] = pic_fix_ok;
  }
  else
  {
    json_gnss_info["gnss_detected"] = 0;
    json_gnss_info["gnss_lat"] = 0;
    json_gnss_info["gnss_lon"] = 0;
    json_gnss_info["gnss_fix_valid"] = 0;
  }

  String jsonString;
  serializeJson(json_gnss_info, jsonString);
  output->println(jsonString);
  int httpCode = http.POST(jsonString);

  if (httpCode > 0) {
    output->printf("HTTP code: %d\n", httpCode);

    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      output->printf("Received payload:");
      output->println(payload);

      JsonDocument tzinfo_json;

      DeserializationError error = deserializeJson(tzinfo_json, payload);
      if (error) {
        output->printf("JSON deserialization failed: ");
        output->println(error.c_str());
        return;
      }

      const char* local_tzinfo_zone = tzinfo_json["zone"];
      if(local_tzinfo_zone)
      {
        strncpy(tzinfo_zone, local_tzinfo_zone, sizeof(tzinfo_zone));
        tzinfo_zone[sizeof(tzinfo_zone) - 1] = 0x00; // null terminate
      }
      else tzinfo_zone[0] = 0x00;
      

      tzinfo_offset = tzinfo_json["offset"];
      tzinfo_dst_offset = tzinfo_json["dst_offset"];
      tzinfo_dst_next = tzinfo_json["next"];
      tzinfo_dst_active = tzinfo_json["dst"];
      tzinfo_source = tzinfo_json["source"];
      tzinfo_available = 1;

      output->printf("Timezone: %s\n", tzinfo_zone);
      output->printf("Offset: %i\nDST: ", tzinfo_offset);
      if(tzinfo_dst_active) output->printf("True\n");
      else output->printf("False\n");
      output->printf("DST offset: %i\n", tzinfo_dst_offset);
      output->printf("Next DST transition: ");
      print_iso8601_string(output, tzinfo_dst_next);
      output->printf("\n");
    }
  } else {
    output->printf("HTTP GET failed, error: %s\n", http.errorToString(httpCode).c_str());
    tzinfo_available = 0;
  }

  http.end();
}


void print_remote_tzinfo(Stream *output)
{
  output->printf("\n=== Remote tzinfo ===\n");
  output->printf("Enabled: ");
  if(remote_tzinfo_enabled)
  {
    output->printf("True\n");
    output->printf("Available: ", tzinfo_available);
    if(tzinfo_available) output->printf("True\n");
    else output->printf("False\n");
    output->printf("Timezone: %s\n", tzinfo_zone);
    output->printf("Offset: %i\nDST: ", tzinfo_offset);
    if(tzinfo_dst_active) output->printf("True\n");
    else output->printf("False\n");
    output->printf("DST offset: %i\n", tzinfo_dst_offset);
    output->printf("Next DST transition: ");
    print_iso8601_string(output, tzinfo_dst_next);
    output->printf("\nSource: ");
    print_tzinfo_source(output, tzinfo_source);
    output->printf("\n");
  }
  else output->printf("False\n");
}

void print_tzinfo_source(Stream *output, TZINFO_SOURCE source)
{
  switch(source)
  {
    case TZINFO_SOURCE_NONE:
      output->printf("NONE");
      break;
    
    case TZINFO_SOURCE_CACHE:
      output->printf("CACHE");
      break;
    
    case TZINFO_SOURCE_GEOIP:
      output->printf("GEOIP");
      break;
    
    case TZINFO_SOURCE_GNSS:
      output->printf("GNSS");
      break;
    
    default:
      output->printf("UNKNOWN");
      break;
  }
}