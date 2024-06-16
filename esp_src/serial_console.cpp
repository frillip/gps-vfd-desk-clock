#include "serial_console.h"

char console_buffer[SERIAL_CONSOLE_BUFFER_LENGTH] = {0};
uint8_t console_buffer_offset = 0;

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

    console_buffer[console_buffer_offset] = c;

    if(c == 0x0A || c == 0x0D) // new line char
    {
      // Just exec first character in buffer for now
      // To do: implement two stage buffer check and proper commands
      // Stage 1: check first 4 bytes for 'esp-' or 'pic-' or first 5 bytes for 'reset'
      // Stage 2: check against entire command string in serial_cmds.txt
      // Stage 3: run command and parse options (if required)
      serial_console_exec(console_buffer[0]);
      console_buffer_offset = 0;      
    }
    else
    {
      console_buffer_offset++;
      if(console_buffer_offset > SERIAL_CONSOLE_BUFFER_LENGTH -1)
      {
        console_buffer_offset = 0;
      }
    }
  }
}

void serial_console_exec(char c)
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
