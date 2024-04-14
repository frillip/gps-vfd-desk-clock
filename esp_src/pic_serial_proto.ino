#include "enums.h"
#include "serial_proto.h"

uint8_t rx_stage = 0;
uint16_t user_data_counter = 0;
bool rx_ignore = 0;

uint8_t pic_char_offset = 0;
char pic_check_buffer[SERIAL_PROTO_CHECK_BUFFER_SIZE] = {0};
char pic_string_buffer[SERIAL_PROTO_STRING_BUFFER_SIZE] = {0};
PIC_MESSAGE_TYPE pic_incoming = PIC_NONE;
PIC_MESSAGE_TYPE pic_waiting = PIC_NONE;
uint8_t pic_bytes_remaining = 0;

char pic_time_string[SERIAL_PROTO_CHECK_BUFFER_SIZE] = {SERIAL_PROTO_HEADER, SERIAL_PROTO_TYPE_PIC_TX, SERIAL_PROTO_DATATYPE_TIMEDATA};
bool pic_time_waiting = 0;
SERIAL_PROTO_DATA_PIC_TIME pic_time_buffer;
time_t pic = 0;
CLOCK_SOURCE pic_utc_source = CLOCK_SOURCE_NONE;
bool pic_tz_set = 0;
int32_t pic_tz_offset = 0;
bool pic_dst_set;
bool pic_dst_active;
int32_t pic_dst_offset;


char pic_gnss_string[SERIAL_PROTO_CHECK_BUFFER_SIZE] = {SERIAL_PROTO_HEADER, SERIAL_PROTO_TYPE_PIC_TX, SERIAL_PROTO_DATATYPE_GNSSDATA};
bool pic_gnss_waiting = 0;
SERIAL_PROTO_DATA_PIC_GNSS pic_gnss_buffer;
extern bool pic_gnss_fix = 0;
extern bool pic_fix_ok = 0;
extern bool pic_utc_valid = 0;
extern bool pic_timemark_valid = 0;
extern uint8_t pic_fix_status = 0;
extern int32_t pic_posllh_lat = 0;
extern int32_t pic_posllh_lon = 0;

char pic_offset_string[SERIAL_PROTO_CHECK_BUFFER_SIZE] = {SERIAL_PROTO_HEADER, SERIAL_PROTO_TYPE_PIC_TX, SERIAL_PROTO_DATATYPE_OFFSETDATA};
bool pic_offset_waiting = 0;
SERIAL_PROTO_DATA_PIC_OFFSET pic_offset_buffer;
CLOCK_SYNC_STATUS pic_clock_sync_state;
CLOCK_SYNC_STATUS pic_clock_sync_state_last;
CLOCK_SYNC_STATUS pic_last_sync_cause;
uint32_t pic_fosc_freq = 0;
int32_t pic_oc_offset = 0;
int32_t pic_accumulated_clocks = 0;
time_t pic_accumulation_delta = 0;
uint32_t pic_total_oc_seq_count = 0;
uint32_t pic_sync_events = 0;

char pic_net_string[SERIAL_PROTO_CHECK_BUFFER_SIZE] = {SERIAL_PROTO_HEADER, SERIAL_PROTO_TYPE_PIC_TX, SERIAL_PROTO_DATATYPE_NETDATA};
bool pic_net_waiting = 0;
SERIAL_PROTO_DATA_PIC_NET pic_net_buffer;

char pic_rtc_string[SERIAL_PROTO_CHECK_BUFFER_SIZE] = {SERIAL_PROTO_HEADER, SERIAL_PROTO_TYPE_PIC_TX, SERIAL_PROTO_DATATYPE_RTCDATA};
bool pic_rtc_waiting = 0;
SERIAL_PROTO_DATA_PIC_RTC pic_rtc_buffer;

char pic_sensor_string[SERIAL_PROTO_CHECK_BUFFER_SIZE] = {SERIAL_PROTO_HEADER, SERIAL_PROTO_TYPE_PIC_TX, SERIAL_PROTO_DATATYPE_SENSORDATA};
bool pic_sensor_waiting = 0;
SERIAL_PROTO_DATA_PIC_SENSOR pic_sensor_buffer;

char pic_display_string[SERIAL_PROTO_CHECK_BUFFER_SIZE] = {SERIAL_PROTO_HEADER, SERIAL_PROTO_TYPE_PIC_TX, SERIAL_PROTO_DATATYPE_DISPLAYDATA};
bool pic_display_waiting = 0;
SERIAL_PROTO_DATA_PIC_DISPLAY pic_display_buffer;

char pic_user_string[SERIAL_PROTO_CHECK_BUFFER_SIZE] = {SERIAL_PROTO_HEADER, SERIAL_PROTO_TYPE_PIC_TX, SERIAL_PROTO_DATATYPE_USERDATA};
bool pic_user_waiting = 0;
SERIAL_PROTO_DATA_PIC_USER pic_user_buffer;

void pic_copy_buffer(PIC_MESSAGE_TYPE message);
PIC_MESSAGE_TYPE pic_check_incoming(void);

void pic_uart_rx()
{
  while(UARTPIC.available())
  {
    char rx_char = UARTPIC.read();
    
    memmove(pic_check_buffer, pic_check_buffer+1, SERIAL_PROTO_CHECK_BUFFER_SIZE-1);
    pic_check_buffer[SERIAL_PROTO_CHECK_BUFFER_SIZE-1] = rx_char;
    
    if(pic_incoming!=PIC_NONE)
    {
      pic_detected = 1;
      pic_string_buffer[pic_char_offset] = rx_char;
      pic_char_offset++;
      pic_bytes_remaining--;
      // If we've reached the end of our buffer
      // then we're getting unknown messages
      // Discard anything that doesn't fit
      // and flag the incoming message as waiting
      if(pic_char_offset>=SERIAL_PROTO_STRING_BUFFER_SIZE-1 || pic_bytes_remaining==0)
      {
        pic_waiting = pic_incoming;
        pic_incoming = PIC_NONE;
      }
    }
    
    // Do we have a message that's finished coming in?
    if(pic_waiting!=PIC_NONE)
    {
      // if so, copy it to the right buffer
      pic_copy_buffer(pic_waiting);
      // Then reset the message buffer
      memset(pic_string_buffer, 0, SERIAL_PROTO_STRING_BUFFER_SIZE);
      // Reset the message waiting flag
      pic_waiting=PIC_NONE;
    }
    
    PIC_MESSAGE_TYPE pic_check_res = pic_check_incoming();
    if(pic_check_res != PIC_NONE)
    {
      pic_waiting = pic_incoming;
      pic_incoming = pic_check_res;
      pic_char_offset = SERIAL_PROTO_CHECK_BUFFER_SIZE;
      switch (pic_incoming)
      {
        case PIC_TIME:
          pic_bytes_remaining = sizeof(SERIAL_PROTO_DATA_PIC_TIME) - SERIAL_PROTO_CHECK_BUFFER_SIZE;
          memcpy(pic_string_buffer, pic_check_buffer, SERIAL_PROTO_CHECK_BUFFER_SIZE);
          break;

        case PIC_GNSS:
          pic_bytes_remaining = sizeof(SERIAL_PROTO_DATA_PIC_GNSS) - SERIAL_PROTO_CHECK_BUFFER_SIZE;
          memcpy(pic_string_buffer, pic_check_buffer, SERIAL_PROTO_CHECK_BUFFER_SIZE);
          break;

        case PIC_OFFSET:
          pic_bytes_remaining = sizeof(SERIAL_PROTO_DATA_PIC_OFFSET) - SERIAL_PROTO_CHECK_BUFFER_SIZE;
          memcpy(pic_string_buffer, pic_check_buffer, SERIAL_PROTO_CHECK_BUFFER_SIZE);
          break;

        case PIC_NET:
          pic_bytes_remaining = sizeof(SERIAL_PROTO_DATA_PIC_NET) - SERIAL_PROTO_CHECK_BUFFER_SIZE;
          memcpy(pic_string_buffer, pic_check_buffer, SERIAL_PROTO_CHECK_BUFFER_SIZE);
          break;

        case PIC_RTC:
          pic_bytes_remaining = sizeof(SERIAL_PROTO_DATA_PIC_RTC) - SERIAL_PROTO_CHECK_BUFFER_SIZE;
          memcpy(pic_string_buffer, pic_check_buffer, SERIAL_PROTO_CHECK_BUFFER_SIZE);
          break;

        case PIC_SENSOR:
          pic_bytes_remaining = sizeof(SERIAL_PROTO_DATA_PIC_SENSOR) - SERIAL_PROTO_CHECK_BUFFER_SIZE;
          memcpy(pic_string_buffer, pic_check_buffer, SERIAL_PROTO_CHECK_BUFFER_SIZE);
          break;

        case PIC_DISPLAY:
          pic_bytes_remaining = sizeof(SERIAL_PROTO_DATA_PIC_DISPLAY) - SERIAL_PROTO_CHECK_BUFFER_SIZE;
          memcpy(pic_string_buffer, pic_check_buffer, SERIAL_PROTO_CHECK_BUFFER_SIZE);
          break;

        case PIC_USER:
          pic_bytes_remaining = sizeof(SERIAL_PROTO_DATA_PIC_USER) - SERIAL_PROTO_CHECK_BUFFER_SIZE;
          memcpy(pic_string_buffer, pic_check_buffer, SERIAL_PROTO_CHECK_BUFFER_SIZE);
          break;

        default:
          break;
      }
    }
    Serial.print(rx_char);
  }
}

void pic_copy_buffer(PIC_MESSAGE_TYPE message)
{
  switch (message)
  {
    case PIC_TIME:
      memcpy(pic_time_buffer.raw, pic_string_buffer, sizeof(pic_time_buffer));
      Serial.println("PIC_TIME");
      pic_time_waiting=1;
      break;

    case PIC_GNSS:
      memcpy(pic_gnss_buffer.raw, pic_string_buffer, sizeof(pic_gnss_buffer));
      Serial.println("PIC_GNSS");
      pic_gnss_waiting=1;
      break;

    case PIC_OFFSET:
      memcpy(pic_offset_buffer.raw, pic_string_buffer, sizeof(pic_offset_buffer));
      Serial.println("PIC_OFFSET");
      pic_offset_waiting=1;
      break;

    case PIC_NET:
      memcpy(pic_net_buffer.raw, pic_string_buffer, sizeof(pic_net_buffer));
      Serial.println("PIC_NET");
      pic_net_waiting=1;
      break;

    case PIC_RTC:
      memcpy(pic_rtc_buffer.raw, pic_string_buffer, sizeof(pic_rtc_buffer));
      Serial.println("PIC_RTC");
      pic_rtc_waiting=1;
      break;

    case PIC_SENSOR:
      memcpy(pic_sensor_buffer.raw, pic_string_buffer, sizeof(pic_sensor_buffer));
      Serial.println("PIC_SENSOR");
      pic_sensor_waiting=1;
      break;

    case PIC_DISPLAY:
      memcpy(pic_display_buffer.raw, pic_string_buffer, sizeof(pic_display_buffer));
      Serial.println("PIC_DISPLAY");
      pic_display_waiting=1;
      break;

    case PIC_USER:
      memcpy(pic_user_buffer.raw, pic_string_buffer, sizeof(pic_user_buffer));
      Serial.println("PIC_USER");
      pic_user_waiting=1;
      break;

    default:
      pic_waiting=PIC_NONE;
      break;
  }
}

PIC_MESSAGE_TYPE pic_check_incoming(void)
{
  if(memcmp(pic_check_buffer, pic_time_string, SERIAL_PROTO_CHECK_BUFFER_SIZE)==0) return PIC_TIME;
  if(memcmp(pic_check_buffer, pic_gnss_string, SERIAL_PROTO_CHECK_BUFFER_SIZE)==0) return PIC_GNSS;
  if(memcmp(pic_check_buffer, pic_offset_string, SERIAL_PROTO_CHECK_BUFFER_SIZE)==0) return PIC_OFFSET;
  if(memcmp(pic_check_buffer, pic_net_string, SERIAL_PROTO_CHECK_BUFFER_SIZE)==0) return PIC_NET;
  if(memcmp(pic_check_buffer, pic_rtc_string, SERIAL_PROTO_CHECK_BUFFER_SIZE)==0) return PIC_RTC;
  if(memcmp(pic_check_buffer, pic_sensor_string, SERIAL_PROTO_CHECK_BUFFER_SIZE)==0) return PIC_SENSOR;
  if(memcmp(pic_check_buffer, pic_display_string, SERIAL_PROTO_CHECK_BUFFER_SIZE)==0) return PIC_DISPLAY;
  if(memcmp(pic_check_buffer, pic_user_string, SERIAL_PROTO_CHECK_BUFFER_SIZE)==0) return PIC_USER;
  return PIC_NONE;
}


uint8_t pic_data_task_cycle = 0;

void pic_data_task(void)
{
    if(pic_time_waiting) pic_process_time();
    if(pic_gnss_waiting) pic_process_gnss();
    if(pic_offset_waiting) pic_process_offset();
    //if(pic_net_waiting) pic_process_net();
    //if(pic_rtc_waiting) pic_process_rtc();
    //if(pic_sensor_waiting) pic_process_sensor();
    //if(pic_display_waiting) pic_process_display();
    //if(pic_user_waiting) pic_process_user();
}

void pic_process_time()
{
  pic_utc_source = pic_time_buffer.fields.utc_source;
  pic = pic_time_buffer.fields.utc;
  pic_tz_set = pic_time_buffer.fields.tz_flags.tz_set; // Unused
  pic_tz_offset = pic_time_buffer.fields.tz_flags.tz_offset * 900;
  
  pic_dst_set = pic_time_buffer.fields.dst_flags.dst_set;
  pic_dst_active = pic_time_buffer.fields.dst_flags.dst_active;
  pic_dst_offset = pic_time_buffer.fields.dst_flags.dst_offset * 900;
}


void pic_process_gnss(void)
{
    pic_gnss_fix = pic_gnss_buffer.fields.flags.gnss_fix;
    pic_fix_ok = pic_gnss_buffer.fields.flags.fix_ok;
    pic_utc_valid = pic_gnss_buffer.fields.flags.utc_valid;
    pic_timemark_valid = pic_gnss_buffer.fields.flags.timemark_valid;
    pic_fix_status = pic_gnss_buffer.fields.flags.fix_status;
    
    pic_posllh_lat = pic_gnss_buffer.fields.posllh_lat;
    pic_posllh_lon = pic_gnss_buffer.fields.posllh_lon;
}


void pic_process_offset(void)
{
    pic_clock_sync_state = pic_offset_buffer.fields.sync_state;
    pic_clock_sync_state_last = pic_offset_buffer.fields.sync_state_last;
    pic_last_sync_cause = pic_offset_buffer.fields.last_sync_cause;
    
    pic_fosc_freq = pic_offset_buffer.fields.fosc_freq;
    pic_oc_offset = pic_offset_buffer.fields.oc_offset;
    pic_accumulated_clocks = pic_offset_buffer.fields.accumulated_clocks;
    pic_accumulation_delta = pic_offset_buffer.fields.accumulation_delta;
    pic_total_oc_seq_count = pic_offset_buffer.fields.total_oc_seq_count;
    pic_sync_events = pic_offset_buffer.fields.sync_events;
}




void pic_uart_tx_timedata()
{
  SERIAL_PROTO_DATA_ESP_TIME time_data_tx = {};
  memset(time_data_tx.raw, 0, sizeof(time_data_tx));

  time_data_tx.fields.header.magic = SERIAL_PROTO_HEADER;
  time_data_tx.fields.header.type = SERIAL_PROTO_TYPE_ESP_TX;
  time_data_tx.fields.header.datatype = SERIAL_PROTO_DATATYPE_TIMEDATA;


  if(WiFi.status() == WL_CONNECTED) time_data_tx.fields.flags.wifi_status = 1;
  if(timeStatus() == timeSet) time_data_tx.fields.flags.ntp_status = 1;
  if(pps_sync) time_data_tx.fields.flags.pps_sync = 1;
  if(scheduler_sync) time_data_tx.fields.flags.scheduler_sync = 1;

  time_data_tx.fields.utc = UTC.now();

  time_data_tx.fields.tz_flags.tz_set = 0;
  time_data_tx.fields.tz_flags.tz_offset = 0;

  time_data_tx.fields.dst_flags.dst_set = 0;
  time_data_tx.fields.dst_flags.dst_active = 0;
  time_data_tx.fields.dst_flags.dst_offset = 3600/900;

  size_t bytesSent = UARTPIC.write(time_data_tx.raw, sizeof(time_data_tx));
}


void pic_uart_tx_netdata()
{
  SERIAL_PROTO_DATA_ESP_NET net_data_tx = {};
  memset(net_data_tx.raw, 0, sizeof(net_data_tx));

  net_data_tx.fields.header.magic = SERIAL_PROTO_HEADER;
  net_data_tx.fields.header.type = SERIAL_PROTO_TYPE_ESP_TX;
  net_data_tx.fields.header.datatype = SERIAL_PROTO_DATATYPE_NETDATA;

  if(WiFi.status() == WL_CONNECTED) net_data_tx.fields.flags.wifi_status = 1;
  if(timeStatus() == timeSet) net_data_tx.fields.flags.ntp_status = 1;
  if(pps_sync) net_data_tx.fields.flags.pps_sync = 1;
  if(scheduler_sync) net_data_tx.fields.flags.scheduler_sync = 1;

  net_data_tx.fields.lastUpdate = lastNtpUpdateTime();
  net_data_tx.fields.ntpInterval = ntp_interval_count;
  net_data_tx.fields.dstFlags = 0;

  size_t bytesSent = UARTPIC.write(net_data_tx.raw, sizeof(net_data_tx));
}

void pic_uart_tx_rtcdata()
{
  SERIAL_PROTO_DATA_ESP_RTC rtc_data_tx = {};
  memset(rtc_data_tx.raw, 0, sizeof(rtc_data_tx));

  rtc_data_tx.fields.header.magic = SERIAL_PROTO_HEADER;
  rtc_data_tx.fields.header.type = SERIAL_PROTO_TYPE_ESP_TX;
  rtc_data_tx.fields.header.datatype = SERIAL_PROTO_DATATYPE_RTCDATA;
  
  rtc_data_tx.fields.flags.rtc_detected = 0;
  rtc_data_tx.fields.flags.rtc_valid = 0;
  rtc_data_tx.fields.flags.rtc_sync = 0;

  //rtc_data_tx.fields.rtc = rtc.getEpoch();
  rtc_data_tx.fields.rtc = 0; 

  size_t bytesSent = UARTPIC.write(rtc_data_tx.raw, sizeof(rtc_data_tx));
}

void pic_uart_tx_sensordata()
{
  SERIAL_PROTO_DATA_ESP_SENSOR sensor_data_tx = {};
  memset(sensor_data_tx.raw, 0, sizeof(sensor_data_tx));

  sensor_data_tx.fields.header.magic = SERIAL_PROTO_HEADER;
  sensor_data_tx.fields.header.type = SERIAL_PROTO_TYPE_ESP_TX;
  sensor_data_tx.fields.header.datatype = SERIAL_PROTO_DATATYPE_SENSORDATA;

  sensor_data_tx.fields.flags.veml6040_detected = light_sensor_detected;
  sensor_data_tx.fields.flags.bme280_detected = env_sensor_detected;

  if(light_sensor_detected)
  {
    sensor_data_tx.fields.lux = light_sensor_lux * 10;
  }
  else sensor_data_tx.fields.lux = 0xFFFF;

  if(env_sensor_detected)
  {
    sensor_data_tx.fields.temp =  env_temp_f * 100;
    sensor_data_tx.fields.pres = env_pres_f / 10;
    sensor_data_tx.fields.hum = env_hum_f * 10;
  }
  else 
  {
    sensor_data_tx.fields.temp = 0xFFFF;
    sensor_data_tx.fields.pres = 0xFFFF;
    sensor_data_tx.fields.hum = 0xFFFF;
  }

  size_t bytesSent = UARTPIC.write(sensor_data_tx.raw, sizeof(sensor_data_tx));
}

void pic_uart_tx_displaydata()
{
  SERIAL_PROTO_DATA_ESP_DISPLAY display_data_tx = {};
  memset(display_data_tx.raw, 0, sizeof(display_data_tx));

  display_data_tx.fields.header.magic = SERIAL_PROTO_HEADER;
  display_data_tx.fields.header.type = SERIAL_PROTO_TYPE_ESP_TX;
  display_data_tx.fields.header.datatype = SERIAL_PROTO_DATATYPE_DISPLAYDATA;

  display_data_tx.fields.flags.update_pending = 0;
  display_data_tx.fields.flags.brightness_manual = 0;
  display_data_tx.fields.flags.oc_running = 0;
  display_data_tx.fields.flags.pwr_stat = 0;
  display_data_tx.fields.flags.switch_state = 0;
  display_data_tx.fields.flags.button_state = 0;

  if(light_sensor_detected)
  {
    display_data_tx.fields.brightness = light_sensor_lux * 35;
    display_data_tx.fields.brightness_target = light_sensor_lux * 35;
  }
  else
  {
    display_data_tx.fields.brightness = 2000;
    display_data_tx.fields.brightness_target = 2000;
  }

  display_data_tx.fields.display_state = UI_DISPLAY_STATE_CLOCK_HHMM;
  display_data_tx.fields.menu_state = UI_MENU_STATE_ROOT;

  size_t bytesSent = UARTPIC.write(display_data_tx.raw, sizeof(display_data_tx));
}

void pic_uart_tx_userdata(char c)
{
  SERIAL_PROTO_DATA_ESP_USER user_data_tx = {};
  memset(user_data_tx.raw, 0, sizeof(user_data_tx));

  user_data_tx.fields.header.magic = SERIAL_PROTO_HEADER;
  user_data_tx.fields.header.type = SERIAL_PROTO_TYPE_ESP_TX;
  user_data_tx.fields.header.datatype = SERIAL_PROTO_DATATYPE_USERDATA;

  user_data_tx.fields.c = c;

  size_t bytesSent = UARTPIC.write(user_data_tx.raw, sizeof(user_data_tx));
}