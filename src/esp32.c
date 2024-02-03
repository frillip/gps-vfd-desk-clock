#include "esp32.h"

bool esp_detected = 0;
bool esp_ntp_valid = 0;

uint8_t uart1_offset = 0;
char uart1_check_buffer[ESP_CHECK_BUFFER_SIZE] = {0};
char uart1_string_buffer[ESP_STRING_BUFFER_SIZE] = {0};
ESP_MESSAGE_TYPE esp_incoming = GNSS_NONE;
ESP_MESSAGE_TYPE esp_waiting = GNSS_NONE;

char esp_time_buffer[ESP_TIME_LENGTH] = {0};
char esp_time_string[ESP_CHECK_BUFFER_SIZE] = {ESP_UART_HEADER, ESP_UART_TYPE_TX, ESP_UART_DATATYPE_TIMEDATA};
bool esp_wifi_status = 0;
bool esp_ntp_status = 0;
time_t esp_ntp_time = 0;
uint16_t esp_ntp_milliseconds = 0;
int8_t esp_ntp_offset = 0;

char esp_net_buffer[ESP_NET_LENGTH] = {0};
char esp_net_string[ESP_CHECK_BUFFER_SIZE] = {ESP_UART_HEADER, ESP_UART_TYPE_TX, ESP_UART_DATATYPE_NETDATA};
//bool esp_wifi_status = 0;
//bool esp_ntp_status = 0
bool esp_pps_sync = 0;
bool esp_scheduler_sync = 0;
time_t esp_ntp_last_update = 0;
uint16_t esp_ntp_interval_count = 0;
uint8_t esp_dst_flags = 0;

char esp_rtc_buffer[ESP_RTC_LENGTH] = {0};
char esp_rtc_string[ESP_CHECK_BUFFER_SIZE] = {ESP_UART_HEADER, ESP_UART_TYPE_TX, ESP_UART_DATATYPE_RTCDATA};
time_t esp_rtc_time = 0;

char esp_sensor_buffer[ESP_SENSOR_LENGTH] = {0};
char esp_sensor_string[ESP_CHECK_BUFFER_SIZE] = {ESP_UART_HEADER, ESP_UART_TYPE_TX, ESP_UART_DATATYPE_SENSORDATA};
uint16_t esp_sensor_temp = 0;
uint16_t esp_sensor_pres = 0;
uint16_t esp_sensor_hum = 0;
uint16_t esp_sensor_lux = 0;

char esp_display_buffer[ESP_DISPLAY_LENGTH] = {0};
char esp_display_string[ESP_CHECK_BUFFER_SIZE] = {ESP_UART_HEADER, ESP_UART_TYPE_TX, ESP_UART_DATATYPE_DISPLAYDATA};
uint16_t esp_brightness = 0;
uint8_t esp_display_state = 0;
uint8_t esp_menu_state = 0;

time_t esp;
time_t ntp;
extern time_t utc;

void esp_ntp_init(void)
{
    UART1_Initialize();
    memset(uart1_string_buffer, 0, ESP_STRING_BUFFER_SIZE);
    memset(uart1_check_buffer, 0, ESP_CHECK_BUFFER_SIZE);
    UART1_SetRxInterruptHandler(esp_rx);
}

void esp_rx(void)
{
    char rx_char = 0;
    
    while(UART1_IsRxReady())
    {
        rx_char = UART1_Read();
        
        ui_uart1_input(rx_char);
    }
}

void esp_ntp_set_calendar(void)
{
    utc = ntp;
}

void esp_process_time(void)
{
    if(!esp_detected) esp_detected = 1;
    esp_wifi_status = esp_net_buffer[3]&0x01;
    esp_ntp_status = (esp_net_buffer[3]>1)&0x01;
    memcpy(&esp_ntp_time, esp_time_buffer+4, 4);
    memcpy(&esp_ntp_milliseconds, esp_time_buffer+8, 2);
    esp_ntp_offset = esp_time_buffer[10];
    if(esp_wifi_status && esp_ntp_status && esp_ntp_time)
    {
        esp_ntp_valid = 1;
        ntp = esp_ntp_time;
        esp = ntp;
    }
}

void esp_process_net(void)
{
    //esp_wifi_status = esp_net_buffer[3]&0x01;
    //esp_ntp_status = (esp_net_buffer[3]>1)&0x01;
    esp_pps_sync = (esp_net_buffer[3]>2)&0x01;
    esp_scheduler_sync = (esp_net_buffer[3]>3)&0x01;
    memcpy(&esp_ntp_last_update, esp_net_buffer+4, 4);
    memcpy(&esp_ntp_interval_count, esp_net_buffer+8, 2);
    esp_dst_flags = esp_net_buffer[10];
}

void esp_process_rtc(void)
{
    memcpy(&esp_rtc_time, esp_rtc_buffer+3, 4);
    if(!esp_ntp_valid) esp = esp_rtc_time;
}

void esp_process_sensor(void)
{
    memcpy(&esp_sensor_temp, esp_sensor_buffer+3, 2);
    memcpy(&esp_sensor_pres, esp_sensor_buffer+5, 2);
    memcpy(&esp_sensor_hum, esp_sensor_buffer+7, 2);
    memcpy(&esp_sensor_lux, esp_sensor_buffer+9, 2);
}

void esp_process_display(void)
{
    memcpy(&esp_brightness, esp_display_buffer+3, 2);
    esp_display_state = esp_display_buffer[5];
    esp_menu_state = esp_display_buffer[6];
}

void esp_process_user(void)
{
    
}
