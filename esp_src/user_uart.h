#ifndef USER_UART_H
#define	USER_UART_H

#include <Arduino.h>
#include <stdint.h>
#include <time.h>
#include <ezTime.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include "serial_console.h"
#include <HardwareSerial.h>

#define DEBUG_UART  0
#define DEBUG_BAUD  115200

#define LOCAL_TIME_PRINT_DELAY 50
#define USER_UART_BUFFER_LENGTH 100

void user_uart_init();
bool user_uart_char_available(void);
void user_uart_task(void);

#endif	/* USER_UART_H */
