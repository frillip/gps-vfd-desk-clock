#include "pic.h"
extern time_t esp;

extern Stream *last_output_stream;

extern USER_PREFS_DATA_STRUCT user_prefs;

uint8_t rx_stage = 0;
uint16_t user_data_counter = 0;
bool rx_ignore = 0;

uint8_t pic_char_offset = 0;
char pic_check_buffer[sizeof(SERIAL_PROTO_HEADER)];
char pic_string_buffer[SERIAL_PROTO_STRING_BUFFER_SIZE] = {0};
PIC_MESSAGE_TYPE pic_incoming = PIC_NONE;
PIC_MESSAGE_TYPE pic_waiting = PIC_NONE;
uint8_t pic_bytes_remaining = 0;

uint8_t pic_output_offset = 0;
char pic_output_buffer[PIC_OUTPUT_BUFFER_LENGTH] = {0};


const SERIAL_PROTO_HEADER pic_time_string = { .magic = SERIAL_PROTO_HEADER_MAGIC, .type = SERIAL_PROTO_TYPE_PIC_TX, .datatype = SERIAL_PROTO_DATATYPE_TIMEDATA};
bool pic_time_waiting = 0;
SERIAL_PROTO_DATA_PIC_TIME pic_time_buffer;
time_t pic = 0;
CLOCK_SOURCE pic_utc_source = CLOCK_SOURCE_NONE;
bool pic_tz_auto = 0;
bool pic_tz_set = 0;
int32_t pic_tz_offset = 0;
bool pic_dst_auto = 0;
bool pic_dst_set = 0;
bool pic_dst_active = 0;
int32_t pic_dst_offset = 0;


const SERIAL_PROTO_HEADER pic_gnss_string = { .magic = SERIAL_PROTO_HEADER_MAGIC, .type = SERIAL_PROTO_TYPE_PIC_TX, .datatype = SERIAL_PROTO_DATATYPE_GNSSDATA};
bool pic_gnss_waiting = 0;
SERIAL_PROTO_DATA_PIC_GNSS pic_gnss_buffer;
bool pic_gnss_detected = 0;
bool pic_gnss_fix = 0;
bool pic_fix_ok = 0;
bool pic_utc_valid = 0;
bool pic_timemark_valid = 0;
UBX_NAV_STATUS_GPSFIX pic_fix_status = GPSFIX_NO_FIX;
time_t gnss = 0;
uint32_t pic_gnss_pps_count;
int32_t pic_posllh_lat = 0;
int32_t pic_posllh_lon = 0;
int16_t pic_posllh_height = 0;
int16_t pic_posllh_hmsl = 0;

const SERIAL_PROTO_HEADER pic_offset_string = { .magic = SERIAL_PROTO_HEADER_MAGIC, .type = SERIAL_PROTO_TYPE_PIC_TX, .datatype = SERIAL_PROTO_DATATYPE_OFFSETDATA};
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
uint32_t pic_pps_seq_count = 0;
uint32_t pic_pps_missing_count = 0;
uint32_t pic_sync_events = 0;

const SERIAL_PROTO_HEADER pic_net_string = { .magic = SERIAL_PROTO_HEADER_MAGIC, .type = SERIAL_PROTO_TYPE_PIC_TX, .datatype = SERIAL_PROTO_DATATYPE_NETDATA};
bool pic_net_waiting = 0;
SERIAL_PROTO_DATA_PIC_NET pic_net_buffer;

const SERIAL_PROTO_HEADER pic_rtc_string = { .magic = SERIAL_PROTO_HEADER_MAGIC, .type = SERIAL_PROTO_TYPE_PIC_TX, .datatype = SERIAL_PROTO_DATATYPE_RTCDATA};
bool pic_rtc_waiting = 0;
SERIAL_PROTO_DATA_PIC_RTC pic_rtc_buffer;
bool pic_rtc_detected = 0;
bool pic_rtc_valid = 0;
bool pic_rtc_sync = 0;
PIC_RTC_TYPE pic_rtc_type = RTC_UNDEFINED;
time_t pic_rtc = 0;


const SERIAL_PROTO_HEADER pic_sensor_string = { .magic = SERIAL_PROTO_HEADER_MAGIC, .type = SERIAL_PROTO_TYPE_PIC_TX, .datatype = SERIAL_PROTO_DATATYPE_SENSORDATA};
bool pic_sensor_waiting = 0;
SERIAL_PROTO_DATA_PIC_SENSOR pic_sensor_buffer;
bool pic_veml6040_detected = 0;
bool pic_bme280_detected = 0;
float pic_lux = 0;
float pic_temp = 0;
float pic_temp_raw = 0;
float pic_pres = 0;
float pic_hum = 0;

const SERIAL_PROTO_HEADER pic_display_string = { .magic = SERIAL_PROTO_HEADER_MAGIC, .type = SERIAL_PROTO_TYPE_PIC_TX, .datatype = SERIAL_PROTO_DATATYPE_DISPLAYDATA};
bool pic_display_waiting = 0;
SERIAL_PROTO_DATA_PIC_DISPLAY pic_display_buffer;
bool pic_update_pending = 0;
bool pic_brightness_manual = 0;
bool pic_oc_running = 0;
bool pic_pwr_stat = 0;
bool pic_switch_state = 0;
bool pic_button_state = 0;
uint16_t pic_brightness = 0;
uint16_t pic_brightness_target = 0;
UI_DISPLAY_STATE pic_display_state_current = UI_DISPLAY_STATE_INIT;
UI_DISPLAY_STATE pic_display_state_selected = UI_DISPLAY_STATE_INIT;
UI_MENU_STATE pic_menu_state = UI_MENU_STATE_ROOT;

const SERIAL_PROTO_HEADER pic_user_string = { .magic = SERIAL_PROTO_HEADER_MAGIC, .type = SERIAL_PROTO_TYPE_PIC_TX, .datatype = SERIAL_PROTO_DATATYPE_USERDATA};
bool pic_user_waiting = 0;
SERIAL_PROTO_DATA_PIC_USER pic_user_buffer;

bool pic_last_char_hold = 0;

uint32_t pic_pps_micros = 0;
uint16_t pic_timeout = 0;
bool pic_new_pps = 0;
bool pic_gnss_sync = 0;
bool pic_ntp_sync = 0;
bool pic_oc_sync = 0;
bool pic_sched_sync = 0;
uint32_t pic_xtal_freq = 0;
uint32_t pic_oc_count = 0;
uint32_t pic_sync_count = 0;
int32_t pic_cycle_slip = 0;

HardwareSerial UARTPIC(PIC_UART);  //using UART2
uint16_t pic_pps_offset_ms = 0;


void pic_init(void)
{
  pic_pps_init();
  pic_uart_init();
}

bool pic_detected = 0;
bool pic_is_detected(void)
{
  return pic_detected;
}

void pic_timeout_incr(void)
{
  if(pic_timeout<PIC_TIMEOUT_LIMIT)
  {
    pic_timeout++;
  }
  else
  {
    pic_detected = 0;
  }
}

void pic_pps_init(void)
{
  pinMode(PIC_PPS_PIN,INPUT);
  attachInterrupt(PIC_PPS_PIN, pic_pps_in, RISING);
}

void ARDUINO_ISR_ATTR pic_pps_in(void)
{
  pic_pps_micros = micros();
  if(!pic_detected) pic_detected = 1;
  pic++;
  pic_new_pps = 1;
  pic_timeout = 0;
}


void print_pic_pps_offset(Stream *output)
{
  extern uint32_t esp_micros;
  int32_t pic_offset_ms = pic_pps_offset_ms;
  int32_t pic_offset_micros = esp_micros - pic_pps_micros;
  output->printf("PIC offset:  ");
  float pic_offset_display = pic_offset_micros;
  if(pic_offset_display<-1000000) pic_offset_display += 1000000;
  if(pic_offset_display>1000000) pic_offset_display -= 1000000;
  if((pic_offset_display < 1000) && (pic_offset_display > -1000 ))
  {
    output->printf("%3.0fus\n", pic_offset_display);
  }
  else
  {
    pic_offset_display = pic_offset_display / 1000;
    output->printf("%6.2fms\n", pic_offset_display,2);
  }
}

void print_pic_pps_relative_offset(Stream *output)
{
  output->printf("PPS offset:  ");
  float pic_relative_offset_display = pic_oc_offset * 25;
  if(pic_relative_offset_display<-1000000000) pic_relative_offset_display += 1000000000;
  if(pic_relative_offset_display>1000000000) pic_relative_offset_display -= 1000000000;
  if((pic_relative_offset_display < 1000) && (pic_relative_offset_display > -1000 ))
  {
    output->printf("%3.0fns\n", pic_relative_offset_display);
  }
  else
  {
    pic_relative_offset_display = pic_relative_offset_display / 1000;
    output->printf("%6.2fus\n", pic_relative_offset_display,2);
  }
}

void pic_uart_init(void)
{
  UARTPIC.begin(PIC_BAUD, SERIAL_8N1, PIC_RXD, PIC_TXD);
  pinMode(PIC_TZ_BT_PIN, INPUT);
}

bool pic_uart_char_available(void)
{
  return UARTPIC.available();
}

uint32_t pic_last_rx_millis = 0;

void pic_uart_rx()
{
  if(UARTPIC.available())
  {
    while(UARTPIC.available())
    {
      char rx_char = UARTPIC.read();
      //output->printf("%X ", rx_char);

      memmove(pic_check_buffer, pic_check_buffer+1, sizeof(SERIAL_PROTO_HEADER)-1);
      pic_check_buffer[sizeof(SERIAL_PROTO_HEADER)-1] = rx_char;
      
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
        // Don't print the last char
        pic_last_char_hold = 1;
      }
      
      PIC_MESSAGE_TYPE pic_check_res = pic_check_incoming();
      if(pic_check_res != PIC_NONE)
      {
        pic_waiting = pic_incoming;
        pic_incoming = pic_check_res;
        pic_char_offset = sizeof(SERIAL_PROTO_HEADER);
        switch (pic_incoming)
        {
          case PIC_TIME:
            pic_bytes_remaining = sizeof(SERIAL_PROTO_DATA_PIC_TIME) - sizeof(SERIAL_PROTO_HEADER);
            memcpy(pic_string_buffer, pic_check_buffer, sizeof(SERIAL_PROTO_HEADER));
            break;

          case PIC_GNSS:
            pic_bytes_remaining = sizeof(SERIAL_PROTO_DATA_PIC_GNSS) - sizeof(SERIAL_PROTO_HEADER);
            memcpy(pic_string_buffer, pic_check_buffer, sizeof(SERIAL_PROTO_HEADER));
            break;

          case PIC_OFFSET:
            pic_bytes_remaining = sizeof(SERIAL_PROTO_DATA_PIC_OFFSET) - sizeof(SERIAL_PROTO_HEADER);
            memcpy(pic_string_buffer, pic_check_buffer, sizeof(SERIAL_PROTO_HEADER));
            break;

          case PIC_NET:
            pic_bytes_remaining = sizeof(SERIAL_PROTO_DATA_PIC_NET) - sizeof(SERIAL_PROTO_HEADER);
            memcpy(pic_string_buffer, pic_check_buffer, sizeof(SERIAL_PROTO_HEADER));
            break;

          case PIC_RTC:
            pic_bytes_remaining = sizeof(SERIAL_PROTO_DATA_PIC_RTC) - sizeof(SERIAL_PROTO_HEADER);
            memcpy(pic_string_buffer, pic_check_buffer, sizeof(SERIAL_PROTO_HEADER));
            break;

          case PIC_SENSOR:
            pic_bytes_remaining = sizeof(SERIAL_PROTO_DATA_PIC_SENSOR) - sizeof(SERIAL_PROTO_HEADER);
            memcpy(pic_string_buffer, pic_check_buffer, sizeof(SERIAL_PROTO_HEADER));
            break;

          case PIC_DISPLAY:
            pic_bytes_remaining = sizeof(SERIAL_PROTO_DATA_PIC_DISPLAY) - sizeof(SERIAL_PROTO_HEADER);
            memcpy(pic_string_buffer, pic_check_buffer, sizeof(SERIAL_PROTO_HEADER));
            break;

          case PIC_USER:
            pic_bytes_remaining = sizeof(SERIAL_PROTO_DATA_PIC_USER) - sizeof(SERIAL_PROTO_HEADER);
            memcpy(pic_string_buffer, pic_check_buffer, sizeof(SERIAL_PROTO_HEADER));
            break;

          default:
            break;
        }
      }

      // If we have plain text incoming, print it
      if(pic_waiting == PIC_NONE && pic_incoming == PIC_NONE && !pic_last_char_hold)
      {
        if(rx_char != SERIAL_PROTO_HEADER_MAGIC && rx_char != SERIAL_PROTO_TYPE_PIC_TX)
        {
          pic_last_rx_millis = millis();
          pic_output_buffer[pic_output_offset] = rx_char;
          pic_output_offset++;
          if( pic_output_offset >= PIC_OUTPUT_BUFFER_LENGTH -1) // Leave a character for null terminator
          {
            pic_flush_output_buffer();
          }
        }
      }
      pic_last_char_hold = 0;
    }
  }
}



bool pic_uart_output_finished(void)
{
  if(pic_last_rx_millis + PIC_UART_OUTPUT_TIMEOUT > millis()) return 1;
  return 0;
}

bool pic_output_buffer_empty(void)
{
  if(pic_output_offset) return 0;
  return 1;
}

void pic_flush_output_buffer(void)
{
  String output = String(pic_output_buffer);
  last_output_stream->printf("%s", output.c_str());
  pic_output_offset = 0;
  memset(pic_output_buffer, 0, PIC_OUTPUT_BUFFER_LENGTH);
}

void pic_copy_buffer(PIC_MESSAGE_TYPE message)
{
  switch (message)
  {
    case PIC_TIME:
      memcpy(pic_time_buffer.raw, pic_string_buffer, sizeof(pic_time_buffer));
      pic_time_waiting=1;
      break;

    case PIC_GNSS:
      memcpy(pic_gnss_buffer.raw, pic_string_buffer, sizeof(pic_gnss_buffer));
      pic_gnss_waiting=1;
      break;

    case PIC_OFFSET:
      memcpy(pic_offset_buffer.raw, pic_string_buffer, sizeof(pic_offset_buffer));
      pic_offset_waiting=1;
      break;

    case PIC_NET:
      memcpy(pic_net_buffer.raw, pic_string_buffer, sizeof(pic_net_buffer));
      pic_net_waiting=1;
      break;

    case PIC_RTC:
      memcpy(pic_rtc_buffer.raw, pic_string_buffer, sizeof(pic_rtc_buffer));
      pic_rtc_waiting=1;
      break;

    case PIC_SENSOR:
      memcpy(pic_sensor_buffer.raw, pic_string_buffer, sizeof(pic_sensor_buffer));
      pic_sensor_waiting=1;
      break;

    case PIC_DISPLAY:
      memcpy(pic_display_buffer.raw, pic_string_buffer, sizeof(pic_display_buffer));
      pic_display_waiting=1;
      break;

    case PIC_USER:
      memcpy(pic_user_buffer.raw, pic_string_buffer, sizeof(pic_user_buffer));
      pic_user_waiting=1;
      break;

    default:
      pic_waiting=PIC_NONE;
      break;
  }
}


PIC_MESSAGE_TYPE pic_check_incoming(void)
{
  if(memcmp(pic_check_buffer, &pic_time_string, sizeof(SERIAL_PROTO_HEADER))==0) return PIC_TIME;
  if(memcmp(pic_check_buffer, &pic_gnss_string, sizeof(SERIAL_PROTO_HEADER))==0) return PIC_GNSS;
  if(memcmp(pic_check_buffer, &pic_offset_string, sizeof(SERIAL_PROTO_HEADER))==0) return PIC_OFFSET;
  if(memcmp(pic_check_buffer, &pic_net_string, sizeof(SERIAL_PROTO_HEADER))==0) return PIC_NET;
  if(memcmp(pic_check_buffer, &pic_rtc_string, sizeof(SERIAL_PROTO_HEADER))==0) return PIC_RTC;
  if(memcmp(pic_check_buffer, &pic_sensor_string, sizeof(SERIAL_PROTO_HEADER))==0) return PIC_SENSOR;
  if(memcmp(pic_check_buffer, &pic_display_string, sizeof(SERIAL_PROTO_HEADER))==0) return PIC_DISPLAY;
  if(memcmp(pic_check_buffer, &pic_user_string, sizeof(SERIAL_PROTO_HEADER))==0) return PIC_USER;
  return PIC_NONE;
}


uint8_t pic_data_task_cycle = 0;

void pic_data_task(void)
{
    if(pic_time_waiting) pic_process_time();
    if(pic_gnss_waiting) pic_process_gnss();
    if(pic_offset_waiting) pic_process_offset();
    if(pic_net_waiting) pic_process_net();
    if(pic_rtc_waiting) pic_process_rtc();
    if(pic_sensor_waiting) pic_process_sensor();
    if(pic_display_waiting) pic_process_display();
    //if(pic_user_waiting) pic_process_user();
}




void pic_process_time()
{
  extern int32_t tz_offset;
  extern bool dst_active;
  extern int32_t dst_offset;
  extern uint32_t esp_micros;

  pic_utc_source = pic_time_buffer.fields.utc_source;

  pic = pic_time_buffer.fields.utc;

  pic_tz_auto = pic_time_buffer.fields.tz_flags.tz_auto;
  pic_tz_set = pic_time_buffer.fields.tz_flags.tz_set;
  pic_tz_offset = pic_time_buffer.fields.tz_offset;
  tz_offset = pic_tz_offset;
  
  pic_dst_auto = pic_time_buffer.fields.dst_flags.dst_auto;
  pic_dst_set = pic_time_buffer.fields.dst_flags.dst_set;
  pic_dst_active = pic_time_buffer.fields.dst_flags.dst_active;
  pic_dst_offset = pic_time_buffer.fields.dst_offset;

  /*
  uint8_t i;
  output->printf("\nTime_struct (%u bytes): ", sizeof(pic_time_buffer));
  for(i=0; i < sizeof(pic_time_buffer); i++)
  {
    output->printf("%X-",pic_time_buffer.raw[i]);
  }

  output->printf("\nheader.magic:  %X\n", pic_time_buffer.fields.header.magic);
  output->printf("  .type:       %X\n", pic_time_buffer.fields.header.type);
  output->printf("  .datatype:   %X\n", pic_time_buffer.fields.header.datatype);

  output->printf("utc_source:    %X\n", pic_time_buffer.fields.utc_source);
  output->printf("utc:           %X\n", pic_time_buffer.fields.utc);

  output->printf("tz_flags.set:  %X\n", pic_time_buffer.fields.tz_flags.tz_set);
  output->printf("  .offset:     %X\n", pic_time_buffer.fields.tz_flags.tz_offset);

  output->printf("dst_flags.set: %X\n", pic_time_buffer.fields.dst_flags.dst_set);
  output->printf("  .active:     %X\n", pic_time_buffer.fields.dst_flags.dst_active);
  output->printf("  .offset:     %X\n", pic_time_buffer.fields.dst_flags.dst_offset);

  //output->printf("checksum:      %X\n", pic_time_buffer.fields.checksum);

  memset(pic_time_buffer.raw, 0xFF, sizeof(pic_time_buffer));

  pic_time_buffer.fields.header.magic = 0x83;
  pic_time_buffer.fields.header.type = 0xb6;
  pic_time_buffer.fields.header.datatype = 0xf0;
  pic_time_buffer.fields.utc_source = CLOCK_SOURCE_RTC;
  pic_time_buffer.fields.utc = 0x66ec1441;
  pic_time_buffer.fields.tz_flags.tz_set = 0x1;
  pic_time_buffer.fields.tz_flags.tz_offset = 0x2;
  pic_time_buffer.fields.dst_flags.dst_set = 0x1;
  pic_time_buffer.fields.dst_flags.dst_active = 0x1;
  pic_time_buffer.fields.dst_flags.dst_offset = 0x3;
  //pic_time_buffer.fields.checksum = 0x0;

  output->printf("\nTime_struct (%u bytes): ", sizeof(pic_time_buffer));
  for(i=0; i < sizeof(pic_time_buffer.raw); i++)
  {
    output->printf("%X-",pic_time_buffer.raw[i]);
  }

  output->printf("\nheader.magic:  %X\n", pic_time_buffer.fields.header.magic);
  output->printf("  .type:       %X\n", pic_time_buffer.fields.header.type);
  output->printf("  .datatype:   %X\n", pic_time_buffer.fields.header.datatype);

  output->printf("utc_source:    %X\n", pic_time_buffer.fields.utc_source);
  output->printf("utc:           %X\n", pic_time_buffer.fields.utc);

  output->printf("tz_flags.set:  %X\n", pic_time_buffer.fields.tz_flags.tz_set);
  output->printf("  .offset:     %X\n", pic_time_buffer.fields.tz_flags.tz_offset);

  output->printf("dst_flags.set: %X\n", pic_time_buffer.fields.dst_flags.dst_set);
  output->printf("  .active:     %X\n", pic_time_buffer.fields.dst_flags.dst_active);
  output->printf("  .offset:     %X\n", pic_time_buffer.fields.dst_flags.dst_offset);

  //output->printf("checksum:      %X\n", pic_time_buffer.fields.checksum);
  */

  dst_active = pic_dst_active;
  dst_offset = pic_dst_offset;

  if(timeStatus() != timeSync)
  {
    if((pic_utc_source != CLOCK_SOURCE_NONE) && (pic_utc_source != CLOCK_SOURCE_ESP) && (pic_utc_source != CLOCK_SOURCE_NTP)) // Do not sync if PIC is using ESP/NTP time or has no clock
    {
      if(pic_new_pps)
      {
        if(timeStatus() == timeNotSet)
        {
          UTC.setTime(pic); // Set the coarse time if ESP time isn't set
        }
        
        if(pic_utc_source == CLOCK_SOURCE_GNSS) // If we have a GNSS sync
        {
          int32_t pic_us_offset = esp_micros - pic_pps_micros;
          while(pic_us_offset>1000000) pic_us_offset = pic_us_offset - 1000000;
          while(pic_us_offset< -1000000) pic_us_offset = pic_us_offset + 1000000;
          if((pic_us_offset > 50000) || (pic_us_offset < -50000)) // Resync to GNSS if we have no NTP time and ESP has drifted
          {
            pic_us_offset = (micros() + PIC_ESP_SET_LATENCY_US) - pic_pps_micros; // recalculate with current micros() + latency for adjustment
            UTC.setTimeus(pic,(uint32_t)pic_us_offset); // Set fine time to GNSS
            esp_pps_unsync();
            scheduler_unsync();
            if(pic_us_offset > 500000) pic_us_offset = pic_us_offset - 1000000;
            last_output_stream->printf("ESP resync: %lius\n", pic_us_offset);
          }
        }
        pic_new_pps = 0;
      }
    }
  }

  pic_time_waiting = 0;
}


void print_local_time(Stream *output, time_t now)
{
  extern int32_t tz_offset;
  extern bool dst_active;
  extern int32_t dst_offset;  
  time_t local = now;
  local += tz_offset;
  if(dst_active) local += dst_offset;
  char buf[32] = {0}; // Allocate buffer
  struct tm *iso_time; // Allocate buffer
  iso_time = gmtime(&local);
  strftime(buf, 32, "%Y-%m-%dT%H:%M:%S", iso_time);
  output->printf(buf);
  print_local_offset(output, (int32_t)(local - now));
}

void print_local_offset(Stream *output, int32_t total_offset)
{
  char buf[10] = {0};
  if((total_offset)>=0)
  {
      output->printf("+"); 
  }
  else
  {
      output->printf("-"); 
      total_offset = total_offset*-1;
  }
  uint32_t total_offset_hours = total_offset/3600;
  uint32_t total_offset_minutes = (total_offset-(total_offset_hours*3600))/60;
  sprintf(buf,"%02lu:%02lu",total_offset_hours,total_offset_minutes);
  output->printf(buf);
}


void print_iso8601_string(Stream *output, time_t time)
{
  char buf[32] = {0}; // Allocate buffer
  struct tm *iso_time; // Allocate buffer
  iso_time = gmtime(&time);
  strftime(buf, 32, "%Y-%m-%dT%H:%M:%SZ", iso_time);
  output->printf(buf);
}


void print_pic_time(Stream *output)
{
  output->printf("UTC:   ");
  print_iso8601_string(output, pic);

  output->printf("\nGNSS:  ");
  print_iso8601_string(output, gnss);
  if(!pic_gnss_detected)
  {
    output->printf(" - MISSING");
  }

  time_t now = UTC.now();
  output->printf("\nESP:   ");
  print_iso8601_string(output, now);

  output->printf("\nNTP:   ");
  print_iso8601_string(output, now);
  if(WiFi.status() != WL_CONNECTED)
  {
    output->printf(" - NO WIFI");
  }
  else if(timeStatus() != timeSync)
  {
    output->printf(" - NO NTP SYNC");
  }

  output->printf("\nRTC:   ");
  print_iso8601_string(output, pic_rtc);
  if(!pic_rtc_detected)
  {
    output->printf(" - MISSING");
  }
  else if(!pic_rtc_valid)
  {
    output->printf(" - INVALID");
  }

  output->printf("\nLocal: ");
  print_local_time(output, pic);
  output->printf("\nTZ:    ");
  print_local_offset(output, pic_tz_offset);
  
  output->printf("\nDST:   ");
  if(pic_dst_active)
  {
      print_local_offset(output, pic_dst_offset);
      output->printf(" (active)");
  }
  else
  {
      output->printf("Inactive");
  }

  output->printf("\nUTC source: ");
  print_clock_source(output, pic_utc_source);
  output->printf("\n");
}


void print_clock_source(Stream *output, CLOCK_SOURCE source)
{
    switch(source)
    {
        case CLOCK_SOURCE_NONE:
            output->printf("NONE");
            break;
            
        case CLOCK_SOURCE_RTC:
            output->printf("RTC");
            break;
            
        case CLOCK_SOURCE_ESP:
            output->printf("ESP");
            break;
            
        case CLOCK_SOURCE_NTP:
            output->printf("NTP");
            break;
            
        case CLOCK_SOURCE_GNSS:
            output->printf("GNSS");
            break;
            
        default:
            output->printf("UNKNOWN SOURCE!");
            break;
    }
}


void pic_process_gnss(void)
{
  pic_gnss_detected = pic_gnss_buffer.fields.flags.gnss_detected;
  pic_gnss_fix = pic_gnss_buffer.fields.flags.gnss_fix;
  pic_fix_ok = pic_gnss_buffer.fields.flags.fix_ok;
  pic_utc_valid = pic_gnss_buffer.fields.flags.utc_valid;
  pic_timemark_valid = pic_gnss_buffer.fields.flags.timemark_valid;
  pic_fix_status = pic_gnss_buffer.fields.flags.fix_status;
  
  gnss = pic_gnss_buffer.fields.gnss;

  pic_gnss_pps_count = pic_gnss_buffer.fields.gnss_pps_count;

  pic_posllh_lat = pic_gnss_buffer.fields.posllh_lat;
  pic_posllh_lon = pic_gnss_buffer.fields.posllh_lon;
  pic_posllh_height = pic_gnss_buffer.fields.posllh_height;
  pic_posllh_hmsl = pic_gnss_buffer.fields.posllh_hmsl;

  // Don't bother with this, the PIC PPS signal should already be synced to GNSS if available
  /*if(pic_gnss_detected && pic_gnss_fix && gnss_new_pps)
  {
    if((gnss_pps_offset_ms > 50) && (gnss_pps_offset_ms < 950) && (timeStatus() != timeSync))
    {
      uint16_t gnss_us_offset = micros() - gnss_pps_micros;
      UTC.setTimeus(gnss,gnss_us_offset);
      pps_sync = 0;
      scheduler_sync = 0;
    }
    gnss_new_pps = 0;
  }*/

  pic_gnss_waiting = 0;
}


void print_gnss_fix(Stream *output, UBX_NAV_STATUS_GPSFIX fix_status)
{
  switch (fix_status)
  {
    case GPSFIX_NO_FIX:
      output->printf("No fix");
      break;
        
    case GPSFIX_DR_ONLY:
      output->printf("Dead reckoning only");
      break;

    case GPSFIX_2D:
      output->printf("2D-fix");
      break;

    case GPSFIX_3D:
      output->printf("3D-fix");
      break;

    case GPSFIX_GPS_DR:
      output->printf("GNSS + dead reckoning combined");
      break;

    case GPSFIX_TIME_ONLY:
      output->printf("Time only fix");
      break;
        
    default:
      output->printf("Unknown");
      break;
  }
}

void print_gnss_data(Stream *output)
{
  if(pic_gnss_detected)
  {
    output->printf("\n=== GNSS ===\n");
    output->printf("Fix: %u  OK: %u  UTC: %u  TM: %u\n", pic_gnss_fix, pic_fix_ok, pic_utc_valid, pic_timemark_valid);

    output->printf("Fix type: %02X - ", pic_fix_status);
    print_gnss_fix(output, pic_fix_status);
    output->printf("\n");

    output->printf("PPS count: %u PPS Miss: %u\n", pic_gnss_pps_count, pic_pps_missing_count);

    output->printf("LAT: %12.7f\n", (float)pic_posllh_lat/10000000);
    output->printf("LON: %12.7f\n", (float)pic_posllh_lon/10000000);
    output->printf("Height: %4.0fm   mASL: %4.0fm\n", (float)pic_posllh_height, (float)pic_posllh_hmsl);
  }
  else
  {
    output->printf("\n=== NO GNSS DETECTED ===\n");
  }
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
  pic_pps_seq_count = pic_offset_buffer.fields.pps_seq_count;
  pic_pps_missing_count = pic_offset_buffer.fields.pps_missing_count;
  pic_sync_events = pic_offset_buffer.fields.sync_events;

  pic_offset_waiting = 0;
}

void print_offset_data(Stream *output)
{
  output->printf("\n=== Clock and PPS stats ===\n");
  uint32_t pic_fosc_freq_i = pic_fosc_freq  / 1000000UL;
  uint32_t pic_fosc_freq_f = pic_fosc_freq  - (pic_fosc_freq_i * 1000000UL);
  output->printf("Crystal freq: %lu.%06luMHz\n", pic_fosc_freq_i, pic_fosc_freq_f);
  output->printf("OC D: %i CLK D: %u CLK T: %i\n", pic_oc_offset, (uint32_t)pic_accumulation_delta, pic_accumulated_clocks);
  output->printf("OC events: %u Resync events: %u\n", pic_total_oc_seq_count, pic_sync_events);
}

void pic_process_net(void)
{
  extern WiFiManager wm;
  if(pic_net_buffer.fields.flags.reset_config)
  {
    wm.resetSettings(); // Delete WiFi credentials
    ESP.restart(); // And reset
  }

  pic_net_waiting = 0;
}

void pic_process_rtc(void)
{
  pic_rtc_detected = pic_rtc_buffer.fields.flags.rtc_detected;
  pic_rtc_valid = pic_rtc_buffer.fields.flags.rtc_valid;
  pic_rtc_sync = pic_rtc_buffer.fields.flags.rtc_sync;
  pic_rtc_type = pic_rtc_buffer.fields.flags.rtc_type;
  pic_rtc = pic_rtc_buffer.fields.rtc;

  pic_rtc_waiting = 0;
}


void print_rtc_data(Stream *output)
{
  if(pic_rtc_detected)
  {
    output->printf("\n=== RTC ===\n");
    if(pic_rtc_type==RTC_DS1307)
    {
      output->printf("DS1307: ");
    }
    else if(pic_rtc_type==RTC_PCF8563)
    {
      output->printf("PCF8563: ");
    }
    else
    {
      output->printf("Undefined: ");
    }
    print_iso8601_string(output, pic_rtc);
    output->printf("\nValid: %u Sync: %u\n", pic_rtc_valid, pic_rtc_sync);
  }
  else
  {
    output->printf("\n=== NO RTC DETECTED ===\n");
  }
}


void pic_process_sensor(void)
{
  pic_veml6040_detected = pic_sensor_buffer.fields.flags.veml6040_detected;
  pic_bme280_detected = pic_sensor_buffer.fields.flags.bme280_detected;

  pic_lux = (float)pic_sensor_buffer.fields.lux / 10;
  pic_temp = (float)pic_sensor_buffer.fields.temp / 100;
  pic_temp_raw = (float)pic_sensor_buffer.fields.temp_raw / 100;
  pic_pres = (float)pic_sensor_buffer.fields.pres / 10;
  pic_hum = ((float)pic_sensor_buffer.fields.hum) / 100;

  pic_sensor_waiting = 0;
}


void print_veml_data(Stream *output)
{
    if(pic_veml6040_detected)
    {
        output->printf("\n=== VEML6040 LUX DATA ===\n");
        output->printf("LUX: %5.1f  BRI: %u/4000\n", pic_lux, pic_brightness);
    }
    else
    {
        output->printf("\n=== NO VEML6040 DETECTED ===\n");
    }
}


void print_bme_data(Stream *output)
{
  if(pic_bme280_detected)
  {
    output->printf("\n=== BME280 env data ===\n");
    output->printf("T: %5.2fC (%5.2fC board)\n", pic_temp, pic_temp_raw);
    output->printf("P: %6.1fmB\n", pic_pres);
    output->printf("H: %6.2f%%\n", pic_hum);
  }
  else
  {
    output->printf("\n=== NO BME280 DETECTED ===\n");
  }
}


void pic_process_display(void)
{
  pic_update_pending = pic_display_buffer.fields.flags.update_pending;
  pic_brightness_manual = pic_display_buffer.fields.flags.brightness_manual;
  pic_oc_running = pic_display_buffer.fields.flags.oc_running;
  pic_pwr_stat = pic_display_buffer.fields.flags.pwr_stat;
  pic_switch_state = pic_display_buffer.fields.flags.switch_state;
  pic_button_state = pic_display_buffer.fields.flags.button_state;

  pic_brightness = pic_display_buffer.fields.brightness;
  pic_brightness_target = pic_display_buffer.fields.brightness_target;

  UI_DISPLAY_STATE pic_display_state_current = pic_display_buffer.fields.display_state.current;
  UI_DISPLAY_STATE pic_display_state_selected = pic_display_buffer.fields.display_state.selected;
  UI_MENU_STATE pic_menu_state = pic_display_buffer.fields.menu_state;

  pic_display_waiting = 0;
}


void print_sync_state_machine(Stream *output)
{
    output->printf("\n=== STATE MACHINE ===\n");
    output->printf("STATE: ");
    sync_state_print(output, pic_clock_sync_state);
    output->printf(" LAST: ");
    sync_state_print(output, pic_clock_sync_state_last);
    output->printf("\nLAST SYNC CAUSE: ");
    sync_state_print(output, pic_last_sync_cause);
    output->printf("\n");
}


void sync_state_print(Stream *output, CLOCK_SYNC_STATUS sync_state)
{
    switch(sync_state)
    {
        case SYNC_POWER_ON:
            output->printf("SYNC_POWER_ON");
            break;
            
        case SYNC_RTC_DETECT:
            output->printf("SYNC_RTC_DETECT");
            break;
            
        case SYNC_NTP_DETECT:
            output->printf("SYNC_NTP_DETECT");
            break;
            
        case SYNC_GNSS_DETECT:
            output->printf("SYNC_GNSS_DETECT");
            break;
            
        case SYNC_GNSS_WAIT_FOR_FIX:
            output->printf("SYNC_GNSS_WAIT_FOR_FIX");
            break;
            
        case SYNC_STARTUP:
            output->printf("SYNC_STARTUP");
            break;
            
        case SYNC_NOSYNC:
            output->printf("SYNC_NOSYNC");
            break;
            
        case SYNC_ADJUST_STAGE_1:
            output->printf("SYNC_ADJUST_STAGE_1");
            break;
            
        case SYNC_ADJUST_STAGE_2:
            output->printf("SYNC_ADJUST_STAGE_2");
            break;
            
        case SYNC_PPS_SYNC:
            output->printf("SYNC_PPS_SYNC");
            break;
            
        case SYNC_SCHED_SYNC:
            output->printf("SYNC_SCHED_SYNC");
            break;
            
        case SYNC_MIN_INTERVAL:
            output->printf("SYNC_MIN_INTERVAL");
            break;
            
        case SYNC_INTERVAL:
            output->printf("SYNC_INTERVAL");
            break;
            
        case SYNC_SYNC:
            output->printf("SYNC_SYNC");
            break;
            
        case SYNC_NOSYNC_MINOR:
            output->printf("SYNC_NOSYNC_MINOR");
            break;
            
        case SYNC_NOSYNC_MINOR_OC:
            output->printf("SYNC_NOSYNC_MINOR_OC");
            break;
            
        case SYNC_NOSYNC_MAJOR:
            output->printf("SYNC_NOSYNC_MAJOR");
            break;
            
        case SYNC_NOSYNC_MAJOR_OC:
            output->printf("SYNC_NOSYNC_MAJOR_OC");
            break;
            
        case SYNC_NOSYNC_FREQ:
            output->printf("SYNC_NOSYNC_FREQ");
            break;
            
        case SYNC_NOSYNC_FREQ_ONLY:
            output->printf("SYNC_NOSYNC_FREQ_ONLY");
            break;
            
        case SYNC_NOSYNC_GNSS:
            output->printf("SYNC_NOSYNC_GNSS");
            break;
            
        case SYNC_NOSYNC_MANUAL:
            output->printf("SYNC_NOSYNC_MANUAL");
            break;

        case SYNC_NO_CLOCK:
            output->printf("SYNC_NO_CLOCK");
            break;

        case SYNC_RTC:
            output->printf("SYNC_RTC");
            break;

        case SYNC_NTP:
            output->printf("SYNC_NTP");
            break;

        case SYNC_NTP_ADJUST:
            output->printf("SYNC_NTP_ADJUST");
            break;
            
        case SYNC_NTP_NO_NETWORK:
            output->printf("SYNC_NTP_NO_NETWORK");
            break;
            
        default:
            output->printf("UNKNOWN SYNC STATE!");
            break;
    }
}


void pic_uart_tx_timedata()
{
  SERIAL_PROTO_DATA_ESP_TIME time_data_tx = {};
  memset(time_data_tx.raw, 0, sizeof(time_data_tx));

  time_data_tx.fields.header.magic = SERIAL_PROTO_HEADER_MAGIC;
  time_data_tx.fields.header.type = SERIAL_PROTO_TYPE_ESP_TX;
  time_data_tx.fields.header.datatype = SERIAL_PROTO_DATATYPE_TIMEDATA;


  if(WiFi.status() == WL_CONNECTED) time_data_tx.fields.flags.wifi_status = 1;
  if(timeStatus() == timeSync) time_data_tx.fields.flags.ntp_status = 1;
  time_data_tx.fields.flags.pps_sync = esp_pps_is_sync();
  time_data_tx.fields.flags.scheduler_sync = scheduler_is_sync();

  time_data_tx.fields.utc = UTC.now();

  time_data_tx.fields.tz_flags.tz_auto = 0;
  time_data_tx.fields.tz_flags.tz_set = 0;
  time_data_tx.fields.tz_offset = 0;

  time_data_tx.fields.dst_flags.dst_auto = 0;
  time_data_tx.fields.dst_flags.dst_set = 0;
  time_data_tx.fields.dst_flags.dst_active = 0;
  time_data_tx.fields.dst_offset = 0;

  size_t bytesSent = UARTPIC.write(time_data_tx.raw, sizeof(time_data_tx));
}


void pic_uart_tx_netdata()
{
  extern uint16_t ntp_interval_count;
  SERIAL_PROTO_DATA_ESP_NET net_data_tx = {};
  memset(net_data_tx.raw, 0, sizeof(net_data_tx));

  net_data_tx.fields.header.magic = SERIAL_PROTO_HEADER_MAGIC;
  net_data_tx.fields.header.type = SERIAL_PROTO_TYPE_ESP_TX;
  net_data_tx.fields.header.datatype = SERIAL_PROTO_DATATYPE_NETDATA;

  if(WiFi.status() == WL_CONNECTED) net_data_tx.fields.flags.wifi_status = 1;
  if(timeStatus() == timeSync) net_data_tx.fields.flags.ntp_status = 1;
  net_data_tx.fields.flags.pps_sync = esp_pps_is_sync();
  net_data_tx.fields.flags.scheduler_sync = scheduler_is_sync();

  net_data_tx.fields.lastUpdate = lastNtpUpdateTime();
  net_data_tx.fields.ntpInterval = ntp_interval_count;
  net_data_tx.fields.dstFlags = 0;

  size_t bytesSent = UARTPIC.write(net_data_tx.raw, sizeof(net_data_tx));
}

void pic_uart_tx_rtcdata()
{
  SERIAL_PROTO_DATA_ESP_RTC rtc_data_tx = {};
  memset(rtc_data_tx.raw, 0, sizeof(rtc_data_tx));

  rtc_data_tx.fields.header.magic = SERIAL_PROTO_HEADER_MAGIC;
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

  sensor_data_tx.fields.header.magic = SERIAL_PROTO_HEADER_MAGIC;
  sensor_data_tx.fields.header.type = SERIAL_PROTO_TYPE_ESP_TX;
  sensor_data_tx.fields.header.datatype = SERIAL_PROTO_DATATYPE_SENSORDATA;

  /*
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
  */

  size_t bytesSent = UARTPIC.write(sensor_data_tx.raw, sizeof(sensor_data_tx));
}


void pic_uart_tx_displaydata()
{
  SERIAL_PROTO_DATA_ESP_DISPLAY display_data_tx = {};
  memset(display_data_tx.raw, 0, sizeof(display_data_tx));

  display_data_tx.fields.header.magic = SERIAL_PROTO_HEADER_MAGIC;
  display_data_tx.fields.header.type = SERIAL_PROTO_TYPE_ESP_TX;
  display_data_tx.fields.header.datatype = SERIAL_PROTO_DATATYPE_DISPLAYDATA;

  display_data_tx.fields.flags.update_pending = 0;
  display_data_tx.fields.flags.brightness_manual = 0;
  display_data_tx.fields.flags.oc_running = 0;
  display_data_tx.fields.flags.pwr_stat = 0;
  display_data_tx.fields.flags.switch_state = 0;
  display_data_tx.fields.flags.button_state = 0;

  /*
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
  */

  display_data_tx.fields.display_state = UI_DISPLAY_STATE_CLOCK_HHMM;
  display_data_tx.fields.menu_state = UI_MENU_STATE_ROOT;

  size_t bytesSent = UARTPIC.write(display_data_tx.raw, sizeof(display_data_tx));
}

void pic_uart_tx_tzinfodata()
{
  extern bool tzinfo_available;
  extern int32_t tzinfo_offset;
  extern int32_t tzinfo_dst_offset;
  extern time_t tzinfo_dst_next;
  extern bool tzinfo_dst_active;
  extern TZINFO_SOURCE tzinfo_source;

  SERIAL_PROTO_DATA_ESP_TZINFO tzinfo_data_tx = {};
  memset(tzinfo_data_tx.raw, 0, sizeof(tzinfo_data_tx));

  tzinfo_data_tx.fields.header.magic = SERIAL_PROTO_HEADER_MAGIC;
  tzinfo_data_tx.fields.header.type = SERIAL_PROTO_TYPE_ESP_TX;
  tzinfo_data_tx.fields.header.datatype = SERIAL_PROTO_DATATYPE_TZINFODATA;

  if(user_prefs.fields.rtzinfo.flags.enabled)
  {
    tzinfo_data_tx.fields.tzinfo_flags.tzinfo_available = tzinfo_available;
    tzinfo_data_tx.fields.tzinfo_flags.tzinfo_source = tzinfo_source;
    tzinfo_data_tx.fields.tz_offset = tzinfo_offset;
    tzinfo_data_tx.fields.dst_offset = tzinfo_dst_offset;
    tzinfo_data_tx.fields.dst_next = tzinfo_dst_next;
    tzinfo_data_tx.fields.dst_flags.dst_active = tzinfo_dst_active;
  }

  size_t bytesSent = UARTPIC.write(tzinfo_data_tx.raw, sizeof(tzinfo_data_tx));
}

void pic_uart_tx_userdata(USER_CMD cmd, uint32_t arg, Stream *output)
{
  SERIAL_PROTO_DATA_ESP_USER user_data_tx = {};
  memset(user_data_tx.raw, 0, sizeof(user_data_tx));

  user_data_tx.fields.header.magic = SERIAL_PROTO_HEADER_MAGIC;
  user_data_tx.fields.header.type = SERIAL_PROTO_TYPE_ESP_TX;
  user_data_tx.fields.header.datatype = SERIAL_PROTO_DATATYPE_USERDATA;

  user_data_tx.fields.cmd = cmd;
  user_data_tx.fields.arg = arg;

  size_t bytesSent = UARTPIC.write(user_data_tx.raw, sizeof(user_data_tx));

  last_output_stream = output;
}

void pic_enter_bootloader(Stream *output)
{
  SERIAL_PROTO_DATA_ESP_BOOTLOADER bootloader_data_tx = {};
  memset(bootloader_data_tx.raw, 0, sizeof(bootloader_data_tx));

  bootloader_data_tx.fields.header.magic = SERIAL_PROTO_HEADER_MAGIC;
  bootloader_data_tx.fields.header.type = SERIAL_PROTO_TYPE_ESP_TX;
  bootloader_data_tx.fields.header.datatype = SERIAL_PROTO_DATATYPE_BOOTLOADERDATA;

  bootloader_data_tx.fields.cmd = BOOTLOADER_CMD_ENTER;
  bootloader_data_tx.fields.arg = 0xA5A5A5A5;

  size_t bytesSent = UARTPIC.write(bootloader_data_tx.raw, sizeof(bootloader_data_tx));
  output->printf("Entering PIC bootloader... ");

  extern bool pic_bootloader_active;
  pic_bootloader_active = pic_bootloader_entry(&UARTPIC);
  if(pic_bootloader_active) output->printf("SUCCESS!\n");
  else  output->printf("FAIL!\n");
}

void pic_exit_bootloader(Stream *output)
{
  output->printf("Exit PIC bootloader... ");
  extern bool pic_bootloader_active;
  pic_bootloader_active = !pic_bootloader_exit(&UARTPIC);
  if(!pic_bootloader_active) output->printf("SUCCESS!\n");
  else  output->printf("FAIL!\n");
}
