#include "enums.h"
#include "serial_proto.h"

uint8_t rx_stage = 0;
uint16_t user_data_counter = 0;
bool rx_ignore = 0;

uint8_t pic_char_offset = 0;
char pic_check_buffer[SERIAL_PROTO_CHECK_BUFFER_SIZE] = {0};
char pic_string_buffer[SERIAL_PROTO_STRING_BUFFER_SIZE] = {0};
PIC_MESSAGE_TYPE PIC_incoming = PIC_NONE;
PIC_MESSAGE_TYPE PIC_waiting = PIC_NONE;
uint8_t PIC_bytes_remaining = 0;

void pic_uart_rx()
{
  char c = UARTPIC.read();
  /*
  if(c==SERIAL_PROTO_HEADER)
  {
    rx_ignore=1;
    user_data_counter = 0;
  }
  if(rx_ignore)
  {
    user_data_counter++;
    if(user_data_counter>11) rx_ignore=0;
  }
  else Serial.print(c);
  */
  Serial.print(c);
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