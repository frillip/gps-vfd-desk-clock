#include "serial_console.h"

char console_buffer[SERIAL_CONSOLE_BUFFER_LENGTH] = {0};
uint8_t console_buffer_offset = 0;
char user_cmd_buf[SERIAL_CONSOLE_BUFFER_LENGTH] = {0};
char user_arg_buf[SERIAL_CONSOLE_BUFFER_LENGTH] = {0};
char last_rx_char;

void serial_console_init()
{
    Serial.begin(DEBUG_BAUD);
}

bool serial_console_char_available(void)
{
  return Serial.available();
}

void serial_console_task(void)
{
  if(Serial.available())
  {
    char c = Serial.read();
    Serial.print(c);

    if(c == 0x0A || c == 0x0D) // new line char
    {
      USER_CMD_TYPE cmd_type = USER_CMD_TYPE_NONE;
      USER_CMD exec = USER_CMD_NONE;

      // Just exec first character in buffer for now
      // To do: implement two stage buffer check and proper commands
      // Stage 1: check first 4 bytes for 'esp-' or 'pic-' or first 5 bytes for 'reset'
      // Stage 2: check against entire command string in serial_cmds.txt
      // Stage 3: run command and parse options (if required)
      if(c == 0x0D) Serial.print("\n"); // Bodge for Windows-style \r\n line endings
      int i=0;
      while(i<console_buffer_offset)
      {
        if(console_buffer[i] == 0x20)
        {
          memcpy(user_cmd_buf, console_buffer, i);
          memcpy(user_arg_buf, console_buffer + i + 1, console_buffer_offset - i - 1);
          while(user_arg_buf[0] == 0x20) // remove excess spaces
          {
            memmove(user_arg_buf, user_arg_buf+1, console_buffer_offset - i - 1);
          }
          i=console_buffer_offset;
        }
        i++;
      }
      if(i==console_buffer_offset)
      {
        memcpy(user_cmd_buf, console_buffer, i);
      }
      Serial.print("BUF: ");
      Serial.println(console_buffer);
      Serial.print("CMD: ");
      Serial.println(user_cmd_buf);
      Serial.print("ARG: ");
      Serial.println(user_arg_buf);
      if(console_buffer_offset==0 && last_rx_char!=0x0A)
      {
        serial_console_print_info();
      }

      cmd_type = serial_console_check_1();
      switch(cmd_type)
      {
        case USER_CMD_TYPE_ESP:
          exec = serial_console_check_2_esp();
          break;

        case USER_CMD_TYPE_PIC:
          exec = serial_console_check_2_pic();
          break;

        case USER_CMD_TYPE_RST:
          exec = serial_console_check_2_rst();
          break;

        case USER_CMD_TYPE_NONE:
          if(console_buffer_offset==1) serial_console_print_info();
          break;
      }

      if(exec != USER_CMD_NONE) serial_console_exec(exec);

      memset(console_buffer, 0, SERIAL_CONSOLE_BUFFER_LENGTH);
      memset(user_cmd_buf, 0, SERIAL_CONSOLE_BUFFER_LENGTH);
      memset(user_arg_buf, 0, SERIAL_CONSOLE_BUFFER_LENGTH);
      console_buffer_offset = 0;
    }
    else if(c == 0x7F) // Backspace
    {
      console_buffer[console_buffer_offset] = 0x00;
      if(console_buffer_offset > 0) console_buffer_offset--;
    }
    else
    {
      console_buffer[console_buffer_offset] = c;
      console_buffer_offset++;
      if(console_buffer_offset > SERIAL_CONSOLE_BUFFER_LENGTH -1)
      {
        memset(console_buffer, 0, SERIAL_CONSOLE_BUFFER_LENGTH);
        console_buffer_offset = 0;
      }
    }

    last_rx_char = c;
  }
}


USER_CMD_TYPE serial_console_check_1(void)
{
  if(strncmp(user_cmd_buf, USER_CMD_STAGE_1_ESP_STRING, USER_CMD_STAGE_1_LENGTH) == 0)
  {
    return USER_CMD_TYPE_ESP;
  }
  if(strncmp(user_cmd_buf, USER_CMD_STAGE_1_PIC_STRING, USER_CMD_STAGE_1_LENGTH) == 0)
  {
    return USER_CMD_TYPE_PIC;
  }
  if(strncmp(user_cmd_buf, USER_CMD_STAGE_1_RST_STRING, USER_CMD_STAGE_1_LENGTH) == 0)
  {
    return USER_CMD_TYPE_RST;
  }
  return USER_CMD_TYPE_NONE;
}


USER_CMD serial_console_check_2_esp(void)
{
  // Check if chars 5-10 are "wifi-"
  if(strncmp(user_cmd_buf + USER_CMD_STAGE_1_LENGTH, USER_CMD_ESP_WIFI_PREFIX_STRING, USER_CMD_ESP_WIFI_PREFIX_LENGTH) == 0)
  {
    if(strcmp(user_cmd_buf, USER_CMD_ESP_WIFI_INFO_STRING) == 0)
    {
      Serial.println(USER_CMD_ESP_WIFI_INFO_STRING);
      return USER_CMD_ESP_WIFI_INFO;
    }

    if(strcmp(user_cmd_buf, USER_CMD_ESP_WIFI_SHOW_STRING) == 0)
    {
      Serial.println(USER_CMD_ESP_WIFI_SHOW_STRING);
      return USER_CMD_ESP_WIFI_SHOW;
    }

    if(strcmp(user_cmd_buf, USER_CMD_ESP_WIFI_CONNECT_STRING) == 0)
    {
      Serial.println(USER_CMD_ESP_WIFI_CONNECT_STRING);
      return USER_CMD_ESP_WIFI_CONNECT;
    }

    if(strcmp(user_cmd_buf, USER_CMD_ESP_WIFI_DISCONNECT_STRING) == 0)
    {
      Serial.println(USER_CMD_ESP_WIFI_DISCONNECT_STRING);
      return USER_CMD_ESP_WIFI_DISCONNECT;
    }

    if(strcmp(user_cmd_buf, USER_CMD_ESP_WIFI_SSID_STRING) == 0)
    {
      Serial.println(USER_CMD_ESP_WIFI_SSID_STRING);
      return USER_CMD_ESP_WIFI_SSID;
    }

    if(strcmp(user_cmd_buf, USER_CMD_ESP_WIFI_PASS_STRING) == 0)
    {
      Serial.println(USER_CMD_ESP_WIFI_PASS_STRING);
      return USER_CMD_ESP_WIFI_PASS;
    }

    if(strcmp(user_cmd_buf, USER_CMD_ESP_WIFI_DHCP_STRING) == 0)
    {
      Serial.println(USER_CMD_ESP_WIFI_DHCP_STRING);
      return USER_CMD_ESP_WIFI_DHCP;
    }

    if(strcmp(user_cmd_buf, USER_CMD_ESP_WIFI_IP_STRING) == 0)
    {
      Serial.println(USER_CMD_ESP_WIFI_IP_STRING);
      return USER_CMD_ESP_WIFI_IP;
    }

    if(strcmp(user_cmd_buf, USER_CMD_ESP_WIFI_MASK_STRING) == 0)
    {
      Serial.println(USER_CMD_ESP_WIFI_MASK_STRING);
      return USER_CMD_ESP_WIFI_MASK;
    }

    if(strcmp(user_cmd_buf, USER_CMD_ESP_WIFI_GATEWAY_STRING) == 0)
    {
      Serial.println(USER_CMD_ESP_WIFI_GATEWAY_STRING);
      return USER_CMD_ESP_WIFI_GATEWAY;
    }

    if(strcmp(user_cmd_buf, USER_CMD_ESP_WIFI_CLEAR_STRING) == 0)
    {
      Serial.println(USER_CMD_ESP_WIFI_CLEAR_STRING);
      return USER_CMD_ESP_WIFI_CLEAR;
    }

    if(strcmp(user_cmd_buf, USER_CMD_ESP_WIFI_SETUP_STRING) == 0)
    {
      Serial.println(USER_CMD_ESP_WIFI_SETUP_STRING);
      return USER_CMD_ESP_WIFI_SETUP;
    }
  }
  else
  {
    if(strcmp(user_cmd_buf, USER_CMD_ESP_RESET_STRING) == 0)
    {
      Serial.println(USER_CMD_ESP_RESET_STRING);
      return USER_CMD_ESP_RESET;
    }

    if(strcmp(user_cmd_buf, USER_CMD_ESP_SET_INTERVAL_STRING) == 0)
    {
      Serial.println(USER_CMD_ESP_SET_INTERVAL_STRING);
      return USER_CMD_ESP_SET_INTERVAL;
    }

    if(strcmp(user_cmd_buf, USER_CMD_ESP_SET_SERVER_STRING) == 0)
    {
      Serial.println(USER_CMD_ESP_SET_SERVER_STRING);
      return USER_CMD_ESP_SET_SERVER;
    }

    if(strcmp(user_cmd_buf, USER_CMD_ESP_RESYNC_STRING) == 0)
    {
      Serial.println(USER_CMD_ESP_RESYNC_STRING);
      return USER_CMD_ESP_RESYNC;
    }
    if(strcmp(user_cmd_buf, USER_CMD_ESP_CLEAR_ALL_STRING) == 0)
    {
      Serial.println(USER_CMD_ESP_CLEAR_ALL_STRING);
      return USER_CMD_ESP_CLEAR_ALL;
    }

    if(strcmp(user_cmd_buf, USER_CMD_ESP_SAVE_STRING) == 0)
    {
      Serial.println(USER_CMD_ESP_SAVE_STRING);
      return USER_CMD_ESP_SAVE;
    }
  }
  return USER_CMD_NONE;
}


USER_CMD serial_console_check_2_pic(void)
{
    if(strcmp(user_cmd_buf, USER_CMD_PIC_INFO_STRING) == 0)
    {
      Serial.println(USER_CMD_PIC_INFO_STRING);
      return USER_CMD_PIC_INFO;
    }

    if(strcmp(user_cmd_buf, USER_CMD_PIC_RESYNC_STRING) == 0)
    {
      Serial.println(USER_CMD_PIC_RESYNC_STRING);
      return USER_CMD_PIC_RESYNC;
    }

    if(strcmp(user_cmd_buf, USER_CMD_PIC_RESET_STRING) == 0)
    {
      Serial.println(USER_CMD_PIC_RESET_STRING);
      return USER_CMD_PIC_RESET;
    }

    if(strcmp(user_cmd_buf, USER_CMD_PIC_SET_RTC_STRING) == 0)
    {
      Serial.println(USER_CMD_PIC_SET_RTC_STRING);
      return USER_CMD_PIC_SET_RTC;
    }

    if(strcmp(user_cmd_buf, USER_CMD_PIC_SET_TZ_OFFSET_STRING) == 0)
    {
      Serial.println(USER_CMD_PIC_SET_TZ_OFFSET_STRING);
      return USER_CMD_PIC_SET_TZ_OFFSET;
    }

    if(strcmp(user_cmd_buf, USER_CMD_PIC_SET_DST_OFFSET_STRING) == 0)
    {
      Serial.println(USER_CMD_PIC_SET_DST_OFFSET_STRING);
      return USER_CMD_PIC_SET_DST_OFFSET;
    }

    if(strcmp(user_cmd_buf, USER_CMD_PIC_SET_DST_AUTO_STRING) == 0)
    {
      Serial.println(USER_CMD_PIC_SET_DST_AUTO_STRING);
      return USER_CMD_PIC_SET_DST_AUTO;
    }

    if(strcmp(user_cmd_buf, USER_CMD_PIC_SET_DST_ACTIVE_STRING) == 0)
    {
      Serial.println(USER_CMD_PIC_SET_DST_ACTIVE_STRING);
      return USER_CMD_PIC_SET_DST_ACTIVE;
    }

    if(strcmp(user_cmd_buf, USER_CMD_PIC_SET_ALARM_ENABLED_STRING) == 0)
    {
      Serial.println(USER_CMD_PIC_SET_ALARM_ENABLED_STRING);
      return USER_CMD_PIC_SET_ALARM_ENABLED;
    }

    if(strcmp(user_cmd_buf, USER_CMD_PIC_SET_ALARM_STRING) == 0)
    {
      Serial.println(USER_CMD_PIC_SET_ALARM_STRING);
      return USER_CMD_PIC_SET_ALARM;
    }

    if(strcmp(user_cmd_buf, USER_CMD_PIC_SET_BEEPS_STRING) == 0)
    {
      Serial.println(USER_CMD_PIC_SET_BEEPS_STRING);
      return USER_CMD_PIC_SET_BEEPS;
    }

    if(strcmp(user_cmd_buf, USER_CMD_PIC_SET_DISPLAY_STRING) == 0)
    {
      Serial.println(USER_CMD_PIC_SET_DISPLAY_STRING);
      return USER_CMD_PIC_SET_DISPLAY;
    }

    if(strcmp(user_cmd_buf, USER_CMD_PIC_SET_BRIGHTNESS_AUTO_STRING) == 0)
    {
      Serial.println(USER_CMD_PIC_SET_BRIGHTNESS_AUTO_STRING);
      return USER_CMD_PIC_SET_BRIGHTNESS_AUTO;
    }

    if(strcmp(user_cmd_buf, USER_CMD_PIC_SET_BRIGHTNESS_STRING) == 0)
    {
      Serial.println(USER_CMD_PIC_SET_BRIGHTNESS_STRING);
      return USER_CMD_PIC_SET_BRIGHTNESS;
    }

    if(strcmp(user_cmd_buf, USER_CMD_PIC_EEPROM_SHOW_STRING) == 0)
    {
      Serial.println(USER_CMD_PIC_EEPROM_SHOW_STRING);
      return USER_CMD_PIC_EEPROM_SHOW;
    }

    if(strcmp(user_cmd_buf, USER_CMD_PIC_CLEAR_ALL_STRING) == 0)
    {
      Serial.println(USER_CMD_PIC_CLEAR_ALL_STRING);
      return USER_CMD_PIC_CLEAR_ALL;
    }

    if(strcmp(user_cmd_buf, USER_CMD_PIC_SAVE_STRING) == 0)
    {
      Serial.println(USER_CMD_PIC_SAVE_STRING);
      return USER_CMD_PIC_SAVE;
    }
  return USER_CMD_NONE;
}


USER_CMD serial_console_check_2_rst(void)
{
  if(strcmp(user_cmd_buf, USER_CMD_RESET_ALL_STRING) == 0)
  {
    Serial.println(USER_CMD_RESET_ALL_STRING);
    return USER_CMD_RESET_ALL;
  }
  if(strcmp(user_cmd_buf, USER_CMD_RESET_ESP_STRING) == 0)
  {
    Serial.println(USER_CMD_RESET_ESP_STRING);
    return USER_CMD_ESP_RESET;
  }
  if(strcmp(user_cmd_buf, USER_CMD_RESET_PIC_STRING) == 0)
  {
    Serial.println(USER_CMD_RESET_PIC_STRING);
    return USER_CMD_PIC_RESET;
  }
  return USER_CMD_NONE;
}


void sercon_print_wifi(void)
{
  wifi_config_t cachedConfig;
  esp_err_t configResult = esp_wifi_get_config( (wifi_interface_t)ESP_IF_WIFI_STA, &cachedConfig );
      
  if( configResult == ESP_OK )
  {
      Serial.printf("SSID: %s, PSK: %s", cachedConfig.ap.ssid, cachedConfig.ap.password );
  }
  else
  {
      Serial.printf( "Nope; esp_wifi_get_config returned %i", configResult );
  }
  Serial.println();
}

void sercon_print_ssids(void)
{
  Serial.println("Scan start");
  // WiFi.scanNetworks will return the number of networks found
  int n = WiFi.scanNetworks();
  Serial.println("Scan done");
  if (n == 0)
  {
    Serial.println("No networks found");
  } else
  {
    Serial.print(n);
    Serial.println(" networks found");
    for (int i = 0; i < n; ++i)
    {
      // Print SSID and RSSI for each network found
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");
      Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN)?" ":"*");
    }
  }
  Serial.println("");
}

void serial_console_exec(USER_CMD cmd)
{
  switch(cmd)
  {
    case USER_CMD_NONE:
      break;

    case USER_CMD_ESP_RESET:
      Serial.println("ESP resetting");
      ESP.restart(); // And reset
      break;

    case USER_CMD_ESP_SET_INTERVAL:
      
      break;

    case USER_CMD_ESP_SET_SERVER:
      
      break;

    case USER_CMD_ESP_RESYNC:
      Serial.println("Manual NTP resync");
      updateNTP();
      break;

    case USER_CMD_ESP_WIFI_INFO:
      sercon_print_wifi();
      break;

    case USER_CMD_ESP_WIFI_SHOW:
      sercon_print_ssids();
      break;

    case USER_CMD_ESP_WIFI_CONNECT:
      Serial.println("ESP reconnecting to WiFi");
      WiFi.reconnect();
      break;

    case USER_CMD_ESP_WIFI_DISCONNECT:
      Serial.println("ESP disconnecting from WiFi");
      WiFi.disconnect();
      break;

    case USER_CMD_ESP_WIFI_SSID:
      
      break;

    case USER_CMD_ESP_WIFI_PASS:
      
      break;

    case USER_CMD_ESP_WIFI_DHCP:
      
      break;

    case USER_CMD_ESP_WIFI_IP:
      
      break;

    case USER_CMD_ESP_WIFI_MASK:
      
      break;

    case USER_CMD_ESP_WIFI_GATEWAY:
      
      break;

    case USER_CMD_ESP_WIFI_CLEAR:
      {
        extern WiFiManager wm;
        Serial.println("ESP clearing WiFi config");
        wm.resetSettings(); // Delete WiFi credentials
        ESP.restart(); // And reset
      }
      break;

    case USER_CMD_ESP_WIFI_SETUP:
      {
        extern WiFiManager wm;
        Serial.println("ESP clearing WiFi config");
        wm.resetSettings(); // Delete WiFi credentials
        ESP.restart(); // And reset
      }
      break;

    case USER_CMD_ESP_CLEAR_ALL:
      {
        extern WiFiManager wm;
        Serial.println("ESP clearing WiFi config");
        wm.resetSettings(); // Delete WiFi credentials
        ESP.restart(); // And reset
      }
      break;

    case USER_CMD_ESP_SAVE:
      
      break;

    case USER_CMD_PIC_INFO:
      pic_uart_tx_userdata(cmd, 0);
      break;

    case USER_CMD_PIC_RESYNC:
      pic_uart_tx_userdata(cmd, 0);
      break;

    case USER_CMD_PIC_RESET:
      pic_uart_tx_userdata(cmd, 0);
      break;

    case USER_CMD_PIC_SET_RTC:
      
      break;

    case USER_CMD_PIC_SET_TZ_OFFSET:
      
      break;

    case USER_CMD_PIC_SET_DST_OFFSET:
      
      break;

    case USER_CMD_PIC_SET_DST_AUTO:
      
      break;

    case USER_CMD_PIC_SET_DST_ACTIVE:
      
      break;

    case USER_CMD_PIC_SET_ALARM_ENABLED:
      
      break;

    case USER_CMD_PIC_SET_ALARM:
      
      break;

    case USER_CMD_PIC_SET_BEEPS:
      
      break;

    case USER_CMD_PIC_SET_DISPLAY:
      
      break;

    case USER_CMD_PIC_SET_BRIGHTNESS_AUTO:
      pic_uart_tx_userdata(cmd, 0);
      break;

    case USER_CMD_PIC_SET_BRIGHTNESS:
      
      break;

    case USER_CMD_PIC_EEPROM_SHOW:
      pic_uart_tx_userdata(cmd, 0);
      break;

    case USER_CMD_PIC_CLEAR_ALL:
      pic_uart_tx_userdata(cmd, 0);
      break;

    case USER_CMD_PIC_SAVE:
      
      break;

    case USER_CMD_RESET_ALL:
      pic_uart_tx_userdata(USER_CMD_PIC_RESET, 0);
      ESP.restart();
      break;
  }
}


void serial_console_print_info(void)
{
  Serial.println("");
  print_pic_time();
  
  if(gnss_is_detected() && timeStatus() != timeNotSet)
  {
    print_gnss_pps_offset();
  }
  if(pic_is_detected() && timeStatus() != timeNotSet)
  {
    print_pic_pps_offset();
  }
  
  print_offset_data();
  print_gnss_data();
  pic_print_rtc();
  print_veml_data();
  print_bme_data();
  print_sync_state_machine();
}


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
    Serial.println("");
    local_print_millis = 0;
  }
}