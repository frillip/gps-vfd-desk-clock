#include "serial_console.h"

const char* serial_console_help_text = R"literal(
esp-reset = Reset the ESP
esp-set-interval [n] = Set NTP interval to [n] (min 300, max 43200)
esp-set-server [s] = Set the NTP server to [s]
esp-resync = Force NTP sync
esp-wifi-info = Print WiFi info
esp-wifi-scan = Scan for and print WiFi networks
esp-wifi-connect = Force WiFi connect
esp-wifi-disconnect = Force WiFi disconnect
esp-wifi-ssid [s] = Set the saved SSID to [s]
esp-wifi-pass [s] = Set the saved passphrase to [s]
esp-wifi-dhcp [b] = Enable/disable DHCP
esp-wifi-ip [s] = Set static IP to [s], unless [s] is 'dhcp' or 'auto'
esp-wifi-mask [s] = Set ip mask to [s], only valid with static IP
esp-wifi-gateway [s] = Set gateway to [s], only valid with static IP
esp-wifi-clear = Clear saved WiFi config
esp-wifi-setup = Enable WiFi setup AP mode

esp-update-check = Check for an OTA update
esp-update-pull = Check for and perform an OTA update if available
esp-update-force = Force and OTA update, regardless of version, and install immediately
esp-update-set-server = Set update server hostname
esp-update-set-path = Set update server path

esp-clear-all = Clear all settings
esp-save = Save settings

pic-info = Show info directly from PIC
pic-reset = Resets the PIC
pic-set-rtc [n] = Set PIC delta epoch to [n] unix epoch time
pic-set-tz-offset [n] = Set timezone offset to [n] in seconds, rounds to nearest 15 minutes
pic-set-dst-offset [n] = Set dst offset to [n] in seconds, rounds to nearest 15 minutes
pic-set-dst-auto [b] = Enable/disable auto dst
pic-set-dst-active [b] = Enable/disable dst (pic-set-dst-auto must be off)
pic-set-alarm-enabled [b] = Enable/disable alarm
pic-set-alarm [n] = Set PIC alarm to [n] seconds past midnight
pic-set-delta [n] = Set PIC delta epoch to [n] unix epoch time
pic-set-beeps [b] = Enable/disable beeping
pic-set-display [e] = Set pic display to [e]: 1=HHMM, 2=MMSS, 3=SSMM, 4=YYYY, 5=MMDD
pic-set-brightness-auto [b] = Set display brightness to auto
pic-set-brightness [n] = Set display brightness to n / 4000
pic-show-eeprom = show settings stored in EEPROM
pic-show-config = show running config settings
pic-clear-all = Clear all settings to defaults
pic-save = Save settings
pic-bootloader-enter = Enter bootloader on PIC
pic-bootloader-exit = Exit bootloader on PIC

rst-all = Reset both
rst-pic = Same as pic-reset
rst-esp = Same as esp-reset

help = show this text
help x = Print help for the command x

\n = print available data
)literal";

USER_CMD_TYPE serial_console_check_1(const char *cmd_buf)
{
  if(strncmp(cmd_buf, USER_CMD_STAGE_1_ESP_STRING, USER_CMD_STAGE_1_LENGTH) == 0)
  {
    return USER_CMD_TYPE_ESP;
  }
  else if(strncmp(cmd_buf, USER_CMD_STAGE_1_PIC_STRING, USER_CMD_STAGE_1_LENGTH) == 0)
  {
    return USER_CMD_TYPE_PIC;
  }
  else if(strncmp(cmd_buf, USER_CMD_STAGE_1_RST_STRING, USER_CMD_STAGE_1_LENGTH) == 0)
  {
    return USER_CMD_TYPE_RST;
  }
  else if(strncmp(cmd_buf, USER_CMD_STAGE_1_HELP_STRING, USER_CMD_STAGE_1_LENGTH) == 0)
  {
    return USER_CMD_TYPE_HELP;
  }
  return USER_CMD_TYPE_NONE;
}


USER_CMD serial_console_check_2_esp(const char *cmd_buf)
{
  // Check if chars 5-10 are "wifi-"
  if(strncmp(cmd_buf + USER_CMD_STAGE_1_LENGTH, USER_CMD_ESP_WIFI_PREFIX_STRING, USER_CMD_ESP_WIFI_PREFIX_LENGTH) == 0)
  {
    if(strcmp(cmd_buf, USER_CMD_ESP_WIFI_INFO_STRING) == 0)
    {
      return USER_CMD_ESP_WIFI_INFO;
    }

    if(strcmp(cmd_buf, USER_CMD_ESP_WIFI_SCAN_STRING) == 0)
    {
      return USER_CMD_ESP_WIFI_SCAN;
    }

    if(strcmp(cmd_buf, USER_CMD_ESP_WIFI_CONNECT_STRING) == 0)
    {
      return USER_CMD_ESP_WIFI_CONNECT;
    }

    if(strcmp(cmd_buf, USER_CMD_ESP_WIFI_DISCONNECT_STRING) == 0)
    {
      return USER_CMD_ESP_WIFI_DISCONNECT;
    }

    if(strcmp(cmd_buf, USER_CMD_ESP_WIFI_SSID_STRING) == 0)
    {
      return USER_CMD_ESP_WIFI_SSID;
    }

    if(strcmp(cmd_buf, USER_CMD_ESP_WIFI_PASS_STRING) == 0)
    {
      return USER_CMD_ESP_WIFI_PASS;
    }

    if(strcmp(cmd_buf, USER_CMD_ESP_WIFI_DHCP_STRING) == 0)
    {
      return USER_CMD_ESP_WIFI_DHCP;
    }

    if(strcmp(cmd_buf, USER_CMD_ESP_WIFI_IP_STRING) == 0)
    {
      return USER_CMD_ESP_WIFI_IP;
    }

    if(strcmp(cmd_buf, USER_CMD_ESP_WIFI_MASK_STRING) == 0)
    {
      return USER_CMD_ESP_WIFI_MASK;
    }

    if(strcmp(cmd_buf, USER_CMD_ESP_WIFI_GATEWAY_STRING) == 0)
    {
      return USER_CMD_ESP_WIFI_GATEWAY;
    }

    if(strcmp(cmd_buf, USER_CMD_ESP_WIFI_CLEAR_STRING) == 0)
    {
      return USER_CMD_ESP_WIFI_CLEAR;
    }

    if(strcmp(cmd_buf, USER_CMD_ESP_WIFI_SETUP_STRING) == 0)
    {
      return USER_CMD_ESP_WIFI_SETUP;
    }
  }
  else
  {
    if(strcmp(cmd_buf, USER_CMD_ESP_RESET_STRING) == 0)
    {
      return USER_CMD_ESP_RESET;
    }

    if(strcmp(cmd_buf, USER_CMD_ESP_NTP_SET_INTERVAL_STRING) == 0)
    {
      return USER_CMD_ESP_NTP_SET_INTERVAL;
    }

    if(strcmp(cmd_buf, USER_CMD_ESP_NTP_SET_SERVER_STRING) == 0)
    {
      return USER_CMD_ESP_NTP_SET_SERVER;
    }

    if(strcmp(cmd_buf, USER_CMD_ESP_NTP_RESYNC_STRING) == 0)
    {
      return USER_CMD_ESP_NTP_RESYNC;
    }

    if(strcmp(cmd_buf, USER_CMD_ESP_UPDATE_CHECK_STRING) == 0)
    {
      return USER_CMD_ESP_UPDATE_CHECK;
    }

    if(strcmp(cmd_buf, USER_CMD_ESP_UPDATE_PULL_STRING) == 0)
    {
      return USER_CMD_ESP_UPDATE_PULL;
    }

    if(strcmp(cmd_buf, USER_CMD_ESP_UPDATE_FORCE_STRING) == 0)
    {
      return USER_CMD_ESP_UPDATE_FORCE;
    }

    if(strcmp(cmd_buf, USER_CMD_ESP_UPDATE_SET_SERVER_STRING) == 0)
    {
      return USER_CMD_ESP_UPDATE_SET_SERVER;
    }

    if(strcmp(cmd_buf, USER_CMD_ESP_UPDATE_SET_PATH_STRING) == 0)
    {
      return USER_CMD_ESP_UPDATE_SET_PATH;
    }
    /*
    if(strcmp(cmd_buf, USER_CMD_ESP_UPDATE_PUSH_ENABLE_STRING) == 0)
    {
      return USER_CMD_ESP_UPDATE_PUSH_ENABLE;
    }

    if(strcmp(cmd_buf, USER_CMD_ESP_UPDATE_PUSH_DISABLE_STRING) == 0)
    {
      return USER_CMD_ESP_UPDATE_PUSH_DISABLE;
    }
    */
    if(strcmp(cmd_buf, USER_CMD_ESP_CLEAR_ALL_STRING) == 0)
    {
      return USER_CMD_ESP_CLEAR_ALL;
    }

    if(strcmp(cmd_buf, USER_CMD_ESP_SAVE_STRING) == 0)
    {
      return USER_CMD_ESP_SAVE;
    }
  }
  return USER_CMD_NONE;
}


USER_CMD serial_console_check_2_pic(const char *cmd_buf)
{
    if(strcmp(cmd_buf, USER_CMD_PIC_INFO_STRING) == 0)
    {
      return USER_CMD_PIC_INFO;
    }

    if(strcmp(cmd_buf, USER_CMD_PIC_RESYNC_STRING) == 0)
    {
      return USER_CMD_PIC_RESYNC;
    }

    if(strcmp(cmd_buf, USER_CMD_PIC_RESET_STRING) == 0)
    {
      return USER_CMD_PIC_RESET;
    }

    if(strcmp(cmd_buf, USER_CMD_PIC_SET_RTC_STRING) == 0)
    {
      return USER_CMD_PIC_SET_RTC;
    }

    if(strcmp(cmd_buf, USER_CMD_PIC_SET_TZ_OFFSET_STRING) == 0)
    {
      return USER_CMD_PIC_SET_TZ_OFFSET;
    }

    if(strcmp(cmd_buf, USER_CMD_PIC_SET_DST_OFFSET_STRING) == 0)
    {
      return USER_CMD_PIC_SET_DST_OFFSET;
    }

    if(strcmp(cmd_buf, USER_CMD_PIC_SET_DST_AUTO_STRING) == 0)
    {
      return USER_CMD_PIC_SET_DST_AUTO;
    }

    if(strcmp(cmd_buf, USER_CMD_PIC_SET_DST_ACTIVE_STRING) == 0)
    {
      return USER_CMD_PIC_SET_DST_ACTIVE;
    }

    if(strcmp(cmd_buf, USER_CMD_PIC_SET_ALARM_ENABLED_STRING) == 0)
    {
      return USER_CMD_PIC_SET_ALARM_ENABLED;
    }

    if(strcmp(cmd_buf, USER_CMD_PIC_SET_ALARM_STRING) == 0)
    {
      return USER_CMD_PIC_SET_ALARM;
    }

    if(strcmp(cmd_buf, USER_CMD_PIC_SET_DELTA_STRING) == 0)
    {
      return USER_CMD_PIC_SET_DELTA;
    }

    if(strcmp(cmd_buf, USER_CMD_PIC_SET_BEEPS_STRING) == 0)
    {
      return USER_CMD_PIC_SET_BEEPS;
    }

    if(strcmp(cmd_buf, USER_CMD_PIC_SET_DISPLAY_STRING) == 0)
    {
      return USER_CMD_PIC_SET_DISPLAY;
    }

    if(strcmp(cmd_buf, USER_CMD_PIC_SET_BRIGHTNESS_AUTO_STRING) == 0)
    {
      return USER_CMD_PIC_SET_BRIGHTNESS_AUTO;
    }

    if(strcmp(cmd_buf, USER_CMD_PIC_SET_BRIGHTNESS_STRING) == 0)
    {
      return USER_CMD_PIC_SET_BRIGHTNESS;
    }

    if(strcmp(cmd_buf, USER_CMD_PIC_SHOW_EEPROM_STRING) == 0)
    {
      return USER_CMD_PIC_SHOW_EEPROM;
    }

    if(strcmp(cmd_buf, USER_CMD_PIC_SHOW_CONFIG_STRING) == 0)
    {
      return USER_CMD_PIC_SHOW_CONFIG;
    }

    if(strcmp(cmd_buf, USER_CMD_PIC_CLEAR_ALL_STRING) == 0)
    {
      return USER_CMD_PIC_CLEAR_ALL;
    }

    if(strcmp(cmd_buf, USER_CMD_PIC_SAVE_STRING) == 0)
    {
      return USER_CMD_PIC_SAVE;
    }

    if(strcmp(cmd_buf, USER_CMD_PIC_BOOTLOADER_ENTER_STRING) == 0)
    {
      return USER_CMD_PIC_BOOTLOADER_ENTER;
    }

    if(strcmp(cmd_buf, USER_CMD_PIC_BOOTLOADER_EXIT_STRING) == 0)
    {
      return USER_CMD_PIC_BOOTLOADER_EXIT;
    }

  return USER_CMD_NONE;
}


USER_CMD serial_console_check_2_rst(const char *cmd_buf)
{
  if(strcmp(cmd_buf, USER_CMD_RESET_ALL_STRING) == 0)
  {
    return USER_CMD_RESET_ALL;
  }
  if(strcmp(cmd_buf, USER_CMD_RESET_ESP_STRING) == 0)
  {
    return USER_CMD_ESP_RESET;
  }
  if(strcmp(cmd_buf, USER_CMD_RESET_PIC_STRING) == 0)
  {
    return USER_CMD_PIC_RESET;
  }
  return USER_CMD_NONE;
}


USER_CMD serial_console_check_2_help(const char *cmd_buf)
{
  if(strcmp(cmd_buf, USER_CMD_HELP_STRING) == 0)
  {
    return USER_CMD_HELP;
  }
  return USER_CMD_NONE;
}

void serial_console_help(Stream *output) // To do: Help text per command? Or let them suffer? (const char *arg_buf)
{
  serial_console_print_help_all(output);
  /*
  if(arg_buf[0] == 0x00)
  {
    serial_console_print_help_all();
  }
  else
  {
    // To do: Help text per command? Or let them suffer?
    serial_console_print_help_all();
  }
  */
}

void serial_console_print_help_all(Stream *output)
{
  output->printf(serial_console_help_text);
}


void serial_console_exec(Stream *output, USER_CMD cmd, const char *arg_buf)
{
  switch(cmd)
  {
    case USER_CMD_NONE:
      break;

    case USER_CMD_ESP_RESET:
      output->println("ESP resetting");
      ESP.restart(); // And reset
      break;

    case USER_CMD_ESP_NTP_SET_INTERVAL:
      uint32_t ntp_interval_new;
      if(serial_console_validate_uint32(arg_buf, &ntp_interval_new))
      {
        extern uint32_t ntp_interval;
        output->printf("Setting NTP resync to %u\n", ntp_interval_new);
        ntp_interval = ntp_interval_new;
        setInterval(ntp_interval);
      }
      break;

    case USER_CMD_ESP_NTP_SET_SERVER:
      output->printf("Not implemented yet :(\n");
      break;

    case USER_CMD_ESP_NTP_RESYNC:
      output->printf("Manual NTP resync\n");
      updateNTP();
      break;

    case USER_CMD_ESP_WIFI_INFO:
      sercon_print_wifi(output);
      break;

    case USER_CMD_ESP_WIFI_SCAN:
      sercon_print_ssids(output);
      break;

    case USER_CMD_ESP_WIFI_CONNECT:
      output->printf("ESP reconnecting to WiFi\n");
      WiFi.reconnect();
      break;

    case USER_CMD_ESP_WIFI_DISCONNECT:
      output->printf("ESP disconnecting from WiFi\n");
      WiFi.disconnect();
      break;

    case USER_CMD_ESP_WIFI_SSID:
      output->printf("Not implemented yet :(\n");
      break;

    case USER_CMD_ESP_WIFI_PASS:
      output->printf("Not implemented yet :(\n");
      break;

    case USER_CMD_ESP_WIFI_DHCP:
      output->printf("Not implemented yet :(\n");
      break;

    case USER_CMD_ESP_WIFI_IP:
      output->printf("Not implemented yet :(\n");
      break;

    case USER_CMD_ESP_WIFI_MASK:
      output->printf("Not implemented yet :(\n");
      break;

    case USER_CMD_ESP_WIFI_GATEWAY:
      output->printf("Not implemented yet :(\n");
      break;

    case USER_CMD_ESP_WIFI_CLEAR:
      {
        extern WiFiManager wm;
        output->printf("ESP clearing WiFi config\n");
        wm.resetSettings(); // Delete WiFi credentials
        ESP.restart(); // And reset
      }
      break;

    case USER_CMD_ESP_WIFI_SETUP:
      {
        extern WiFiManager wm;
        output->printf("ESP clearing WiFi config\n");
        wm.resetSettings(); // Delete WiFi credentials
        ESP.restart(); // And reset
      }
      break;

    case USER_CMD_ESP_UPDATE_CHECK:
      updater_check(output);
      break;

    case USER_CMD_ESP_UPDATE_PULL:
      updater_pull(output);
      break;

    case USER_CMD_ESP_UPDATE_FORCE:
      updater_force(output);
      break;

    case USER_CMD_ESP_UPDATE_SET_SERVER:
      output->printf("Not implemented yet :(\n");
      break;

    case USER_CMD_ESP_UPDATE_SET_PATH:
      output->printf("Not implemented yet :(\n");
      break;
    /*
    case USER_CMD_ESP_UPDATE_PUSH_ENABLE:
      updater_push_enable();
      print_updater_push_info(output);
      break;

    case USER_CMD_ESP_UPDATE_PUSH_DISABLE:
      updater_push_disable();
      print_updater_push_info(output);
      break;
    */

    case USER_CMD_ESP_CLEAR_ALL:
      {
        extern WiFiManager wm;
        output->printf("ESP clearing WiFi config\n");
        wm.resetSettings(); // Delete WiFi credentials
        ESP.restart(); // And reset
      }
      break;

    case USER_CMD_ESP_SAVE:
      output->printf("Not implemented yet :(\n");
      break;

    case USER_CMD_PIC_INFO:
      pic_uart_tx_userdata(cmd, 0, output);
      break;

    case USER_CMD_PIC_RESYNC:
      pic_uart_tx_userdata(cmd, 0, output);
      break;

    case USER_CMD_PIC_RESET:
      pic_uart_tx_userdata(cmd, 0, output);
      break;

    case USER_CMD_PIC_SET_RTC:
      // time_t values are implemented as uint32_t in XC16, not int32_t
      uint32_t rtc_val_new;
      if(serial_console_validate_uint32(arg_buf, &rtc_val_new))
      {
        pic_uart_tx_userdata(cmd, rtc_val_new, output);
      }
      break;

    case USER_CMD_PIC_SET_TZ_OFFSET:
      int32_t tz_offset_val_new;
      if(serial_console_validate_int32(arg_buf, &tz_offset_val_new))
      {
        pic_uart_tx_userdata(cmd, tz_offset_val_new, output);
      }
      break;

    case USER_CMD_PIC_SET_DST_OFFSET:
      uint32_t dst_offset_val_new;
      if(serial_console_validate_uint32(arg_buf, &dst_offset_val_new))
      {
        pic_uart_tx_userdata(cmd, dst_offset_val_new, output);
      }
      break;

    case USER_CMD_PIC_SET_DST_AUTO:
      output->printf("Not implemented yet :(\n");
      break;

    case USER_CMD_PIC_SET_DST_ACTIVE:
      output->printf("Not implemented yet :(\n");
      break;

    case USER_CMD_PIC_SET_ALARM_ENABLED:
      output->printf("Not implemented yet :(\n");
      break;

    case USER_CMD_PIC_SET_ALARM:
      output->printf("Not implemented yet :(\n");
      break;

    case USER_CMD_PIC_SET_DELTA:
      // time_t values are implemented as uint32_t in XC16, not int32_t
      uint32_t delta_val_new;
      if(serial_console_validate_uint32(arg_buf, &delta_val_new))
      {
        pic_uart_tx_userdata(cmd, delta_val_new, output);
      }
      break;

    case USER_CMD_PIC_SET_BEEPS:
      output->printf("Not implemented yet :(\n");
      break;

    case USER_CMD_PIC_SET_DISPLAY:
      output->printf("Not implemented yet :(\n");
      break;

    case USER_CMD_PIC_SET_BRIGHTNESS_AUTO:
      pic_uart_tx_userdata(cmd, 0, output);
      break;

    case USER_CMD_PIC_SET_BRIGHTNESS:
      uint32_t brightness;
      if(serial_console_validate_uint32(arg_buf, &brightness))
      {
        pic_uart_tx_userdata(cmd, brightness, output);
      }
      break;

    case USER_CMD_PIC_SHOW_EEPROM:
      pic_uart_tx_userdata(cmd, 0, output);
      break;

    case USER_CMD_PIC_SHOW_CONFIG:
      pic_uart_tx_userdata(cmd, 0, output);
      break;

    case USER_CMD_PIC_CLEAR_ALL:
      pic_uart_tx_userdata(cmd, 0, output);
      break;

    case USER_CMD_PIC_SAVE:
      pic_uart_tx_userdata(cmd, 0, output);
      break;

    case USER_CMD_PIC_BOOTLOADER_ENTER:
      pic_enter_bootloader(output);
      break;

    case USER_CMD_PIC_BOOTLOADER_EXIT:
      pic_exit_bootloader(output);
      break;

    case USER_CMD_RESET_ALL:
      pic_uart_tx_userdata(USER_CMD_PIC_RESET, 0, output);
      ESP.restart();
      break;
  }
}

void sercon_print_wifi(Stream *output)
{
  wifi_config_t cachedConfig;
  esp_err_t configResult = esp_wifi_get_config( (wifi_interface_t)ESP_IF_WIFI_STA, &cachedConfig );
      
  if( configResult == ESP_OK )
  {
      output->printf("SSID: %s\n", cachedConfig.ap.ssid);
      output->printf("PSK: %s\n", cachedConfig.ap.password);
      output->printf("IP: %s\n", WiFi.localIP().toString().c_str());
  }
  else
  {
      output->printf( "Nope; esp_wifi_get_config returned %i", configResult );
  }
  output->printf("\n");
}

void sercon_print_ssids(Stream *output)
{
  output->printf("Scan start\n");
  // WiFi.scanNetworks will return the number of networks found
  int n = WiFi.scanNetworks();
  output->print("Scan done\n");
  if (n == 0)
  {
    output->printf("No networks found\n");
  } else
  {
    output->printf("%i networks found\n",n);
    for (int i = 0; i < n; ++i)
    {
      // Print SSID and RSSI for each network found
      output->printf("%u: %s", i + 1, WiFi.SSID(i).c_str());
      output->printf(" (%i)", WiFi.RSSI(i));

      switch(WiFi.encryptionType(i))
      {
        case WIFI_AUTH_OPEN:
          output->printf("     OPEN");
          break;

        case WIFI_AUTH_WEP:
          output->printf("      WEP");
          break;

        case WIFI_AUTH_WPA_PSK:
          output->printf("  WPA-PSK");
          break;

        case WIFI_AUTH_WPA2_PSK:
        case WIFI_AUTH_WPA_WPA2_PSK:
          output->printf(" WPA2-PSK");
          break;

        case WIFI_AUTH_WPA2_WPA3_PSK:
        case WIFI_AUTH_WPA3_PSK:
          output->printf("  OPEN");
          break;

        case WIFI_AUTH_ENTERPRISE:
          output->printf("  WPA-EAP");
          break;

/* // Same value as WIFI_AUTH_ENTERPRISE above
        case WIFI_AUTH_WPA2_ENTERPRISE:
          output->printf(" WPA2-EAP");
          break;
*/

/*  // Not defined in arduino esp32 yet
        //case WIFI_AUTH_WPA2_WPA3_ENTERPRISE:
        case WIFI_AUTH_WPA2_WPA3_ENTERPRISE :
          output->printf(" WPA3-EAP");
          break;
*/

        default:
          output->printf("   OTHER");
          break;
      }
      output->printf("\n");
    }
  }
  output->printf("\n");
}


void serial_console_print_info(Stream *output)
{
  output->printf("\n");
  print_pic_time(output);
  
  if(gnss_is_detected() && timeStatus() != timeNotSet)
  {
    print_gnss_pps_offset(output);
  }
  if(pic_is_detected() && timeStatus() != timeNotSet)
  {
    print_pic_pps_offset(output);
  }
  if(pic_is_detected() && gnss_is_detected() && timeStatus() != timeNotSet)
  {
    print_pic_pps_relative_offset(output);
  }
  
  print_offset_data(output);
  print_gnss_data(output);
  print_rtc_data(output);
  print_veml_data(output);
  print_bme_data(output);
  print_sync_state_machine(output);
}

/*
uint32_t local_print_millis = 0;

void serial_console_second_changed(uint32_t millis)
{
  local_print_millis = millis;
}


bool serial_console_print_local_available(void)
{
  // Disable this for now
  return 0;

  if(local_print_millis)
  {
    if(millis() > (local_print_millis + LOCAL_TIME_PRINT_DELAY))
    {
      return 1;
    }
  }
  return 0;
}


void serial_console_print_local(void)
{
  extern time_t pic;
  if(millis() > (local_print_millis + LOCAL_TIME_PRINT_DELAY))
  {
    print_local_time(pic);
    output->println("");
    local_print_millis = 0;
  }
}
*/

bool serial_console_validate_uint32(const char* input, uint32_t* output)
{
  // Check if the input is null
  if (input == NULL)
  {
    return false;
  }

  uint32_t temp;
  // Parse
  int result = sscanf(input, "%ld", &temp);

  // Success?
  if (result == 1)
  {
    // Bounds check
    if (temp > UINT32_MAX)
    {
        return false;
    }
    // Store the parsed value in the output variable
    *output = temp;
    return true;
  }

  // If parsing failed, return false
  return false;
}

bool serial_console_validate_int32(const char* input, int32_t* output)
{
  // Check if the input is null
  if (input == NULL)
  {
    return false;
  }

  int32_t temp;
  // Parse
  int result = sscanf(input, "%ld", &temp);

  // Success?
  if (result == 1)
  {
    // Bounds check
    if (temp > INT32_MAX)
    {
        return false;
    }
    if (temp < INT32_MIN)
    {
        return false;
    }
    // Store the parsed value in the output variable
    *output = temp;
    return true;
  }

  // If parsing failed, return false
  return false;
}
