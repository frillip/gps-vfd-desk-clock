#include "serial_console.h"

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
  extern WiFiManager wm;
  if(Serial.available())
  {
    char c = Serial.read();
    Serial.print(c);
    if(c==0x45) ESP.restart(); // Reset ESP on 'E'
    else if(c==0x57) // on 'W'...
    {
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
}

uint32_t local_print_millis = 0;

void serial_console_second_changed(uint32_t millis)
{
  local_print_millis = millis;
}

bool serial_console_print_local_available(void)
{
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
