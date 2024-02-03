/* 
 * File:   esp32.h
 * Author: Frillip
 *
 * Created on 28 January 2024, 20:17
 */

#ifndef ESP32_H
#define	ESP32_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "ui.h"

#define ESP_UART_HEADER 0x83
#define ESP_UART_TYPE_TX 0x65
#define ESP_UART_TYPE_RX 0x70
#define ESP_UART_DATATYPE_TIMEDATA 0x00
#define ESP_UART_DATATYPE_GPSDATA 0x10
#define ESP_UART_DATATYPE_OFFSETDATA 0x20
#define ESP_UART_DATATYPE_NETDATA 0x30
#define ESP_UART_DATATYPE_RTCDATA 0x40
#define ESP_UART_DATATYPE_SENSORDATA 0x50
#define ESP_UART_DATATYPE_DISPLAYDATA 0x60
#define ESP_UART_DATATYPE_MISCDATA 0x70
#define ESP_UART_DATATYPE_USERDATA 0x80

#define ESP_STRING_BUFFER_SIZE 100
#define ESP_CHECK_BUFFER_SIZE 3
#define ESP_TIME_LENGTH 11
#define ESP_GPS_LENGTH 12
#define ESP_OFFSET_LENGTH 16
#define ESP_NET_LENGTH 11
#define ESP_RTC_LENGTH 7
#define ESP_SENSOR_LENGTH 11
#define ESP_DISPLAY_LENGTH 7
#define ESP_USER_LENGTH 20
    
#define ESP_DETECT_LIMIT 300
#define ESP_NTP_LIMIT 500
    
typedef enum
{
    ESP_TIME,
    ESP_GPS,
    ESP_OFFSET,
    ESP_NET,
    ESP_RTC,
    ESP_SENSOR,
    ESP_DISPLAY,
    ESP_USER,
} ESP_MESSAGE_TYPE;
    
void esp_ntp_init(void);
void esp_rx(void);
void esp_ntp_set_calendar(void);
void esp_process_time(void);
void esp_process_net(void);
void esp_process_rtc(void);
void esp_process_sensor(void);
void esp_process_display(void);
void esp_process_user(void);


#ifdef	__cplusplus
}
#endif

#endif	/* ESP32_H */

