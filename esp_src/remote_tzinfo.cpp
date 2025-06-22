#include "remote_tzinfo.h"

String remote_tzinfo_json_https = REMOTE_TZINFO_HTTPS_DEFAULT;
String remote_tzinfo_json_server = REMOTE_TZINFO_SERVER_DEFAULT;
String remote_tzinfo_json_path = REMOTE_TZINFO_PATH_DEFAULT;
String remote_tzinfo_json_url = remote_tzinfo_json_https + remote_tzinfo_json_server + remote_tzinfo_json_path;
uint32_t remote_tzinfo_gnss_accuracy = REMOTE_TZINFO_GNSS_ACCURACY_DEFAULT;

extern bool pic_gnss_detected;
extern bool pic_fix_ok;
extern int32_t pic_posllh_lat;
extern int32_t pic_posllh_lon;

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
  HTTPClient http;
  http.begin(remote_tzinfo_json_url);
  http.addHeader("User-Agent", ESP_USER_AGENT_STRING);

  StaticJsonDocument<200> json_gnss_info;
  if(remote_tzinfo_gnss_accuracy > 0)
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

      StaticJsonDocument<512> tzinfo_json;

      DeserializationError error = deserializeJson(tzinfo_json, payload);
      if (error) {
        output->printf("JSON deserialization failed: ");
        output->println(error.c_str());
        return;
      }

      // Access JSON fields (example)
      const char* tzinfo_zone = tzinfo_json["zone"];
      time_t tzinfo_next = tzinfo_json["next"];
      bool tzinfo_dst = tzinfo_json["dst"];
      uint32_t tzinfo_offset = tzinfo_json["offset"];
      uint32_t tzinfo_dst_offset = tzinfo_json["dst_offset"];

      output->printf("Timezone: %s\n", tzinfo_zone);
      output->printf("Offset: %u\nDST: ", tzinfo_offset);
      if(tzinfo_dst) output->printf("True\n");
      else output->printf("False\n");
      output->printf("DST offset: %u\n", tzinfo_dst_offset);
      output->printf("Next DST transition: ");
      print_iso8601_string(output, tzinfo_next);
      output->printf("\n");
    }
  } else {
    output->printf("HTTP GET failed, error: %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();
}


bool remote_tzinfo_get_json(void)
{
 return 0;
}


bool remote_tzinfo_process(void)
{
  return 0;
}