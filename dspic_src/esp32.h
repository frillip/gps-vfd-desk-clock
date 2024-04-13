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
#include "gnss_pps.h"
#include "mcc_generated_files/pin_manager.h"
#include "mcc_generated_files/uart2.h"
#include "mcc_generated_files/uart1.h"
#include "mcc_generated_files/system.h"
#include "mcc_generated_files/clock.h"
#include "pic_pps.h"
#include "sync_state.h"
#include "ui.h"
#include "ublox_ubx.h"
#include "../common/enums.h"
#include "../common/serial_proto.h"
    
#define ESP_DETECT_LIMIT 300
#define ESP_NTP_LIMIT 500
    
#define ESP_NTP_OFFSET_MAX_MS 30 // Achievable accuracy by NTP on ESP32 is usually around 30ms
#define ESP_NTP_OFFSET_MIN_MS -30
    
#define esp_pps_input() (_RB15)
    
void esp_ntp_init(void);
void esp_rx(void);
void esp_copy_buffer(ESP_MESSAGE_TYPE esp_waiting);
ESP_MESSAGE_TYPE esp_check_incoming(void);

bool ntp_is_calendar_sync(time_t utc);
void esp_ntp_set_calendar(void);
void esp_start_sync_timer(void);
void esp_stop_sync_timer(void);
void esp_reset_sync_timer(void);
void esp_store_sync_timer(void);
void esp_ioc_handler(void);
void print_esp_offset(void);

void esp_process_time(void);
void esp_process_net(void);
void esp_process_rtc(void);
void esp_process_sensor(void);
void esp_process_display(void);
void esp_process_user(void);
void esp_data_task(void);

void esp_tx(void *buffer, uint16_t len);
void esp_tx_time(void);
void esp_tx_gnss(void);
void esp_tx_offset(void);
void esp_tx_net(void);
void esp_tx_rtc(void);
void esp_tx_sensor(void);
void esp_tx_display(void);
void esp_tx_user_start(void);
void esp_tx_user_stop(void);

void print_esp_data(void);

#ifdef	__cplusplus
}
#endif

#endif	/* ESP32_H */

