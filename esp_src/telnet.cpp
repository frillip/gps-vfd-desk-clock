#include "telnet.h"

ESPTelnetStream telnet;
extern Stream *last_output_stream;

char telnet_buffer[TELNET_BUFFER_LENGTH] = {0};
uint8_t telnet_buffer_offset = 0;
char telnet_cmd_buf[TELNET_BUFFER_LENGTH] = {0};
char telnet_arg_buf[TELNET_BUFFER_LENGTH] = {0};
char telnet_last_rx_char;

void telnet_init(void)
{
  telnet.begin(TELNET_PORT, false);
  telnet.onConnect(onTelnetConnect);
  telnet.onConnectionAttempt(onTelnetConnectionAttempt);
  telnet.onReconnect(onTelnetReconnect);
  telnet.onDisconnect(onTelnetDisconnect);
  //telnet.onInputReceived(onTelnetInput);
}

// (optional) callback functions for telnet events
void onTelnetConnect(String ip)
{
  Serial.printf("\n- Telnet: %s connected\n", ip);
  telnet.printf("\nWelcome %s\n", telnet.getIP());
  telnet.printf("(type 'quit' or 'exit' to disconnect.)\n");
  last_output_stream = &telnet; 
}

void onTelnetDisconnect(String ip)
{
  Serial.printf("- Telnet: %s disconnected\n", ip);
  last_output_stream = &Serial; 
}

void onTelnetReconnect(String ip)
{
  Serial.printf("\n- Telnet: %s reconnected\n", ip);
  last_output_stream = &telnet; 
}

void onTelnetConnectionAttempt(String ip)
{
  Serial.printf("- Telnet: %s tried to connected\n", ip);
}

bool telnet_char_available(void)
{
  if(telnet.available()) return 1;
  return 0;
}


void telnet_console_task(void) // Not a true stream, only triggered on new line
{
  if(telnet.available())
  {
    char c = telnet.read();

    if(c == 0x0A) // new line char
    {
      USER_CMD_TYPE cmd_type = USER_CMD_TYPE_NONE;
      USER_CMD exec = USER_CMD_NONE;

      int i=0;
      while(i<telnet_buffer_offset)
      {
        if(telnet_buffer[i] == 0x20)
        {
          memcpy(telnet_cmd_buf, telnet_buffer, i);
          memcpy(telnet_arg_buf, telnet_buffer + i + 1, telnet_buffer_offset - i - 1);
          while(telnet_arg_buf[0] == 0x20) // remove excess spaces
          {
            memmove(telnet_arg_buf, telnet_arg_buf+1, telnet_buffer_offset - i - 1);
          }
          i=telnet_buffer_offset;
        }
        i++;
      }
      if(i==telnet_buffer_offset)
      {
        memcpy(telnet_cmd_buf, telnet_buffer, i);
      }
      /*
      if(telnet_buffer_offset==0 && telnet_last_rx_char==0x0A)
      {
        serial_console_print_info(&telnet);
      }
      */

      // Check if it's an exit command
      if(strcmp(telnet_cmd_buf, "quit") == 0) telnet.disconnectClient();
      else if(strcmp(telnet_cmd_buf, "exit") == 0) telnet.disconnectClient();

      cmd_type = serial_console_check_1(telnet_cmd_buf);
      switch(cmd_type)
      {
        case USER_CMD_TYPE_ESP:
          exec = serial_console_check_2_esp(telnet_cmd_buf);
          break;

        case USER_CMD_TYPE_PIC:
          exec = serial_console_check_2_pic(telnet_cmd_buf);
          break;

        case USER_CMD_TYPE_RST:
          exec = serial_console_check_2_rst(telnet_cmd_buf);
          break;

        case USER_CMD_TYPE_HELP:
          exec = serial_console_check_2_help(telnet_cmd_buf);
          break;

        case USER_CMD_TYPE_NONE:
          if(telnet_buffer_offset==0) serial_console_print_info(&telnet);
          break;
      }


      if((exec != USER_CMD_NONE) && (exec != USER_CMD_HELP))
      {
        serial_console_exec(&telnet, exec, telnet_arg_buf);
      }
      else if(exec == USER_CMD_HELP)
      {
        serial_console_help(&telnet);
      }

      memset(telnet_buffer, 0, TELNET_BUFFER_LENGTH);
      memset(telnet_cmd_buf, 0, TELNET_BUFFER_LENGTH);
      memset(telnet_arg_buf, 0, TELNET_BUFFER_LENGTH);
      telnet_buffer_offset = 0;
      telnet.printf("\n> ");
    }
    else if(c == 0x7F) // Backspace
    {
      telnet_buffer[telnet_buffer_offset] = 0x00;
      if(telnet_buffer_offset > 0) telnet_buffer_offset--;
    }
    else if(c == 0x0D) return;
    else
    {
      telnet_buffer[telnet_buffer_offset] = c;
      telnet_buffer_offset++;
      if(telnet_buffer_offset > TELNET_BUFFER_LENGTH -1)
      {
        memset(telnet_buffer, 0, TELNET_BUFFER_LENGTH);
        telnet_buffer_offset = 0;
        telnet.print("\n> ");
      }
    }

    telnet_last_rx_char = c;
  }
}