#include "user_prefs.h"

Preferences user_pref_store;

USER_PREFS_DATA_STRUCT user_prefs = { .raw = {0} };

extern Stream *last_output_stream;
        
void user_prefs_init(void)
{
  user_prefs_load();
  user_prefs_print(last_output_stream);
  if(!user_prefs_validate())
  {
    user_prefs_reset();
    last_output_stream->printf("User preferences reset!\n");
    user_prefs_print(last_output_stream);
    user_prefs_save();
  }
  else
  {
    user_prefs_apply();
  }
}


bool user_prefs_validate(void)
{
  bool check_passed = 1;

  check_passed &= (user_prefs.fields.header == USER_PREFS_HEADER_VALUE);
  if(!check_passed)
  {
    last_output_stream->printf("Invalid EEPROM header: %08lX\n", user_prefs.fields.header);
    return 0;
  }

  // Entire uint16_t range is fine for telnet port, besides port 0
  check_passed &= ( user_prefs.fields.telnet.port > 0 );

  // cannot validate user_prefs.fields.ntp.server until network connects
  // To do: check after succssful WiFi connection?
  // How to tell the difference between bad ntp server string
  // and non-functional network? Maybe leave it for now.
  // V3 will have an RGB status LED that can indicate this

  // Check NTP interval is sane and between limits
  check_passed &= ( user_prefs.fields.ntp.interval <= CLOCK_NTP_INTERVAL_MAX );
  check_passed &= ( user_prefs.fields.ntp.interval >= CLOCK_NTP_INTERVAL_MIN );

  // Same problem as NTP server for remote_tzinfo server and path
  // Check remote_tzinfo interval is sane and between limits
  check_passed &= ( user_prefs.fields.rtzinfo.interval <= REMOTE_TZINFO_INTERVAL_MAX );
  check_passed &= ( user_prefs.fields.rtzinfo.interval >= REMOTE_TZINFO_INTERVAL_MIN );
  // Check remote_tzinfo gnss precision is sane and between limits
  check_passed &= ( user_prefs.fields.rtzinfo.gnss_accuracy <= REMOTE_TZINFO_GNSS_ACCURACY_MAX );
  check_passed &= ( user_prefs.fields.rtzinfo.gnss_accuracy >= REMOTE_TZINFO_GNSS_ACCURACY_MIN );

  // Same problem as NTP server for update server and path
  // Check auto update time is sane and between limits
  check_passed &= ( user_prefs.fields.updater.flags.auto_time <= UPDATE_AUTO_CHECK_LOCAL_HOUR_MAX );
  check_passed &= ( user_prefs.fields.updater.flags.auto_time >= UPDATE_AUTO_CHECK_LOCAL_HOUR_MIN );
  
  return check_passed;
}


void user_prefs_apply(void)
{
  extern bool telnet_enabled; // esp-telnet-enable // esp-telnet-disable // TELNET_ENABLED_DEFAULT 1
  extern uint16_t telnet_port; // esp-telnet-port // TELNET_PORT_DEFAULT 23

  extern String ntp_server; // #define CLOCK_NTP_SERVER_DEFAULT "rubidium.darksky.io" // esp-ntp-set-interval
  uint32_t ntp_interval; // #define CLOCK_NTP_INTERVAL_DEFAULT 1800 // esp-ntp-set-server

  extern bool remote_tzinfo_enabled; // #define REMOTE_TZINFO_ENABLED_DEFAULT 1 // esp-tzinfo-enable
  extern String remote_tzinfo_json_server; // #define REMOTE_TZINFO_SERVER_DEFAULT "rubidium.darksky.io" // esp-tzinfo-set-server // remote_tzinfo_regenerate_url()
  extern String remote_tzinfo_json_path; // #define REMOTE_TZINFO_PATH_DEFAULT   "/smol_clock/v2/tzinfo" // esp-tzinfo-set-path // remote_tzinfo_regenerate_url()
  extern uint32_t remote_tzinfo_interval; // #define REMOTE_TZINFO_INTERVAL_DEFAULT 900 // esp-tzinfo-set-interval
  extern int32_t remote_tzinfo_gnss_accuracy; // #define REMOTE_TZINFO_GNSS_ACCURACY_DEFAULT 1 // esp-tzinfo-set-acc

  extern String updater_json_server; // esp-update-set-server // updater_regenerate_url()
  extern String updater_json_path; // esp-update-set-path // updater_regenerate_url()
  extern String updater_config_string; // #define UPDATER_CONFIG_STRING_DEFAULT "" // esp-update-set-config
  extern bool updater_auto_enabled; // #define UPDATE_AUTO_ENABLED_DEFAULT 0 // esp-update-auto-enable
  extern uint8_t updater_auto_check_local_hour; // #define UPDATE_AUTO_CHECK_LOCAL_HOUR_DEFAULT 4 // esp-update-auto-time

  if(user_prefs.fields.header == USER_PREFS_HEADER_VALUE)
  {
    telnet_enabled = user_prefs.fields.telnet.flags.enabled;
    telnet_port = user_prefs.fields.telnet.port;

    ntp_server = String(user_prefs.fields.ntp.server);
    ntp_interval = user_prefs.fields.ntp.interval;

    remote_tzinfo_enabled = user_prefs.fields.rtzinfo.flags.enabled;
    remote_tzinfo_json_server = String(user_prefs.fields.rtzinfo.server);
    remote_tzinfo_json_path = String(user_prefs.fields.rtzinfo.path);
    remote_tzinfo_regenerate_url();
    remote_tzinfo_interval = user_prefs.fields.rtzinfo.interval;
    remote_tzinfo_gnss_accuracy = user_prefs.fields.rtzinfo.gnss_accuracy;

    updater_json_server = String(user_prefs.fields.updater.server);
    updater_json_path = String(user_prefs.fields.updater.path);
    updater_regenerate_url();
    updater_config_string = String(user_prefs.fields.updater.config);
    updater_auto_enabled = user_prefs.fields.updater.flags.auto_enabled;
    updater_auto_check_local_hour = user_prefs.fields.updater.flags.auto_time;
  }
  else
  {

  }
}


void user_prefs_sync(void)
{
  extern bool telnet_enabled; // esp-telnet-enable // esp-telnet-disable // TELNET_ENABLED_DEFAULT 1
  extern uint16_t telnet_port; // esp-telnet-port // TELNET_PORT_DEFAULT 23

  extern String ntp_server; // #define CLOCK_NTP_SERVER_DEFAULT "rubidium.darksky.io" // esp-ntp-set-interval
  uint32_t ntp_interval; // #define CLOCK_NTP_INTERVAL_DEFAULT 1800 // esp-ntp-set-server

  extern bool remote_tzinfo_enabled; // #define REMOTE_TZINFO_ENABLED_DEFAULT 1 // esp-tzinfo-enable
  extern String remote_tzinfo_json_server; // #define REMOTE_TZINFO_SERVER_DEFAULT "rubidium.darksky.io" // esp-tzinfo-set-server // remote_tzinfo_regenerate_url()
  extern String remote_tzinfo_json_path; // #define REMOTE_TZINFO_PATH_DEFAULT   "/smol_clock/v2/tzinfo" // esp-tzinfo-set-path // remote_tzinfo_regenerate_url()
  extern uint32_t remote_tzinfo_interval; // #define REMOTE_TZINFO_INTERVAL_DEFAULT 900 // esp-tzinfo-set-interval
  extern int32_t remote_tzinfo_gnss_accuracy; // #define REMOTE_TZINFO_GNSS_ACCURACY_DEFAULT 1 // esp-tzinfo-set-acc

  extern String updater_json_server; // esp-update-set-server // updater_regenerate_url()
  extern String updater_json_path; // esp-update-set-path // updater_regenerate_url()
  extern String updater_config_string; // #define UPDATER_CONFIG_STRING_DEFAULT "" // esp-update-set-config
  extern bool updater_auto_enabled; // #define UPDATE_AUTO_ENABLED_DEFAULT 0 // esp-update-auto-enable
  extern uint8_t updater_auto_check_local_hour; // #define UPDATE_AUTO_CHECK_LOCAL_HOUR_DEFAULT 4 // esp-update-auto-time

  user_prefs.fields.header = USER_PREFS_HEADER_VALUE;

  // Telnet
  user_prefs.fields.telnet.flags.enabled = telnet_enabled;
  user_prefs.fields.telnet.port          = telnet_port;

  // NTP
  strlcpy(user_prefs.fields.ntp.server, ntp_server.c_str(), PREFERENCES_STORED_STRING_SIZE);
  user_prefs.fields.ntp.interval = ntp_interval;

  // Remote tzinfo
  user_prefs.fields.rtzinfo.flags.enabled   = remote_tzinfo_enabled;
  strlcpy(user_prefs.fields.rtzinfo.server, remote_tzinfo_json_server.c_str(), PREFERENCES_STORED_STRING_SIZE);
  strlcpy(user_prefs.fields.rtzinfo.path,   remote_tzinfo_json_path.c_str(),   PREFERENCES_STORED_STRING_SIZE);
  user_prefs.fields.rtzinfo.interval      = remote_tzinfo_interval;
  user_prefs.fields.rtzinfo.gnss_accuracy = remote_tzinfo_gnss_accuracy;

  // Updater
  user_prefs.fields.updater.flags.auto_enabled = updater_auto_enabled;
  user_prefs.fields.updater.flags.auto_time    = updater_auto_check_local_hour;
  strlcpy(user_prefs.fields.updater.server, updater_json_server.c_str(),  PREFERENCES_STORED_STRING_SIZE);
  strlcpy(user_prefs.fields.updater.path,   updater_json_path.c_str(),    PREFERENCES_STORED_STRING_SIZE);
  strlcpy(user_prefs.fields.updater.config, updater_config_string.c_str(), PREFERENCES_STORED_STRING_SIZE);
}


void user_prefs_reset(void)
{
  memset(&user_prefs.raw, 0, sizeof(user_prefs.raw));

  user_prefs.fields.header = USER_PREFS_HEADER_VALUE;

  // Telnet
  user_prefs.fields.telnet.flags.enabled = TELNET_ENABLED_DEFAULT;
  user_prefs.fields.telnet.port          = TELNET_PORT_DEFAULT;

  // NTP
  strlcpy(user_prefs.fields.ntp.server, CLOCK_NTP_SERVER_DEFAULT, PREFERENCES_STORED_STRING_SIZE);
  user_prefs.fields.ntp.interval = CLOCK_NTP_INTERVAL_DEFAULT;

  // Remote tzinfo
  user_prefs.fields.rtzinfo.flags.enabled   = REMOTE_TZINFO_ENABLED_DEFAULT;
  strlcpy(user_prefs.fields.rtzinfo.server, REMOTE_TZINFO_SERVER_DEFAULT, PREFERENCES_STORED_STRING_SIZE);
  strlcpy(user_prefs.fields.rtzinfo.path,   REMOTE_TZINFO_PATH_DEFAULT,   PREFERENCES_STORED_STRING_SIZE);
  user_prefs.fields.rtzinfo.interval      = REMOTE_TZINFO_INTERVAL_DEFAULT;
  user_prefs.fields.rtzinfo.gnss_accuracy = REMOTE_TZINFO_GNSS_ACCURACY_DEFAULT;

  // Updater
  user_prefs.fields.updater.flags.auto_enabled = UPDATE_AUTO_ENABLED_DEFAULT;
  user_prefs.fields.updater.flags.auto_time    = UPDATE_AUTO_CHECK_LOCAL_HOUR_DEFAULT;
  strlcpy(user_prefs.fields.updater.server, UPDATER_JSON_SERVER_DEFAULT,  PREFERENCES_STORED_STRING_SIZE);
  strlcpy(user_prefs.fields.updater.path,   UPDATER_JSON_PATH_DEFAULT,    PREFERENCES_STORED_STRING_SIZE);
  strlcpy(user_prefs.fields.updater.config, UPDATER_CONFIG_STRING_DEFAULT, PREFERENCES_STORED_STRING_SIZE);

  user_prefs_apply();
}


void user_prefs_load(void)
{
  user_pref_store.begin(PREFERENCES_NAMESPACE_STRING, RO_MODE);
  user_pref_store.getBytes(PREFERENCES_NAMESPACE_KEY_STRUCT, &user_prefs.raw, sizeof(user_prefs.raw));
  user_pref_store.end();
}

void user_prefs_save(void)
{
  user_pref_store.begin(PREFERENCES_NAMESPACE_STRING, RW_MODE);
  user_pref_store.putBytes(PREFERENCES_NAMESPACE_KEY_STRUCT, &user_prefs.raw, sizeof(user_prefs.raw));
  user_pref_store.end();
}

void user_prefs_print(Stream *output)
{
  output->printf("header: 0x%08lX\n", user_prefs.fields.header);

  // Telnet
  output->printf("telnet.flags.enabled: %u\n", user_prefs.fields.telnet.flags.enabled);
  output->printf("telnet.port: %u\n", user_prefs.fields.telnet.port);

  // NTP
  output->printf("ntp.server: %s\n", user_prefs.fields.ntp.server);
  output->printf("ntp.interval: %u\n", user_prefs.fields.ntp.interval);

  // Remote tzinfo
  output->printf("rtzinfo.flags.enabled: %u\n", user_prefs.fields.rtzinfo.flags.enabled);
  output->printf("rtzinfo.server: %s\n", user_prefs.fields.rtzinfo.server);
  output->printf("rtzinfo.path: %s\n", user_prefs.fields.rtzinfo.path);
  output->printf("rtzinfo.interval: %u\n", user_prefs.fields.rtzinfo.interval);
  output->printf("rtzinfo.gnss_accuracy: %i\n", user_prefs.fields.rtzinfo.gnss_accuracy);

  // Updater
  output->printf("updater.flags.auto_enabled: %u\n", user_prefs.fields.updater.flags.auto_enabled);
  output->printf("updater.flags.auto_time: %u\n", user_prefs.fields.updater.flags.auto_time);
  output->printf("updater.server: %s\n", user_prefs.fields.updater.server);
  output->printf("updater.path: %s\n", user_prefs.fields.updater.path);
  output->printf("updater.config: %s\n", user_prefs.fields.updater.config);
}

void user_prefs_erase(void)
{
  nvs_flash_erase();
  nvs_flash_init();
}
