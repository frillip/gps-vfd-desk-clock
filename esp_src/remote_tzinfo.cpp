#include "remote_tzinfo.h"

extern USER_PREFS_DATA_STRUCT user_prefs;

static constexpr char REMOTE_TZINFO_URL_DEFAULT[] = REMOTE_TZINFO_HTTPS_DEFAULT REMOTE_TZINFO_SERVER_DEFAULT REMOTE_TZINFO_PATH_DEFAULT;
String remote_tzinfo_json_url = REMOTE_TZINFO_URL_DEFAULT;
uint32_t remote_tzinfo_interval_count = REMOTE_TZINFO_INTERVAL_MAX + 1;  // Trigger at power on

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

const char tzinfo_root_ca_progmem[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc
h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+
0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U
A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW
T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH
B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC
B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv
KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn
OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn
jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw
qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI
rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV
HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK
NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5
ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur
TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC
jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc
oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq
4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA
mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d
emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=
-----END CERTIFICATE-----
)EOF";

void remote_tzinfo_set_server(const char* new_server)
{
  memset(user_prefs.fields.rtzinfo.server, 0, sizeof(user_prefs.fields.rtzinfo.server));
  strlcpy(user_prefs.fields.rtzinfo.server, new_server, sizeof(user_prefs.fields.rtzinfo.server));
  remote_tzinfo_regenerate_url();
}

void remote_tzinfo_set_path(const char* new_path)
{
  String remote_tzinfo_json_path = new_path;
  if(remote_tzinfo_json_path.charAt(0) != '/') remote_tzinfo_json_path = String("/") + remote_tzinfo_json_path;
  memset(user_prefs.fields.rtzinfo.path, 0, sizeof(user_prefs.fields.rtzinfo.path));
  strlcpy(user_prefs.fields.rtzinfo.path, remote_tzinfo_json_path.c_str(), sizeof(user_prefs.fields.rtzinfo.path));
  remote_tzinfo_regenerate_url();
}

void remote_tzinfo_regenerate_url(void)
{
  remote_tzinfo_json_url = String(REMOTE_TZINFO_HTTPS_DEFAULT) + String(user_prefs.fields.rtzinfo.server) + String(user_prefs.fields.rtzinfo.path);
}

float remote_tzinfo_round_acc(int32_t value)
{
  float scaled_value = (float)value / 10000000;
  float multiplier = pow(10.0, user_prefs.fields.rtzinfo.gnss_accuracy);
  return roundf(scaled_value * multiplier) / multiplier;
}

void remote_tzinfo_check(Stream *output)
{
  tzinfo_available = 0; // Reset available flag
  esp_pps_sync_ignore_counter = 5; // ignore for 5 seconds

  HTTPClient http;
  bool UseHTTPS = ( strncmp(remote_tzinfo_json_url.c_str(), "https://", 8) == 0 );

  if(UseHTTPS)
  {
    WiFiClientSecure* secureClient = new WiFiClientSecure();
    if (tzinfo_root_ca_progmem != NULL)
    {
      secureClient->setCACert(tzinfo_root_ca_progmem);
    }
    else
    {
      secureClient->setInsecure();
    }
    http.begin(*secureClient, remote_tzinfo_json_url);
  }
  else
  {
    http.begin(remote_tzinfo_json_url);
  }
  
  http.useHTTP10(true);
  http.addHeader("User-Agent", ESP_USER_AGENT_STRING);

  JsonDocument json_gnss_info;
  if(user_prefs.fields.rtzinfo.gnss_accuracy >= 0)
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
  if(user_prefs.fields.rtzinfo.flags.enabled)
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