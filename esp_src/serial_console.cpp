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
      if(console_buffer_offset==0 && last_rx_char!=0x0A) serial_console_exec_char(0x5A);

      cmd_type = serial_console_check_1();
      switch(cmd_type)
      {
        case USER_CMD_TYPE_ESP:

          break;

        case USER_CMD_TYPE_PIC:

          break;

        case USER_CMD_TYPE_RST:
          exec = serial_console_check_2_rst();
          break;

        case USER_CMD_TYPE_NONE:
          if(console_buffer_offset==1) serial_console_exec_char(user_cmd_buf[0]);
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

/*
#define USER_CMD_STAGE_1_ESP_STRING "esp-"
#define USER_CMD_STAGE_1_PIC_STRING "pic-"
#define USER_CMD_STAGE_1_RST_STRING "rst-"
#define USER_CMD_STAGE_1_LENGTH 4

#define USER_CMD_ESP_RESET_STRING "esp-reset"
#define USER_CMD_ESP_SET_INTERVAL_STRING "esp-set-interval"
#define USER_CMD_ESP_SET_SERVER_STRING "esp-set-server"
#define USER_CMD_ESP_RESYNC_STRING "esp-resync"
#define USER_CMD_ESP_WIFI_INFO "esp-wifi-show"
#define USER_CMD_ESP_WIFI_CONNECT_STRING "esp-wifi-connect"
#define USER_CMD_ESP_WIFI_DISCONNECT_STRING "esp-wifi-disconnect"
#define USER_CMD_ESP_WIFI_SSID_STRING "esp-wifi-ssid"
#define USER_CMD_ESP_WIFI_PASS_STRING "esp-wifi-pass"
#define USER_CMD_ESP_WIFI_DHCP_STRING "esp-wifi-dhcp"
#define USER_CMD_ESP_WIFI_IP_STRING "esp-wifi-ip"
#define USER_CMD_ESP_WIFI_MASK_STRING "esp-wifi-mask"
#define USER_CMD_ESP_WIFI_GATEWAY_STRING "esp-wifi-gateway"
#define USER_CMD_ESP_WIFI_CLEAR_STRING "esp-wifi-clear"
#define USER_CMD_ESP_WIFI_SETUP_STRING "esp-wifi-setup"
#define USER_CMD_ESP_CLEAR_ALL_STRING "esp-clear-all"
#define USER_CMD_ESP_SAVE_STRING "esp-save"
#define USER_CMD_PIC_INFO_STRING "pic-info"
#define USER_CMD_PIC_RESET_STRING "pic-reset"
#define USER_CMD_PIC_SET_RTC_STRING "pic-set-rtc"
#define USER_CMD_PIC_SET_TZ_OFFSET_STRING "pic-set-tz-offset"
#define USER_CMD_PIC_SET_DST_OFFSET_STRING "pic-set-dst-offset"
#define USER_CMD_PIC_SET_DST_AUTO_STRING "pic-set-dst-auto"
#define USER_CMD_PIC_SET_DST_ACTIVE_STRING "pic-set-dst-active"
#define USER_CMD_PIC_SET_ALARM_ENABLED_STRING "pic-set-alarm-enabled"
#define USER_CMD_PIC_SET_ALARM_STRING "pic-set-alarm"
#define USER_CMD_PIC_SET_BEEPS_STRING "pic-set-beeps"
#define USER_CMD_PIC_SET_DISPLAY_STRING "pic-set-display"
#define USER_CMD_PIC_CLEAR_ALL_STRING "pic-clear-all"
#define USER_CMD_PIC_SAVE_STRING "pic-save"
#define USER_CMD_RESET_ALL_STRING "rst-all"
#define USER_CMD_RESET_PIC_STRING "rst-pic"
#define USER_CMD_RESET_ESP_STRING "rst-esp"
*/

USER_CMD_TYPE serial_console_check_1(void)
{
  if(strncmp(user_cmd_buf, USER_CMD_STAGE_1_ESP_STRING, USER_CMD_STAGE_1_LENGTH) == 0)
  {
    Serial.println("ESP");
    return USER_CMD_TYPE_ESP;
  }
  if(strncmp(user_cmd_buf, USER_CMD_STAGE_1_PIC_STRING, USER_CMD_STAGE_1_LENGTH) == 0)
  {
    Serial.println("PIC");
    return USER_CMD_TYPE_PIC;
  }
  if(strncmp(user_cmd_buf, USER_CMD_STAGE_1_RST_STRING, USER_CMD_STAGE_1_LENGTH) == 0)
  {
    Serial.print("RST");
    return USER_CMD_TYPE_RST;
  }
  Serial.println("NONE");
  return USER_CMD_TYPE_NONE;
}

USER_CMD serial_console_check_2_esp(void)
{

}

USER_CMD serial_console_check_2_pic(void)
{

}

USER_CMD serial_console_check_2_rst(void)
{
  if(strcmp(user_cmd_buf, USER_CMD_RESET_ALL_STRING) == 0)
  {
    Serial.println(" ALL");
    return USER_CMD_RESET_ALL;
  }
  else if(strcmp(user_cmd_buf, USER_CMD_RESET_ESP_STRING) == 0)
  {
    Serial.println(" ESP");
    return USER_CMD_ESP_RESET;
  }
  else if(strcmp(user_cmd_buf, USER_CMD_RESET_PIC_STRING) == 0)
  {
    Serial.println(" PIC");
    return USER_CMD_PIC_RESET;
  }
  Serial.println(" NONE");
  return USER_CMD_NONE;
}

/*
typedef enum
{
	USER_CMD_NONE = 0,
	USER_CMD_ESP_RESET,
	USER_CMD_ESP_SET_INTERVAL,
	USER_CMD_ESP_SET_SERVER,
	USER_CMD_ESP_RESYNC,
	USER_CMD_ESP_WIFI_INFO,
	USER_CMD_ESP_WIFI_CONNECT,
	USER_CMD_ESP_WIFI_DISCONNECT,
	USER_CMD_ESP_WIFI_SSID,
	USER_CMD_ESP_WIFI_PASS,
	USER_CMD_ESP_WIFI_DHCP,
	USER_CMD_ESP_WIFI_IP,
	USER_CMD_ESP_WIFI_MASK,
	USER_CMD_ESP_WIFI_GATEWAY,
	USER_CMD_ESP_WIFI_CLEAR,
	USER_CMD_ESP_WIFI_SETUP,
	USER_CMD_ESP_CLEAR_ALL,
	USER_CMD_ESP_SAVE,
	USER_CMD_PIC_INFO,
	USER_CMD_PIC_RESET,
	USER_CMD_PIC_SET_RTC,
	USER_CMD_PIC_SET_TZ_OFFSET,
	USER_CMD_PIC_SET_DST_OFFSET,
	USER_CMD_PIC_SET_DST_AUTO,
	USER_CMD_PIC_SET_DST_ACTIVE,
	USER_CMD_PIC_SET_ALARM_ENABLED,
	USER_CMD_PIC_SET_ALARM,
	USER_CMD_PIC_SET_BEEPS,
	USER_CMD_PIC_SET_DISPLAY,
	USER_CMD_PIC_CLEAR_ALL,
	USER_CMD_PIC_SAVE,
	USER_CMD_RESET_ALL,
} USER_CMD;
*/

void serial_console_exec(USER_CMD cmd)
{
  switch(cmd)
  {
    case USER_CMD_NONE:
      break;

    case USER_CMD_ESP_RESET:
      serial_console_exec_char(0x45);
      break;

    case USER_CMD_ESP_SET_INTERVAL:
      
      break;

    case USER_CMD_ESP_SET_SERVER:
      
      break;

    case USER_CMD_ESP_RESYNC:
      
      break;

    case USER_CMD_ESP_WIFI_INFO:
      
      break;

    case USER_CMD_ESP_WIFI_CONNECT:
      
      break;

    case USER_CMD_ESP_WIFI_DISCONNECT:
      
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
      serial_console_exec_char(0x57);
      break;

    case USER_CMD_ESP_WIFI_SETUP:
      serial_console_exec_char(0x57);
      break;

    case USER_CMD_ESP_CLEAR_ALL:
      
      break;

    case USER_CMD_ESP_SAVE:
      
      break;

    case USER_CMD_PIC_INFO:
      serial_console_exec_char(0x0A);
      break;

    case USER_CMD_PIC_RESET:
      serial_console_exec_char(0x52);
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
      
      break;

    case USER_CMD_PIC_SET_BRIGHTNESS:
      
      break;

    case USER_CMD_PIC_CLEAR_ALL:
      
      break;

    case USER_CMD_PIC_SAVE:
      
      break;

    case USER_CMD_RESET_ALL:
      serial_console_exec_char(0x52);
      serial_console_exec_char(0x45);
      break;
  }
}

void serial_console_exec_char(char c)
{
  if(c==0x45) ESP.restart(); // Reset ESP on 'E'
  else if(c==0x57) // on 'W'...
  {
    extern WiFiManager wm;
    wm.resetSettings(); // Delete WiFi credentials
    ESP.restart(); // And reset
  }
  else if(c==0x5A) // on 'Z'
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
  USER_CMD cmd = static_cast<USER_CMD>(c);
  pic_uart_tx_userdata(cmd, 0);
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
