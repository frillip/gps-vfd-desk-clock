#define PIC_UART_HEADER 0x83
#define PIC_UART_TYPE_TX 0x65
#define PIC_UART_TYPE_RX 0x70
#define PIC_UART_DATATYPE_TIMEDATA 0x00
#define PIC_UART_DATATYPE_GNSSDATA 0x10
#define PIC_UART_DATATYPE_OFFSETDATA 0x20
#define PIC_UART_DATATYPE_NETDATA 0x30
#define PIC_UART_DATATYPE_RTCDATA 0x40
#define PIC_UART_DATATYPE_SENSORDATA 0x50
#define PIC_UART_DATATYPE_DISPLAYDATA 0x60
#define PIC_UART_DATATYPE_MISCDATA 0x70
#define PIC_UART_DATATYPE_USERDATA 0x80

uint8_t rx_stage = 0;
uint16_t user_data_counter = 0;
bool rx_ignore = 0;

void pic_uart_rx()
{
  char c = UARTPIC.read();
  if(c==PIC_UART_HEADER)
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
}

#pragma pack(push, 1)
union TimeDataUnion
{
  struct _struct
  {
    uint8_t header;
    uint8_t type;
    uint8_t datatype;
    uint8_t flags;
    time_t utc;
    uint16_t milliseconds;
    int8_t offset;
  } fields;
  uint8_t raw[sizeof(struct _struct)];
};
#pragma pack(pop)

void pic_uart_tx_timedata()
{
  TimeDataUnion time_data_tx = {};
  time_data_tx.fields.header = PIC_UART_HEADER;
  time_data_tx.fields.type = PIC_UART_TYPE_TX;
  time_data_tx.fields.datatype = PIC_UART_DATATYPE_TIMEDATA;

  if(WiFi.status() == WL_CONNECTED) time_data_tx.fields.flags |= 0x01;
  if(timeStatus() == timeSet) time_data_tx.fields.flags |= 0x02;
  if(pps_sync) time_data_tx.fields.flags |= 0x04;
  if(scheduler_sync) time_data_tx.fields.flags |= 0x08;
  time_data_tx.fields.utc = UTC.now();
  time_data_tx.fields.milliseconds = UTC.ms();
  time_data_tx.fields.offset = 0;

  size_t bytesSent = UARTPIC.write(time_data_tx.raw, sizeof(time_data_tx.raw));
}

#pragma pack(push, 1)
union NetDataUnion
{
  struct _struct
  {
    uint8_t header;
    uint8_t type;
    uint8_t datatype;
    uint8_t flags;
    time_t lastUpdate;
    uint16_t ntpInterval;
    uint8_t dstFlags;
  } fields;
  uint8_t raw[sizeof(struct _struct)];
};
#pragma pack(pop)

void pic_uart_tx_netdata()
{
  NetDataUnion net_data_tx = {};
  net_data_tx.fields.header = PIC_UART_HEADER;
  net_data_tx.fields.type = PIC_UART_TYPE_TX;
  net_data_tx.fields.datatype = PIC_UART_DATATYPE_NETDATA;

  if(WiFi.status() == WL_CONNECTED) net_data_tx.fields.flags |= 0x01;
  if(timeStatus() == timeSet) net_data_tx.fields.flags |= 0x02;
  if(pps_sync) net_data_tx.fields.flags |= 0x04;
  if(scheduler_sync) net_data_tx.fields.flags |= 0x08;

  net_data_tx.fields.lastUpdate = lastNtpUpdateTime();
  net_data_tx.fields.ntpInterval = ntp_interval_count;
  net_data_tx.fields.dstFlags = 0;

  size_t bytesSent = UARTPIC.write(net_data_tx.raw, sizeof(net_data_tx.raw));
}

#pragma pack(push, 1)
union RTCDataUnion
{
  struct _struct
  {
    uint8_t header;
    uint8_t type;
    uint8_t datatype;
    time_t rtc;
  } fields;
  uint8_t raw[sizeof(struct _struct)];
};
#pragma pack(pop)

void pic_uart_tx_rtcdata()
{
  RTCDataUnion rtc_data_tx = {};
  rtc_data_tx.fields.header = PIC_UART_HEADER;
  rtc_data_tx.fields.type = PIC_UART_TYPE_TX;
  rtc_data_tx.fields.datatype = PIC_UART_DATATYPE_RTCDATA;

  rtc_data_tx.fields.rtc = rtc.getEpoch();

  size_t bytesSent = UARTPIC.write(rtc_data_tx.raw, sizeof(rtc_data_tx.raw));
}

#pragma pack(push, 1)
union SensorDataUnion
{
  struct _struct
  {
    uint8_t header;
    uint8_t type;
    uint8_t datatype;
    uint16_t temp;
    uint16_t pres;
    uint16_t hum;
    uint16_t lux;
  } fields;
  uint8_t raw[sizeof(struct _struct)];
};
#pragma pack(pop)

void pic_uart_tx_sensordata()
{
  SensorDataUnion sensor_data_tx = {};
  sensor_data_tx.fields.header = PIC_UART_HEADER;
  sensor_data_tx.fields.type = PIC_UART_TYPE_TX;
  sensor_data_tx.fields.datatype = PIC_UART_DATATYPE_SENSORDATA;

  
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
  if(light_sensor_detected)
  {
    sensor_data_tx.fields.lux = light_sensor_lux * 10;
  }
  else sensor_data_tx.fields.lux = 0xFFFF;

  size_t bytesSent = UARTPIC.write(sensor_data_tx.raw, sizeof(sensor_data_tx.raw));
}

#pragma pack(push, 1)
union DisplayDataUnion
{
  struct _struct
  {
    uint8_t header;
    uint8_t type;
    uint8_t datatype;
    uint16_t brightness;
    uint8_t display_state;
    uint8_t menu_state;
  } fields;
  uint8_t raw[sizeof(struct _struct)];
};
#pragma pack(pop)

void pic_uart_tx_displaydata()
{
  DisplayDataUnion display_data_tx = {};
  display_data_tx.fields.header = PIC_UART_HEADER;
  display_data_tx.fields.type = PIC_UART_TYPE_TX;
  display_data_tx.fields.datatype = PIC_UART_DATATYPE_DISPLAYDATA;

  display_data_tx.fields.brightness = 2000;
  display_data_tx.fields.display_state = 0;
  display_data_tx.fields.menu_state = 0;

  size_t bytesSent = UARTPIC.write(display_data_tx.raw, sizeof(display_data_tx.raw));
}

#pragma pack(push, 1)
union UserDataUnion
{
  struct _struct
  {
    uint8_t header;
    uint8_t type;
    uint8_t datatype;
    char c;
  } fields;
  uint8_t raw[sizeof(struct _struct)];
};
#pragma pack(pop)

void pic_uart_tx_userdata(char c)
{
  UserDataUnion user_data_tx = {};
  user_data_tx.fields.header = PIC_UART_HEADER;
  user_data_tx.fields.type = PIC_UART_TYPE_TX;
  user_data_tx.fields.datatype = PIC_UART_DATATYPE_USERDATA;

  user_data_tx.fields.c = c;

  size_t bytesSent = UARTPIC.write(user_data_tx.raw, sizeof(user_data_tx.raw));
}