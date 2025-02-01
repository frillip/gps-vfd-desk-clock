#include "user_uart.h"

char user_uart_buffer[USER_UART_BUFFER_LENGTH] = {0};
uint8_t user_uart_buffer_offset = 0;
char user_uart_cmd_buf[USER_UART_BUFFER_LENGTH] = {0};
char user_uart_arg_buf[USER_UART_BUFFER_LENGTH] = {0};
char last_rx_char;

void user_uart_init()
{
  Serial.begin(DEBUG_BAUD);
}

bool user_uart_char_available(void)
{
  return Serial.available();
}

void user_uart_task(void)
{
  if(Serial.available())
  {
    char c = Serial.read();
    if(c == 0x7F)
    {
      // Only print backspace if we have something in the buffer
      if(user_uart_buffer_offset > 0) Serial.print(c);
    }
    else
    {
      Serial.print(c);
    }

    if(c == 0x0A || c == 0x0D) // new line char
    {
      USER_CMD_TYPE cmd_type = USER_CMD_TYPE_NONE;
      USER_CMD exec = USER_CMD_NONE;

      // Just exec first character in buffer for now
      // To do: implement two stage buffer check and proper commands
      // Stage 1: check first 4 bytes for 'esp-' or 'pic-' or first 5 bytes for 'reset'
      // Stage 2: check against entire command string in serial_cmds.txt
      // Stage 3: run command and parse options (if required)

      Serial.print(user_uart_buffer);

      int i=0;
      while(i<user_uart_buffer_offset)
      {
        if(user_uart_buffer[i] == 0x20)
        {
          memcpy(user_uart_cmd_buf, user_uart_buffer, i);
          memcpy(user_uart_arg_buf, user_uart_buffer + i + 1, user_uart_buffer_offset - i - 1);
          while(user_uart_arg_buf[0] == 0x20) // remove excess spaces
          {
            memmove(user_uart_arg_buf, user_uart_arg_buf+1, user_uart_buffer_offset - i - 1);
          }
          i=user_uart_buffer_offset;
        }
        i++;
      }
      if(i==user_uart_buffer_offset)
      {
        memcpy(user_uart_cmd_buf, user_uart_buffer, i);
      }

      if(user_uart_buffer_offset==0 && last_rx_char!=0x0A)
      {
        serial_console_print_info(&Serial);
      }

      cmd_type = serial_console_check_1(user_uart_cmd_buf);
      switch(cmd_type)
      {
        case USER_CMD_TYPE_ESP:
          exec = serial_console_check_2_esp(user_uart_cmd_buf);
          break;

        case USER_CMD_TYPE_PIC:
          exec = serial_console_check_2_pic(user_uart_cmd_buf);
          break;

        case USER_CMD_TYPE_RST:
          exec = serial_console_check_2_rst(user_uart_cmd_buf);
          break;

        case USER_CMD_TYPE_HELP:
          exec = serial_console_check_2_help(user_uart_cmd_buf);
          break;

        case USER_CMD_TYPE_NONE:
          if(user_uart_buffer_offset==1) serial_console_print_info(&Serial);
          break;
      }


      if((exec != USER_CMD_NONE) && (exec != USER_CMD_HELP))
      {
        serial_console_exec(&Serial, exec, user_uart_arg_buf);
      }
      else if(exec == USER_CMD_HELP)
      {
        serial_console_help(&Serial);
      }

      memset(user_uart_buffer, 0, USER_UART_BUFFER_LENGTH);
      memset(user_uart_cmd_buf, 0, USER_UART_BUFFER_LENGTH);
      memset(user_uart_arg_buf, 0, USER_UART_BUFFER_LENGTH);
      user_uart_buffer_offset = 0;
      Serial.print("> ");
    }
    else if(c == 0x7F) // Backspace
    {
      user_uart_buffer[user_uart_buffer_offset] = 0x00;
      if(user_uart_buffer_offset > 0) user_uart_buffer_offset--;
    }
    //else if(c == 0x0D) return;
    else
    {
      user_uart_buffer[user_uart_buffer_offset] = c;
      user_uart_buffer_offset++;
      if(user_uart_buffer_offset > USER_UART_BUFFER_LENGTH -1)
      {
        memset(user_uart_buffer, 0, USER_UART_BUFFER_LENGTH);
        user_uart_buffer_offset = 0;
        Serial.print("\n> ");
      }
    }

    last_rx_char = c;
  }
}