#include <Arduino.h>
#include <stdint.h>
#include <time.h>
#include <ezTime.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include "enums.h"
#include "serial_proto.h"
#include "scheduler.h"
#include "esp_pps.h"
#include <HardwareSerial.h>
#include <esp32-hal-timer.h>

#define PIC_UART    2
#define PIC_BAUD    115200
#define PIC_RXD     13
#define PIC_TXD     12
#define PIC_PPS_PIN 27

#define PIC_TIMEOUT_LIMIT 300 // in 0.01s counts

#define PIC_ESP_SET_LATENCY_US 300

void pic_init(void);
bool pic_is_detected(void);
void pic_timeout_incr(void);
void pic_pps_init(void);
void ARDUINO_ISR_ATTR pic_pps_in(void);
void print_pic_pps_offset(void);

void pic_uart_init(void);
bool pic_uart_char_available(void);
void pic_uart_rx(void);
void pic_copy_buffer(PIC_MESSAGE_TYPE message);

PIC_MESSAGE_TYPE pic_check_incoming(void);
void pic_data_task(void);

void pic_process_time(void);
void print_local_time(time_t now);
void print_local_offset(int32_t total_offset);
void print_iso8601_string(time_t time);
void print_pic_time(void);
void print_clock_source(CLOCK_SOURCE source);

void print_gnss_fix(UBX_NAV_STATUS_GPSFIX fix_status);
void pic_process_gnss(void);
void print_gnss_data(void);

void pic_process_offset(void);
void print_offset_data(void);

void pic_process_net(void);

void pic_process_rtc(void);
void pic_print_rtc(void);

void pic_process_sensor(void);
void print_veml_data(void);
void print_bme_data(void);

void pic_process_display(void);
void print_sync_state_machine(void);
void sync_state_print(CLOCK_SYNC_STATUS sync_state);

void pic_uart_tx_timedata(void);
void pic_uart_tx_netdata(void);
void pic_uart_tx_rtcdata(void);
void pic_uart_tx_sensordata(void);
void pic_uart_tx_displaydata(void);
void pic_uart_tx_userdata(USER_CMD cmd, uint32_t arg);