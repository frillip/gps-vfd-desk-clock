#ifndef PIC_H
#define	PIC_H

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
#include "telnet.h"
#include "pic_bootloader.h"

#define PIC_UART    2
#define PIC_BAUD    115200
#define PIC_RXD     13
#define PIC_TXD     12
#define PIC_PPS_PIN 27

#define PIC_TIMEOUT_LIMIT 300 // in 0.01s counts

#define PIC_ESP_SET_LATENCY_US 300

#define PIC_OUTPUT_BUFFER_LENGTH 100

#define PIC_UART_OUTPUT_TIMEOUT 100

#define PIC_TZ_BT_PIN 32

void pic_init(void);
bool pic_is_detected(void);
void pic_timeout_incr(void);
void pic_pps_init(void);
void ARDUINO_ISR_ATTR pic_pps_in(void);
void print_pic_pps_offset(Stream *output);
void print_pic_pps_relative_offset(Stream *output);

void pic_uart_init(void);
bool pic_uart_char_available(void);
void pic_uart_rx(void);
bool pic_uart_output_finished(void);
bool pic_output_buffer_empty(void);
void pic_flush_output_buffer(void);
void pic_copy_buffer(PIC_MESSAGE_TYPE message);

PIC_MESSAGE_TYPE pic_check_incoming(void);
void pic_data_task(void);

void pic_process_time(void);
void print_local_time(Stream *output, time_t now);
void print_local_offset(Stream *output, int32_t total_offset);
void print_iso8601_string(Stream *output, time_t time);
void print_pic_time(Stream *output);
void print_clock_source(Stream *output, CLOCK_SOURCE source);

void print_gnss_fix(Stream *output, UBX_NAV_STATUS_GPSFIX fix_status);
void pic_process_gnss(void);
void print_gnss_data(Stream *output);

void pic_process_offset(void);
void print_offset_data(Stream *output);

void pic_process_net(void);

void pic_process_rtc(void);
void print_rtc_data(Stream *output);

void pic_process_sensor(void);
void print_veml_data(Stream *output);
void print_bme_data(Stream *output);

void pic_process_display(void);
void print_sync_state_machine(Stream *output);
void sync_state_print(Stream *output, CLOCK_SYNC_STATUS sync_state);

void pic_uart_tx_timedata(void);
void pic_uart_tx_netdata(void);
void pic_uart_tx_rtcdata(void);
void pic_uart_tx_sensordata(void);
void pic_uart_tx_displaydata(void);
void pic_uart_tx_userdata(USER_CMD cmd, uint32_t arg, Stream *output);

void pic_enter_bootloader(Stream *output);
void pic_exit_bootloader(Stream *output);

#endif	/* PIC_H */