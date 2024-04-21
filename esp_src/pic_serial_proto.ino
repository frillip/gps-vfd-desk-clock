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
bool pic_gnss_detected = 0;
bool pic_gnss_fix = 0;
bool pic_fix_ok = 0;
bool pic_utc_valid = 0;
bool pic_timemark_valid = 0;
uint8_t pic_fix_status = 0;
time_t gnss = 0;
int32_t pic_posllh_lat = 0;
int32_t pic_posllh_lon = 0;
int16_t pic_posllh_height = 0;
int16_t pic_posllh_hmsl = 0;

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
bool pic_rtc_detected = 0;
bool pic_rtc_valid = 0;
bool pic_rtc_sync = 0;
time_t pic_rtc = 0;


char pic_sensor_string[SERIAL_PROTO_CHECK_BUFFER_SIZE] = {SERIAL_PROTO_HEADER, SERIAL_PROTO_TYPE_PIC_TX, SERIAL_PROTO_DATATYPE_SENSORDATA};
bool pic_sensor_waiting = 0;
SERIAL_PROTO_DATA_PIC_SENSOR pic_sensor_buffer;
bool pic_veml6040_detected = 0;
bool pic_bme280_detected = 0;
float pic_lux = 0;
float pic_temp = 0;
float pic_temp_raw = 0;
float pic_pres = 0;
float pic_hum = 0;

char pic_display_string[SERIAL_PROTO_CHECK_BUFFER_SIZE] = {SERIAL_PROTO_HEADER, SERIAL_PROTO_TYPE_PIC_TX, SERIAL_PROTO_DATATYPE_DISPLAYDATA};
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
UI_DISPLAY_STATE pic_display_state = UI_DISPLAY_STATE_INIT;
UI_MENU_STATE pic_menu_state = UI_MENU_STATE_ROOT;

char pic_user_string[SERIAL_PROTO_CHECK_BUFFER_SIZE] = {SERIAL_PROTO_HEADER, SERIAL_PROTO_TYPE_PIC_TX, SERIAL_PROTO_DATATYPE_USERDATA};
bool pic_user_waiting = 0;
SERIAL_PROTO_DATA_PIC_USER pic_user_buffer;

void pic_copy_buffer(PIC_MESSAGE_TYPE message);
PIC_MESSAGE_TYPE pic_check_incoming(void);
bool pic_last_char_hold = 0;

void print_clock_source(CLOCK_SOURCE source);
void print_clock_source(CLOCK_SOURCE source)
{
    switch(source)
    {
        case CLOCK_SOURCE_NONE:
            Serial.print("NONE");
            break;
            
        case CLOCK_SOURCE_RTC:
            Serial.print("RTC");
            break;
            
        case CLOCK_SOURCE_ESP:
            Serial.print("ESP");
            break;
            
        case CLOCK_SOURCE_NTP:
            Serial.print("NTP");
            break;
            
        case CLOCK_SOURCE_GNSS:
            Serial.print("GNSS");
            break;
            
        default:
            Serial.print("UNKNOWN SOURCE!");
            break;
    }
}


void sync_state_print(CLOCK_SYNC_STATUS sync_state);
void sync_state_print(CLOCK_SYNC_STATUS sync_state)
{
    switch(sync_state)
    {
        case SYNC_POWER_ON:
            Serial.print("SYNC_POWER_ON");
            break;
            
        case SYNC_RTC_DETECT:
            Serial.print("SYNC_RTC_DETECT");
            break;
            
        case SYNC_NTP_DETECT:
            Serial.print("SYNC_NTP_DETECT");
            break;
            
        case SYNC_GNSS_DETECT:
            Serial.print("SYNC_GNSS_DETECT");
            break;
            
        case SYNC_GNSS_WAIT_FOR_FIX:
            Serial.print("SYNC_GNSS_WAIT_FOR_FIX");
            break;
            
        case SYNC_STARTUP:
            Serial.print("SYNC_STARTUP");
            break;
            
        case SYNC_NOSYNC:
            Serial.print("SYNC_NOSYNC");
            break;
            
        case SYNC_ADJUST_STAGE_1:
            Serial.print("SYNC_ADJUST_STAGE_1");
            break;
            
        case SYNC_ADJUST_STAGE_2:
            Serial.print("SYNC_ADJUST_STAGE_2");
            break;
            
        case SYNC_PPS_SYNC:
            Serial.print("SYNC_PPS_SYNC");
            break;
            
        case SYNC_SCHED_SYNC:
            Serial.print("SYNC_SCHED_SYNC");
            break;
            
        case SYNC_MIN_INTERVAL:
            Serial.print("SYNC_MIN_INTERVAL");
            break;
            
        case SYNC_INTERVAL:
            Serial.print("SYNC_INTERVAL");
            break;
            
        case SYNC_SYNC:
            Serial.print("SYNC_SYNC");
            break;
            
        case SYNC_NOSYNC_MINOR:
            Serial.print("SYNC_NOSYNC_MINOR");
            break;
            
        case SYNC_NOSYNC_MINOR_OC:
            Serial.print("SYNC_NOSYNC_MINOR_OC");
            break;
            
        case SYNC_NOSYNC_MAJOR:
            Serial.print("SYNC_NOSYNC_MAJOR");
            break;
            
        case SYNC_NOSYNC_MAJOR_OC:
            Serial.print("SYNC_NOSYNC_MAJOR_OC");
            break;
            
        case SYNC_NOSYNC_FREQ:
            Serial.print("SYNC_NOSYNC_FREQ");
            break;
            
        case SYNC_NOSYNC_FREQ_ONLY:
            Serial.print("SYNC_NOSYNC_FREQ_ONLY");
            break;
            
        case SYNC_NOSYNC_GNSS:
            Serial.print("SYNC_NOSYNC_GNSS");
            break;
            
        case SYNC_NOSYNC_MANUAL:
            Serial.print("SYNC_NOSYNC_MANUAL");
            break;

        case SYNC_NO_CLOCK:
            Serial.print("SYNC_NO_CLOCK");
            break;

        case SYNC_RTC:
            Serial.print("SYNC_RTC");
            break;

        case SYNC_NTP:
            Serial.print("SYNC_NTP");
            break;

        case SYNC_NTP_ADJUST:
            Serial.print("SYNC_NTP_ADJUST");
            break;
            
        case SYNC_NTP_NO_NETWORK:
            Serial.print("SYNC_NTP_NO_NETWORK");
            break;
            
        default:
            Serial.print("UNKNOWN SYNC STATE!");
            break;
    }
}


void print_iso8601_string(time_t time)
{
  char buf[32] = {0}; // Allocate buffer
  struct tm *iso_time; // Allocate buffer
  iso_time = gmtime(&time);
  strftime(buf, 32, "%Y-%m-%dT%H:%M:%SZ", iso_time);
  Serial.print(buf);
}


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
      // Don't print the last char
      pic_last_char_hold = 1;
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

    // If we have plain text incoming, print it
    if(pic_waiting == PIC_NONE && pic_incoming == PIC_NONE && !pic_last_char_hold)
    {
      if(rx_char != SERIAL_PROTO_HEADER && rx_char != SERIAL_PROTO_TYPE_PIC_TX)
      {
        Serial.print(rx_char);
      }
    }
    pic_last_char_hold = 0;
  }
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
    if(pic_rtc_waiting) pic_process_rtc();
    if(pic_sensor_waiting) pic_process_sensor();
    if(pic_display_waiting) pic_process_display();
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

  if(WiFi.status() != WL_CONNECTED)
  {
    if(timeStatus() == timeNotSet)
    {
      if(pic_utc_source != CLOCK_SOURCE_NONE)
      {
        if(pic_new_pps)
        {
          if((pic_pps_offset_ms > 50) && (pic_pps_offset_ms < 950) && (timeStatus() != timeSync))
          {
            uint16_t pic_ms_offset = (micros() - pic_pps_micros) / 1000;
            UTC.setTime(pic,pic_ms_offset);
            pps_sync = 0;
            scheduler_sync = 0;
          }
          else
          {
            UTC.setTime(pic);
          }
          pic_new_pps = 0;
        }
      }
    }
  }

  pic_time_waiting = 0;
}


void print_pic_time(void)
{
  Serial.print("UTC:   ");
  print_iso8601_string(pic);

  Serial.print("\r\nGNSS:  ");
  print_iso8601_string(gnss);
  if(!pic_gnss_detected)
  {
    Serial.print(" - MISSING");
  }

  Serial.print("\r\nESP:   ");
  time_t now = UTC.now();
  print_iso8601_string(now);

  Serial.print("\r\nNTP:   ");
  print_iso8601_string(now);
  if(WiFi.status() != WL_CONNECTED)
  {
    Serial.print(" - NO WIFI");
  }
  else if(timeStatus() != timeSync)
  {
    Serial.print(" - NO NTP SYNC");
  }

  Serial.print("\r\nRTC:   ");
  print_iso8601_string(pic_rtc);
  if(!pic_rtc_detected)
  {
    Serial.print(" - MISSING");
  }

  Serial.print("\r\nLocal: ");
  time_t local = pic;
  local += pic_tz_offset;
  if(pic_dst_active) local += pic_dst_offset;
  char buf[32] = {0}; // Allocate buffer
  struct tm *iso_time; // Allocate buffer
  iso_time = gmtime(&local);
  strftime(buf, 32, "%Y-%m-%dT%H:%M:%S", iso_time);
  Serial.print(buf);
  int32_t total_offset = pic_tz_offset+pic_dst_offset;
  if((total_offset)>=0)
  {
    Serial.print("+"); 
  }
  else
  {
    Serial.print("-"); 
    total_offset = total_offset*-1;
  }
  uint32_t total_offset_hours = total_offset/3600;
  uint32_t total_offset_minutes = (total_offset-(total_offset_hours*3600))/60;

  sprintf(buf,"%02lu:%02lu",total_offset_hours,total_offset_minutes);
  Serial.print(buf);

  Serial.print("\r\nUTC source: ");
  print_clock_source(pic_utc_source);
  Serial.println("");
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

  pic_posllh_lat = pic_gnss_buffer.fields.posllh_lat;
  pic_posllh_lon = pic_gnss_buffer.fields.posllh_lon;
  pic_posllh_height = pic_gnss_buffer.fields.posllh_height;
  pic_posllh_hmsl = pic_gnss_buffer.fields.posllh_hmsl;

  if(pic_gnss_detected && pic_gnss_fix && gnss_new_pps)
  {
    if((gnss_pps_offset_ms > 50) && (gnss_pps_offset_ms < 950) && (timeStatus() != timeSync))
    {
      uint16_t gnss_ms_offset = (micros() - gnss_pps_micros) / 1000;
      UTC.setTime(gnss,gnss_ms_offset);
      pps_sync = 0;
      scheduler_sync = 0;
    }
    gnss_new_pps = 0;
  }

  pic_gnss_waiting = 0;
}


void print_gnss_data(void)
{
  if(pic_gnss_detected)
  {
    Serial.print("\r\n=== GNSS ===\r\n");
    Serial.print("Fix: ");
    Serial.print(pic_gnss_fix);
    Serial.print("  OK: ");
    Serial.print(pic_fix_ok);
    Serial.print("  UTC: ");
    Serial.print(pic_utc_valid);
    Serial.print("  TM: ");
    Serial.println(pic_timemark_valid);
    Serial.print("Fix type: ");
    Serial.print(pic_fix_status,HEX);
    Serial.print(" - ");
    switch (pic_fix_status)
          {
              case 0x00:
                  Serial.println("No fix");
                  break;
                  
              case 0x01:
                  Serial.println("Dead reckoning only");
                  break;

              case 0x02:
                  Serial.println("2D-fix");
                  break;

              case 0x03:
                  Serial.println("3D-fix");
                  break;

              case 0x04:
                  Serial.println("GNSS + dead reckoning combined");
                  break;

              case 0x05:
                  Serial.println("Time only fix");
                  break;
                  
              default:
                  Serial.println("Unknown");
                  break;
          }

    Serial.print("LAT: ");
    Serial.println((float)pic_posllh_lat/10000000,7);

    Serial.print("LON: ");
    Serial.println((float)pic_posllh_lon/10000000,7);

    Serial.print("Height: ");
    Serial.print((float)pic_posllh_height,0);
    Serial.print("m  mASL:");
    Serial.print((float)pic_posllh_hmsl,0);
    Serial.println("m");
  }
  else
  {
    Serial.print("\r\n=== NO GNSS DETECTED ===\r\n");
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
  pic_sync_events = pic_offset_buffer.fields.sync_events;

  pic_offset_waiting = 0;
}

void print_offset_data(void)
{
  Serial.print("\r\n=== Clock and PPS stats ===\r\n");
  Serial.print("Crystal freq: ");
  Serial.print((float)pic_fosc_freq / 1000000,6);
  Serial.print("MHz\r\nOC D: ");
  Serial.print(pic_oc_offset);
  Serial.print(" CLK D: ");
  Serial.print(pic_accumulation_delta);
  Serial.print(" CLK T: ");
  Serial.print(pic_accumulated_clocks);
  Serial.print("\r\nOC events: ");
  Serial.print(pic_total_oc_seq_count);
  Serial.print(" Resync events: ");
  Serial.println(pic_sync_events);
}


void pic_process_rtc(void)
{
  pic_rtc_detected = pic_rtc_buffer.fields.flags.rtc_detected;
  pic_rtc_valid = pic_rtc_buffer.fields.flags.rtc_valid;
  pic_rtc_sync = pic_rtc_buffer.fields.flags.rtc_sync;
  pic_rtc = pic_rtc_buffer.fields.rtc;

  pic_rtc_waiting = 0;
}


void pic_print_rtc(void)
{
  if(pic_rtc_detected)
  {
    Serial.print("\r\n=== RTC ===\r\n");
    Serial.print("D: ");
    Serial.print(pic_rtc_detected);
    Serial.print("  V: ");
    Serial.print(pic_rtc_valid);
    Serial.print("  S: ");
    Serial.println(pic_rtc_sync);
  }
  else
  {
    Serial.print("\r\n=== NO RTC DETECTED ===\r\n");
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


void print_veml_data(void)
{
    if(pic_veml6040_detected)
    {
        Serial.print("\r\n=== VEML6040 LUX DATA ===\r\n");
        Serial.print("LUX: ");
        Serial.print(pic_lux,1);
        Serial.print(" BRI: ");
        Serial.print(pic_brightness);
        Serial.print("/4000\r\n");
    }
    else
    {
        Serial.print("\r\n=== NO VEML6040 DETECTED ===\r\n");
    }
}


void print_bme_data()
{
  if(pic_bme280_detected)
  {
    Serial.print("\r\n=== BME280 env data ===\r\n");
    Serial.print("T: ");
    Serial.print(pic_temp,2);
    Serial.print("C (");
    Serial.print(pic_temp_raw,2);
    Serial.print("C board)\r\nP: ");
    Serial.print(pic_pres,1);
    Serial.print("mB\r\nH: ");
    Serial.print(pic_hum,2);
    Serial.println("%");
  }
  else
  {
    Serial.print("\r\n=== NO BME280 DETECTED ===\r\n");
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

  UI_DISPLAY_STATE pic_display_state = pic_display_buffer.fields.display_state;
  UI_MENU_STATE pic_menu_state = pic_display_buffer.fields.menu_state;

  pic_display_waiting = 0;
}


void print_sync_state_machine(void)
{
    Serial.print("\r\n=== STATE MACHINE ===\r\n");
    Serial.print("STATE: ");
    sync_state_print(pic_clock_sync_state);
    Serial.print(" LAST: ");
    sync_state_print(pic_clock_sync_state_last);
    Serial.print("\r\nLAST SYNC CAUSE: ");
    sync_state_print(pic_last_sync_cause);
    Serial.print("\r\n");
}


void pic_uart_tx_timedata()
{
  SERIAL_PROTO_DATA_ESP_TIME time_data_tx = {};
  memset(time_data_tx.raw, 0, sizeof(time_data_tx));

  time_data_tx.fields.header.magic = SERIAL_PROTO_HEADER;
  time_data_tx.fields.header.type = SERIAL_PROTO_TYPE_ESP_TX;
  time_data_tx.fields.header.datatype = SERIAL_PROTO_DATATYPE_TIMEDATA;


  if(WiFi.status() == WL_CONNECTED) time_data_tx.fields.flags.wifi_status = 1;
  if(timeStatus() == timeSync) time_data_tx.fields.flags.ntp_status = 1;
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
  if(timeStatus() == timeSync) net_data_tx.fields.flags.ntp_status = 1;
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