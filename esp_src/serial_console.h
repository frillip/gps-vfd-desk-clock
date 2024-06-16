#include <Arduino.h>
#include <stdint.h>
#include <time.h>
#include <ezTime.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include "enums.h"
#include "serial_proto.h"
#include "esp_pps.h"
#include "gnss_pps_esp.h"
#include "pic.h"
#include <HardwareSerial.h>

#define LOCAL_TIME_PRINT_DELAY 50
#define SERIAL_CONSOLE_BUFFER_LENGTH 100

#define DEBUG_UART  0
#define DEBUG_BAUD  115200

void serial_console_init();
bool serial_console_char_available(void);
void serial_console_task(void);
void serial_console_exec(char c);

void serial_console_second_changed(uint32_t millis);
bool serial_console_print_local_available(void);
void serial_console_print_local(void);