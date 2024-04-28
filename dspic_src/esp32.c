#include "esp32.h"

bool esp_detected = 0;
bool esp_ntp_valid = 0;

uint8_t esp_offset = 0;
char esp_check_buffer[sizeof(SERIAL_PROTO_HEADER)] = {0};
char esp_string_buffer[SERIAL_PROTO_STRING_BUFFER_SIZE] = {0};
ESP_MESSAGE_TYPE esp_incoming = ESP_NONE;
ESP_MESSAGE_TYPE esp_waiting = ESP_NONE;
uint8_t esp_bytes_remaining = 0;

SERIAL_PROTO_DATA_ESP_TIME esp_time_buffer;
bool esp_time_waiting = 0;
SERIAL_PROTO_HEADER esp_time_string = { .magic = SERIAL_PROTO_HEADER_MAGIC, .type = SERIAL_PROTO_TYPE_ESP_TX, .datatype = SERIAL_PROTO_DATATYPE_TIMEDATA};
bool esp_wifi_status = 0;
bool esp_ntp_status = 0;
bool esp_pps_sync = 0;
bool esp_scheduler_sync = 0;
time_t esp_ntp_time = 0;
uint16_t esp_ntp_milliseconds = 0;
int8_t esp_ntp_offset = 0;

SERIAL_PROTO_DATA_ESP_NET esp_net_buffer;
bool esp_net_waiting = 0;
SERIAL_PROTO_HEADER esp_net_string = { .magic = SERIAL_PROTO_HEADER_MAGIC, .type = SERIAL_PROTO_TYPE_ESP_TX, .datatype = SERIAL_PROTO_DATATYPE_NETDATA};
//bool esp_wifi_status = 0;
//bool esp_ntp_status = 0
//bool esp_pps_sync = 0;
//bool esp_scheduler_sync = 0;
time_t esp_ntp_last_update = 0;
uint16_t esp_ntp_interval_count = 0;
uint8_t esp_dst_flags = 0;

SERIAL_PROTO_DATA_ESP_RTC esp_rtc_buffer;
bool esp_rtc_waiting = 0;
SERIAL_PROTO_HEADER esp_rtc_string = { .magic = SERIAL_PROTO_HEADER_MAGIC, .type = SERIAL_PROTO_TYPE_ESP_TX, .datatype = SERIAL_PROTO_DATATYPE_RTCDATA};
time_t esp_rtc_time = 0;

SERIAL_PROTO_DATA_ESP_SENSOR esp_sensor_buffer;
bool esp_sensor_waiting = 0;
SERIAL_PROTO_HEADER esp_sensor_string = { .magic = SERIAL_PROTO_HEADER_MAGIC, .type = SERIAL_PROTO_TYPE_ESP_TX, .datatype = SERIAL_PROTO_DATATYPE_SENSORDATA};
uint16_t esp_sensor_lux = 0;
uint16_t esp_sensor_temp = 0;
uint16_t esp_sensor_pres = 0;
uint16_t esp_sensor_hum = 0;

SERIAL_PROTO_DATA_ESP_DISPLAY esp_display_buffer;
bool esp_display_waiting = 0;
SERIAL_PROTO_HEADER esp_display_string = { .magic = SERIAL_PROTO_HEADER_MAGIC, .type = SERIAL_PROTO_TYPE_ESP_TX, .datatype = SERIAL_PROTO_DATATYPE_DISPLAYDATA};
uint16_t esp_brightness = 0;
bool esp_brightness_updated = 0;
uint8_t esp_display_state = 0;
uint8_t esp_menu_state = 0;

SERIAL_PROTO_DATA_ESP_USER esp_user_buffer;
bool esp_user_waiting = 0;
SERIAL_PROTO_HEADER esp_user_string = { .magic = SERIAL_PROTO_HEADER_MAGIC, .type = SERIAL_PROTO_TYPE_ESP_TX, .datatype = SERIAL_PROTO_DATATYPE_USERDATA};

time_t esp;
time_t ntp;
extern time_t utc;
extern time_t gnss;
extern time_t power_on_time;
extern uint32_t total_oc_seq_count;

int32_t esp_time_offset_counter = 0;
int32_t esp_time_offset = 0;
bool esp_time_offset_stale = 0;

extern bool gnss_detected;
extern bool gnss_fix;
extern bool pps_sync;
extern bool scheduler_sync;

bool ntp_calendar_sync = 0;

bool esp_gnss_data_updated = 0;

void esp_ntp_init(void)
{
    UART1_Initialize();
    memset(esp_string_buffer, 0, SERIAL_PROTO_STRING_BUFFER_SIZE);
    memset(esp_check_buffer, 0, sizeof(SERIAL_PROTO_HEADER));
    UART1_SetRxInterruptHandler(esp_rx);
    CN_SetInterruptHandler(esp_ioc_handler);
    esp_start_sync_timer();
}

void esp_rx(void)
{
    char rx_char = 0;
    
    while(UART1_IsRxReady())
    {
        rx_char = UART1_Read();
        
        memmove(esp_check_buffer, esp_check_buffer+1, sizeof(SERIAL_PROTO_HEADER)-1);
        esp_check_buffer[sizeof(SERIAL_PROTO_HEADER)-1] = rx_char;
        
        if(esp_incoming!=ESP_NONE)
        {
            if(!esp_detected) esp_time_offset_counter = 0; // Reset this if redetecting
            esp_detected = 1;
            esp_string_buffer[esp_offset] = rx_char;
            esp_offset++;
            esp_bytes_remaining--;
            // If we've reached the end of our buffer
            // then we're getting unknown messages
            // Discard anything that doesn't fit
            // and flag the incoming message as waiting
            if(esp_offset>=SERIAL_PROTO_STRING_BUFFER_SIZE-1 || esp_bytes_remaining==0)
            {
                esp_waiting = esp_incoming;
                esp_incoming = ESP_NONE;
            }
        }
        
        // Do we have a message that's finished coming in?
        if(esp_waiting!=ESP_NONE)
        {
            // if so, copy it to the right buffer
            esp_copy_buffer(esp_waiting);
            // Then reset the message buffer
            memset(esp_string_buffer, 0, SERIAL_PROTO_STRING_BUFFER_SIZE);
            // Reset the message waiting flag
            esp_waiting=ESP_NONE;
        }
        
        ESP_MESSAGE_TYPE esp_check_res = esp_check_incoming();
        if(esp_check_res != ESP_NONE)
        {
            esp_waiting = esp_incoming;
            esp_incoming = esp_check_res;
            esp_offset = sizeof(SERIAL_PROTO_HEADER);
            switch (esp_incoming)
            {
                case ESP_TIME:
                    esp_bytes_remaining = sizeof(SERIAL_PROTO_DATA_ESP_TIME) - sizeof(SERIAL_PROTO_HEADER);
                    memcpy(esp_string_buffer, esp_check_buffer, sizeof(SERIAL_PROTO_HEADER));
                    break;
        
                case ESP_NET:
                    esp_bytes_remaining = sizeof(SERIAL_PROTO_DATA_ESP_NET) - sizeof(SERIAL_PROTO_HEADER);
                    memcpy(esp_string_buffer, esp_check_buffer, sizeof(SERIAL_PROTO_HEADER));
                    break;

                case ESP_RTC:
                    esp_bytes_remaining = sizeof(SERIAL_PROTO_DATA_ESP_RTC) - sizeof(SERIAL_PROTO_HEADER);
                    memcpy(esp_string_buffer, esp_check_buffer, sizeof(SERIAL_PROTO_HEADER));
                    break;
        
                case ESP_SENSOR:
                    esp_bytes_remaining = sizeof(SERIAL_PROTO_DATA_ESP_SENSOR) - sizeof(SERIAL_PROTO_HEADER);
                    memcpy(esp_string_buffer, esp_check_buffer, sizeof(SERIAL_PROTO_HEADER));
                    break;
        
                case ESP_DISPLAY:
                    esp_bytes_remaining = sizeof(SERIAL_PROTO_DATA_ESP_DISPLAY) - sizeof(SERIAL_PROTO_HEADER);
                    memcpy(esp_string_buffer, esp_check_buffer, sizeof(SERIAL_PROTO_HEADER));
                    break;

                case ESP_USER:
                    esp_bytes_remaining = sizeof(SERIAL_PROTO_DATA_ESP_USER) - sizeof(SERIAL_PROTO_HEADER);
                    memcpy(esp_string_buffer, esp_check_buffer, sizeof(SERIAL_PROTO_HEADER));
                    break;
        
                default:
                    break;
            }
        }
    }
}

void esp_copy_buffer(ESP_MESSAGE_TYPE message)
{
    switch (message)
    {
        case ESP_TIME:
            memcpy(esp_time_buffer.raw, esp_string_buffer, sizeof(esp_time_buffer));
            esp_time_waiting=1;
            break;

        case ESP_NET:
            memcpy(esp_net_buffer.raw, esp_string_buffer, sizeof(esp_net_buffer));
            esp_net_waiting=1;
            break;

        case ESP_RTC:
            memcpy(esp_rtc_buffer.raw, esp_string_buffer, sizeof(esp_rtc_buffer));
            esp_rtc_waiting=1;
            break;

        case ESP_SENSOR:
            memcpy(esp_sensor_buffer.raw, esp_string_buffer, sizeof(esp_sensor_buffer));
            esp_sensor_waiting=1;
            break;

        case ESP_DISPLAY:
            memcpy(esp_display_buffer.raw, esp_string_buffer, sizeof(esp_display_buffer));
            esp_display_waiting=1;
            break;

        case ESP_USER:
            memcpy(esp_user_buffer.raw, esp_string_buffer, sizeof(esp_user_buffer));
            esp_user_waiting=1;
            break;

        default:
            esp_waiting=ESP_NONE;
            break;
    }
}

ESP_MESSAGE_TYPE esp_check_incoming(void)
{
    if(memcmp(esp_check_buffer, &esp_time_string, sizeof(SERIAL_PROTO_HEADER))==0) return ESP_TIME;
    if(memcmp(esp_check_buffer, &esp_net_string, sizeof(SERIAL_PROTO_HEADER))==0) return ESP_NET;
    if(memcmp(esp_check_buffer, &esp_rtc_string, sizeof(SERIAL_PROTO_HEADER))==0) return ESP_RTC;
    if(memcmp(esp_check_buffer, &esp_sensor_string, sizeof(SERIAL_PROTO_HEADER))==0) return ESP_SENSOR;
    if(memcmp(esp_check_buffer, &esp_display_string, sizeof(SERIAL_PROTO_HEADER))==0) return ESP_DISPLAY;
    if(memcmp(esp_check_buffer, &esp_user_string, sizeof(SERIAL_PROTO_HEADER))==0) return ESP_USER;
    return ESP_NONE;
}

bool ntp_is_calendar_sync(time_t utc)
{
    return utc==ntp;
}

void esp_ntp_set_calendar(void)
{
#ifdef DEBUG_MESSAGES
    printf("NTP calendar sync\r\nTime is now: ");
    ui_print_iso8601_string(ntp);
    printf("\r\n");
#endif
    
    utc = ntp;
    if(!power_on_time) power_on_time = utc - total_oc_seq_count;
    ntp_calendar_sync = 1;
}

void esp_start_sync_timer(void)
{
    //TMR3 0; 
    TMR3 = 0x00;
    //Period = 0.001 s; Frequency = 40000000 Hz; PR2 4999; 
    PR3 = 0x1387;
    //TCKPS 1:8; T32 16 Bit; TON enabled; TSIDL disabled; TCS FOSC/2; TGATE disabled; 
    T3CON = 0x8010;
    // Clear T3 interrupt flag
    IFS0bits.T3IF = false;
    // Enable T2 interrupts
    IEC0bits.T3IE = true;
    // Set interrupt priority
    IPC2bits.T3IP = 1;
}

void esp_stop_sync_timer(void)
{
    TMR3 = 0x00;
    T3CON = 0x0010;
    IFS0bits.T3IF = false;
    IEC0bits.T3IE = false;
    esp_time_offset_counter = 0;
}

time_t ntp_old = 0;

void esp_reset_sync_timer(void)
{
    TMR3 = 0x00;
    if(ntp!=ntp_old) esp_time_offset_counter = 0;
    else esp_time_offset_stale = 1;
    ntp_old = ntp;
}

void esp_store_sync_timer(void)
{
    esp_time_offset = esp_time_offset_counter;
    esp_time_offset_stale = 0;
}

void esp_ioc_handler(void)
{
    if(esp_pps_input())
    {
        esp_store_sync_timer();
        if(esp_detected)
        {
            esp++;
            if(esp_ntp_valid)
            {
                ntp++;
            }
        }
    }
}

void print_esp_offset(void)
{
    if(esp_time_waiting) esp_process_time();
    int32_t esp_time_offset_display = esp_time_offset;
    while(esp_time_offset_display>500)
    {
        esp_time_offset_display -= 1000;
    }
    if(esp_ntp_valid && esp_pps_sync && esp_scheduler_sync)
    {
        printf("NTP ");
    }
    else
    {
        printf("ESP ");
    }
    if(gnss_detected && gnss_fix && scheduler_sync)
    {
        esp_time_offset_display += ((ntp - gnss)*1000);
        printf("%lims off GNSS", esp_time_offset_display);
    }
    else
    {
        printf("%lims off OC", esp_time_offset_display);
    }
    if(esp_time_offset_stale) printf(" - stale");
    printf("\r\n");
}

void __attribute__ ( ( interrupt, no_auto_psv ) ) _T3Interrupt (  )
{
    IFS0bits.T3IF = false;
    esp_time_offset_counter++;
    if(esp_time_offset_counter>(ESP_DETECT_LIMIT*10)) esp_detected = 0;
}

void esp_process_time(void)
{
    esp_wifi_status = esp_time_buffer.fields.flags.wifi_status;
    esp_ntp_status = esp_time_buffer.fields.flags.ntp_status;
    esp_pps_sync = esp_time_buffer.fields.flags.pps_sync;
    esp_scheduler_sync = esp_time_buffer.fields.flags.scheduler_sync;
    
    esp_ntp_time = esp_time_buffer.fields.utc;
    esp = esp_ntp_time;
    
    //esp_ntp_milliseconds = esp_time_buffer.fields.milliseconds;
    //esp_ntp_offset = esp_time_buffer.fields.offset;
    
    if(esp_wifi_status && esp_ntp_status && esp_ntp_time)
    {
        esp_ntp_valid = 1;
        ntp = esp_ntp_time;
        esp = ntp;
    }
    else
    {
        esp_ntp_valid = 0;
    }
    
    memset(esp_time_buffer.raw, 0, sizeof(esp_time_buffer));
    esp_time_waiting = 0;
}

void esp_process_net(void)
{
    /*
    esp_wifi_status = esp_net_buffer.fields.flags.wifi_status;
    esp_ntp_status = esp_net_buffer.fields.flags.ntp_status;
    esp_pps_sync = esp_net_buffer.fields.flags.pps_sync;
    esp_scheduler_sync = esp_net_buffer.fields.flags.scheduler_sync;
    */
    esp_ntp_last_update = esp_net_buffer.fields.lastUpdate;
    esp_ntp_interval_count = esp_net_buffer.fields.ntpInterval;
    esp_dst_flags = esp_net_buffer.fields.dstFlags;
    memset(esp_net_buffer.raw, 0, sizeof(esp_net_buffer));
    esp_net_waiting = 0;
}

void esp_process_rtc(void)
{
    esp_rtc_time = esp_rtc_buffer.fields.rtc;
    if(!esp_ntp_valid) esp = esp_rtc_time;
    memset(esp_rtc_buffer.raw, 0, sizeof(esp_rtc_buffer));
    esp_rtc_waiting = 0;
}

void esp_process_sensor(void)
{
    esp_sensor_lux = esp_sensor_buffer.fields.lux;
    esp_sensor_temp = esp_sensor_buffer.fields.temp;
    esp_sensor_pres = esp_sensor_buffer.fields.pres;
    esp_sensor_hum = esp_sensor_buffer.fields.hum;
    memset(esp_sensor_buffer.raw, 0, sizeof(esp_sensor_buffer));
    esp_sensor_waiting = 0;
}

void esp_process_display(void)
{
    esp_brightness = esp_display_buffer.fields.brightness;
    esp_brightness_updated = 1;
    esp_display_state = esp_display_buffer.fields.display_state;
    esp_menu_state = esp_display_buffer.fields.menu_state;
    memset(esp_display_buffer.raw, 0, sizeof(esp_display_buffer));
    esp_display_waiting = 0;
}

void esp_process_user(void)
{
    char user_cmd = esp_user_buffer.fields.c;
    ui_uart1_input(user_cmd);
    memset(esp_user_buffer.raw, 0, sizeof(esp_user_buffer));
    esp_user_waiting = 0;
}

uint8_t esp_data_task_cycle = 0;

void esp_data_task(void)
{
    if(esp_time_waiting) esp_process_time();
    if(esp_net_waiting) esp_process_net();
    if(esp_rtc_waiting) esp_process_rtc();
    if(esp_sensor_waiting) esp_process_sensor();
    if(esp_display_waiting) esp_process_display();
    if(esp_user_waiting) esp_process_user();
    
    switch(esp_data_task_cycle)
    {
        case 0:
            esp_tx_time();
            esp_tx_gnss();
            esp_tx_rtc();
            esp_tx_offset();
            break;
            
        case 10:
            esp_tx_sensor();
            break;
                  
        case 20:
            esp_tx_display();
            break;

        default:
            break;
    }
    
    esp_data_task_cycle++;
    if(esp_data_task_cycle>120) esp_data_task_cycle = 0;
}

void esp_data_task_reset_cycle(void)
{
    esp_data_task_cycle = 0;
}

void esp_tx(void *buffer, uint16_t len) 
{
    uint16_t i;
    uint8_t *data = buffer;

    for(i=0; i<len; i++)
    {
        while(UART1_IsTxReady() == false)
        {
        }

        UART1_Write(*data++);
    }
}

extern CLOCK_SOURCE utc_source;
extern time_t display;
extern int32_t tz_offset;
extern int32_t dst_offset;
extern bool dst_active;

void esp_tx_time(void)
{
    SERIAL_PROTO_DATA_PIC_TIME esp_tx_buffer;
    memset(esp_tx_buffer.raw, 0, sizeof(esp_tx_buffer));

    esp_tx_buffer.fields.header.magic = SERIAL_PROTO_HEADER_MAGIC;
    esp_tx_buffer.fields.header.type = SERIAL_PROTO_TYPE_PIC_TX;
    esp_tx_buffer.fields.header.datatype = SERIAL_PROTO_DATATYPE_TIMEDATA;
    
    esp_tx_buffer.fields.utc_source = utc_source;
    esp_tx_buffer.fields.utc = utc;
    esp_tx_buffer.fields.tz_flags.tz_set = 0; // Unused
    esp_tx_buffer.fields.tz_flags.tz_offset = tz_offset / 900;
    
    esp_tx_buffer.fields.dst_flags.dst_set = 0;
    esp_tx_buffer.fields.dst_flags.dst_active = dst_active;
    esp_tx_buffer.fields.dst_flags.dst_offset = (dst_offset / 900);
    
    esp_tx(esp_tx_buffer.raw,sizeof(esp_tx_buffer));  
}

extern UBX_NAV_STATUS_GPSFIX ubx_nav_status_gpsfix;
extern bool ubx_nav_status_gpsfixok;
extern bool ubx_nav_timeutc_valid;
extern bool ubx_tim_tm2_valid;
extern int32_t ubx_nav_posllh_lat;
extern int32_t ubx_nav_posllh_lon;
extern int32_t ubx_nav_posllh_height;
extern int32_t ubx_nav_posllh_hmsl;

void esp_tx_gnss(void)
{
    SERIAL_PROTO_DATA_PIC_GNSS esp_tx_buffer;
    memset(esp_tx_buffer.raw, 0, sizeof(esp_tx_buffer));

    esp_tx_buffer.fields.header.magic = SERIAL_PROTO_HEADER_MAGIC;
    esp_tx_buffer.fields.header.type = SERIAL_PROTO_TYPE_PIC_TX;
    esp_tx_buffer.fields.header.datatype = SERIAL_PROTO_DATATYPE_GNSSDATA;
    
    esp_tx_buffer.fields.flags.gnss_detected = gnss_detected;
    esp_tx_buffer.fields.flags.gnss_fix = gnss_fix;
    esp_tx_buffer.fields.flags.fix_ok = ubx_nav_status_gpsfixok;
    esp_tx_buffer.fields.flags.utc_valid = ubx_nav_timeutc_valid;
    esp_tx_buffer.fields.flags.timemark_valid = ubx_tim_tm2_valid;
    esp_tx_buffer.fields.flags.fix_status = ubx_nav_status_gpsfix;
    
    esp_tx_buffer.fields.gnss = gnss;
    
    esp_tx_buffer.fields.posllh_lat = ubx_nav_posllh_lat;
    esp_tx_buffer.fields.posllh_lon = ubx_nav_posllh_lon;
    esp_tx_buffer.fields.posllh_height = ubx_nav_posllh_height / 1000;
    esp_tx_buffer.fields.posllh_hmsl = ubx_nav_posllh_hmsl / 1000;
    
    esp_tx(esp_tx_buffer.raw,sizeof(esp_tx_buffer));
    esp_gnss_data_updated = 0;
}

extern CLOCK_SYNC_STATUS clock_sync_state;
extern CLOCK_SYNC_STATUS clock_sync_state_last;
extern CLOCK_SYNC_STATUS last_sync_cause;
extern int32_t oc_offset;
extern int32_t accumulated_clocks;
extern time_t accumulation_delta;
extern uint32_t fosc_freq;
extern uint32_t total_oc_seq_count;
extern uint32_t sync_events;

void esp_tx_offset(void)
{
    SERIAL_PROTO_DATA_PIC_OFFSET esp_tx_buffer;
    memset(esp_tx_buffer.raw, 0, sizeof(esp_tx_buffer));

    esp_tx_buffer.fields.header.magic = SERIAL_PROTO_HEADER_MAGIC;
    esp_tx_buffer.fields.header.type = SERIAL_PROTO_TYPE_PIC_TX;
    esp_tx_buffer.fields.header.datatype = SERIAL_PROTO_DATATYPE_OFFSETDATA;
    
    esp_tx_buffer.fields.sync_state = clock_sync_state;
    esp_tx_buffer.fields.sync_state_last = clock_sync_state_last;
    esp_tx_buffer.fields.last_sync_cause = last_sync_cause;
    
    esp_tx_buffer.fields.fosc_freq = fosc_freq;
    esp_tx_buffer.fields.oc_offset = oc_offset;
    esp_tx_buffer.fields.accumulated_clocks = accumulated_clocks;
    esp_tx_buffer.fields.accumulation_delta = accumulation_delta;
    esp_tx_buffer.fields.total_oc_seq_count = total_oc_seq_count;
    esp_tx_buffer.fields.sync_events = sync_events;
    
    esp_tx(esp_tx_buffer.raw,sizeof(esp_tx_buffer));
}

void esp_tx_net(void)
{
    SERIAL_PROTO_DATA_PIC_NET esp_tx_buffer;
    memset(esp_tx_buffer.raw, 0, sizeof(esp_tx_buffer));
    
    esp_tx_buffer.fields.header.magic = SERIAL_PROTO_HEADER_MAGIC;
    esp_tx_buffer.fields.header.type = SERIAL_PROTO_TYPE_PIC_TX;
    esp_tx_buffer.fields.header.datatype = SERIAL_PROTO_DATATYPE_RTCDATA;
    
    esp_tx_buffer.fields.flags.reset_config = 0;
    
    esp_tx(esp_tx_buffer.raw,sizeof(esp_tx_buffer));
}

extern time_t rtc;
extern bool rtc_detected;
extern bool rtc_valid;
extern bool rtc_sync;

void esp_tx_rtc(void)
{
    SERIAL_PROTO_DATA_PIC_RTC esp_tx_buffer;
    memset(esp_tx_buffer.raw, 0, sizeof(esp_tx_buffer));
    
    esp_tx_buffer.fields.header.magic = SERIAL_PROTO_HEADER_MAGIC;
    esp_tx_buffer.fields.header.type = SERIAL_PROTO_TYPE_PIC_TX;
    esp_tx_buffer.fields.header.datatype = SERIAL_PROTO_DATATYPE_RTCDATA;
    
    esp_tx_buffer.fields.flags.rtc_detected = rtc_detected;
    esp_tx_buffer.fields.flags.rtc_valid = rtc_detected;
    esp_tx_buffer.fields.flags.rtc_sync = rtc_sync;
    
    esp_tx_buffer.fields.rtc = rtc;
    
    esp_tx(esp_tx_buffer.raw,sizeof(esp_tx_buffer));
}

extern bool veml6040_detected;
extern uint16_t veml_ambient_light;
extern bool bme280_detected;
extern int32_t bme280_temperature;
extern uint32_t bme280_pressure;
extern uint32_t bme280_humidity;

void esp_tx_sensor(void)
{
    SERIAL_PROTO_DATA_PIC_SENSOR esp_tx_buffer;
    memset(esp_tx_buffer.raw, 0, sizeof(esp_tx_buffer));
    
    esp_tx_buffer.fields.header.magic = SERIAL_PROTO_HEADER_MAGIC;
    esp_tx_buffer.fields.header.type = SERIAL_PROTO_TYPE_PIC_TX;
    esp_tx_buffer.fields.header.datatype = SERIAL_PROTO_DATATYPE_SENSORDATA;
    
    esp_tx_buffer.fields.flags.veml6040_detected = veml6040_detected;
    esp_tx_buffer.fields.flags.bme280_detected = bme280_detected;
    
    esp_tx_buffer.fields.lux = veml_ambient_light;
    
    esp_tx_buffer.fields.temp = bme280_temperature;
    esp_tx_buffer.fields.temp_raw = bme280_temperature - BME280_TEMP_OFFSET;
    esp_tx_buffer.fields.pres = (bme280_pressure / 10);
    esp_tx_buffer.fields.hum = bme280_humidity;
    
    esp_tx(esp_tx_buffer.raw,sizeof(esp_tx_buffer));
}

extern bool display_update_pending;
extern bool display_brightness_manual;
extern bool display_brightness_oc_running;
extern uint16_t display_brightness;
extern uint16_t display_brightness_target;
extern UI_DISPLAY_STATE ui_state_current;
extern UI_MENU_STATE ui_menu_current;

void esp_tx_display(void)
{
    SERIAL_PROTO_DATA_PIC_DISPLAY esp_tx_buffer;
    memset(esp_tx_buffer.raw, 0, sizeof(esp_tx_buffer));
    
    esp_tx_buffer.fields.header.magic = SERIAL_PROTO_HEADER_MAGIC;
    esp_tx_buffer.fields.header.type = SERIAL_PROTO_TYPE_PIC_TX;
    esp_tx_buffer.fields.header.datatype = SERIAL_PROTO_DATATYPE_DISPLAYDATA;

    esp_tx_buffer.fields.flags.update_pending = display_update_pending;
    esp_tx_buffer.fields.flags.brightness_manual = display_brightness_manual;
    esp_tx_buffer.fields.flags.oc_running = display_brightness_oc_running;
    esp_tx_buffer.fields.flags.pwr_stat = PWR_STAT_GetValue();
    esp_tx_buffer.fields.flags.switch_state = ui_switch_input_state();
    esp_tx_buffer.fields.flags.button_state = ui_button_input_state();
    
    esp_tx_buffer.fields.brightness = display_brightness;
    esp_tx_buffer.fields.brightness_target = display_brightness_target;
    
    esp_tx_buffer.fields.display_state = ui_state_current;
    esp_tx_buffer.fields.menu_state = ui_menu_current;
    
    esp_tx(esp_tx_buffer.raw,sizeof(esp_tx_buffer));
}

void esp_tx_user_start(void)
{
    char esp_tx_buffer[3] = { 0x83, 0x70, 0x80 };
    esp_tx(esp_tx_buffer,3);
}

void esp_tx_user_stop(void)
{
    char esp_tx_buffer[3] = { 0x80, 0x70, 0x83 };
    esp_tx(esp_tx_buffer,3);
}

void print_esp_data(void)
{
    if(esp_detected)
    {
        printf("\r\n=== ESP32 ===\r\n");
        printf("UTC: ");
        ui_print_iso8601_string(ntp);
        printf("\r\nWiFi: %01u Sync: %01u\r\n", esp_wifi_status, esp_ntp_status);
        print_esp_offset();
    }
    else
    {
        printf("\r\n=== NO ESP32 DETECTED ===\r\n");
    }
}