#include "updater_pull.h"

extern USER_PREFS_DATA_STRUCT user_prefs;

static constexpr char UPDATER_JSON_URL_DEFAULT[] = UPDATER_JSON_HTTPS_DEFAULT UPDATER_JSON_SERVER_DEFAULT UPDATER_JSON_PATH_DEFAULT;
String updater_json_url = UPDATER_JSON_URL_DEFAULT;

// Every 30 minutes, check if we are in the update window
// also sufficiently spreads checks out over the hour as reboot time is suitably random
// Could be refined, but this works for now
uint16_t updater_auto_check_interval = UPDATE_AUTO_CHECK_INTERVAL_DEFAULT;
bool updater_auto_checked_today = 1;    // Start as 1, so we don't immediately check for updates on boot, ie, after an update.

ESP32OTAPull ota;
extern Stream *last_output_stream;
Stream *output_callback;

extern uint8_t esp_pps_sync_ignore_counter;

const char root_ca_progmem[] PROGMEM = R"EOF(
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

// Function to use PROGMEM certificate
String getPROGMEMCertificate() {
    return String(root_ca_progmem);
}

void updater_set_server(const char* new_server)
{
  memset(user_prefs.fields.updater.server, 0, sizeof(user_prefs.fields.updater.server));
  strlcpy(user_prefs.fields.updater.server, new_server, sizeof(user_prefs.fields.updater.server));
  updater_regenerate_url();
}

void updater_set_path(const char* new_path)
{
  String updater_json_path = new_path;
  if(updater_json_path.charAt(0) != '/') updater_json_path = String("/") + updater_json_path;
  memset(user_prefs.fields.updater.path, 0, sizeof(user_prefs.fields.updater.path));
  strlcpy(user_prefs.fields.updater.path, updater_json_path.c_str(), sizeof(user_prefs.fields.updater.path));
  updater_regenerate_url();
}

void updater_set_config(const char* new_config)
{
  memset(user_prefs.fields.updater.config, 0, sizeof(user_prefs.fields.updater.config));
  if(!new_config)
  {
    return;
  }

  if((strcmp(new_config, "default") == 0))
  {
    strlcpy(user_prefs.fields.updater.config, UPDATER_CONFIG_STRING_DEFAULT, sizeof(user_prefs.fields.updater.config));
  }
  else if(new_config[0] != '\0')
  {
    strlcpy(user_prefs.fields.updater.config, new_config, sizeof(user_prefs.fields.updater.config));
  }
}

void updater_regenerate_url(void)
{
  updater_json_url = String(UPDATER_JSON_HTTPS_DEFAULT) + String(user_prefs.fields.updater.server) + String(user_prefs.fields.updater.path);
}

void updater_auto_check(void)
{
  if(user_prefs.fields.updater.flags.auto_enabled)
  {
    extern int32_t tz_offset;
    extern bool dst_active;
    extern int32_t dst_offset;
    extern time_t esp;
    time_t local = esp;
    local += tz_offset;
    if(dst_active) local += dst_offset;
    struct tm *local_tm; // Allocate buffer
    local_tm = gmtime(&local);

    uint8_t updater_auto_check_reset_time = 23; // Initialise as 23:00
    if(user_prefs.fields.updater.flags.auto_time)  // If this is a value after midnight
    {
      updater_auto_check_reset_time = user_prefs.fields.updater.flags.auto_time - 1; // modify accordingly
    }

    if(local_tm->tm_hour == updater_auto_check_reset_time)
    {
      updater_auto_checked_today = 0;
    }

    if(updater_auto_checked_today == 1)
    {
      return;
    }
    else
    {
      if(local_tm->tm_hour == user_prefs.fields.updater.flags.auto_time)
      {
        last_output_stream->printf("Checking for update...\n");
        updater_auto();
      }
      return;
    }
  }
  return;
}

bool updater_auto(void)
{
  esp_pps_sync_ignore_counter = 5; // ignore for 5 seconds
  last_output_stream->printf("ESP Current Version:   %s", ESP_VERSION);
  if(user_prefs.fields.updater.config[0]) // not empty
  {
    last_output_stream->printf("_%s", user_prefs.fields.updater.config);
  }
  last_output_stream->printf("\n");
  ota.SetRootCA(root_ca_progmem);
  int ret = ota
    .SetConfig(user_prefs.fields.updater.config)
    .CheckForOTAUpdate(updater_json_url.c_str(), ESP_VERSION, ESP32OTAPull::DONT_DO_UPDATE);
  if(ret == ESP32OTAPull::UPDATE_AVAILABLE)
  {
    last_output_stream->printf("Update available");
    if(user_prefs.fields.updater.config[0])
    {
      last_output_stream->printf("Config string: %s\n", user_prefs.fields.updater.config);
    }
    last_output_stream->printf("... Updating\n\n");
    output_callback = last_output_stream;
    ret = ota
      .SetCallback(updater_callback_percent)
      .SetConfig(user_prefs.fields.updater.config)
      .CheckForOTAUpdate(updater_json_url.c_str(), ESP_VERSION, ESP32OTAPull::UPDATE_AND_BOOT);
    last_output_stream->printf("Rebooting...\n");
    return 1;    
  }
  last_output_stream->printf("No update found\n");
  updater_auto_checked_today = 1;
  return 0;
}

bool updater_check(Stream *output)
{
  esp_pps_sync_ignore_counter = 5; // ignore for 5 seconds
  output->printf("ESP Current Version:   %s", ESP_VERSION);
  if(user_prefs.fields.updater.config[0])
  {
    output->printf("_%s", user_prefs.fields.updater.config);
  }
  output->printf("\n");
  ota.SetRootCA(root_ca_progmem);
  int ret = ota
    .SetConfig(user_prefs.fields.updater.config)
    .CheckForOTAUpdate(updater_json_url.c_str(), ESP_VERSION, ESP32OTAPull::DONT_DO_UPDATE);
  output->printf("CheckForOTAUpdate returned %d (%s)\n", ret, updater_errtext(ret));
  String otaVersion = ota.GetVersion();
  output->printf("OTA Version Available: %s", otaVersion.c_str());
  if(user_prefs.fields.updater.config[0])
  {
    output->printf("_%s", user_prefs.fields.updater.config);
  }
  output->printf("\n");
  return 0;
}

void updater_pull(Stream *output)
{
  esp_pps_sync_ignore_counter = 5; // ignore for 5 seconds
  output->printf("ESP Current Version:   %s", ESP_VERSION);
  if(!user_prefs.fields.updater.config[0])
  {
    output->printf("_%s", user_prefs.fields.updater.config);
  }
  output->printf("\n");
  output_callback = output;
  ota.SetRootCA(root_ca_progmem);
  int ret = ota
    .SetCallback(updater_callback_percent)
    .SetConfig(user_prefs.fields.updater.config)
    .CheckForOTAUpdate(updater_json_url.c_str(), ESP_VERSION, ESP32OTAPull::UPDATE_BUT_NO_BOOT);
  output->printf("CheckForOTAUpdate returned %d (%s)\n", ret, updater_errtext(ret));
  if(ret == ESP32OTAPull::UPDATE_OK)
  {
    output->printf("Use esp-reset to finish update.\n");
  }
}

void updater_force(Stream *output)
{
  esp_pps_sync_ignore_counter = 5; // ignore for 5 seconds
  output->printf("ESP Current Version:   %s", ESP_VERSION);
  if(user_prefs.fields.updater.config[0])
  {
    output->printf("_%s", user_prefs.fields.updater.config);
  }
  output->printf("\n");
  output_callback = output;
  ota.SetRootCA(root_ca_progmem);
  int ret = ota
    .SetCallback(updater_callback_percent)
    .AllowDowngrades(true)
    .CheckForOTAUpdate(updater_json_url.c_str(), "0.0.0", ESP32OTAPull::UPDATE_AND_BOOT);
  output->printf("CheckForOTAUpdate returned %d (%s)\n", ret, updater_errtext(ret));
  output->printf("Rebooting...\n");
}

void updater_callback_percent(int offset, int totallength)
{
  static int prev_percent = -1;
  int percent = 100 * offset / totallength;
  if (percent != prev_percent)
  {
    output_callback->printf("Updating %d of %d (%02d%%)...\n", offset, totallength, 100 * offset / totallength);
    prev_percent = percent;
  }
}

const char *updater_errtext(int code)
{
  switch(code)
  {
    case ESP32OTAPull::UPDATE_AVAILABLE:
      return "An update is available but wasn't installed";
    case ESP32OTAPull::NO_UPDATE_PROFILE_FOUND:
      return "No profile matches";
    case ESP32OTAPull::NO_UPDATE_AVAILABLE:
      return "Profile matched, but update not applicable";
    case ESP32OTAPull::UPDATE_OK:
      return "An update was done, but no reboot";
    case ESP32OTAPull::HTTP_FAILED:
      return "HTTP GET failure";
    case ESP32OTAPull::WRITE_ERROR:
      return "Write error"; 
    case ESP32OTAPull::JSON_PROBLEM:
      return "Invalid JSON";
    case ESP32OTAPull::OTA_UPDATE_FAIL:
      return "Update fail (no OTA partition?)";
    default:
      if (code > 0)
        return "Unexpected HTTP response code";
      break;
  }
  return "Unknown error";
}