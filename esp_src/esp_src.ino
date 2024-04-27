//#include <ESP32Time.h>
#include <ezTime.h>
#include <HardwareSerial.h>
#include <WiFi.h>
#include <WiFiManager.h>
//#include <Wire.h>
//#include <SparkFunBME280.h>
#include <esp_task_wdt.h>
#define WDT_TIMEOUT 15

WiFiManager wm;
//ESP32Time rtc;

#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22
#define I2C_FREQ 100000UL

bool env_sensor_detected = 0;
float env_temp_f = 0;
float env_pres_f = 0;
float env_hum_f = 0;
#define ENV_SENSOR_ID 0x76
//BME280 env_sensor;


bool light_sensor_detected = 0;
bool light_sensor_gain = 0;         // x1
uint8_t light_sensor_id;
uint8_t light_sensor_int_time = 2;
unsigned int light_sensor_int_time_ms;
bool light_sensor_unsaturated;
double light_sensor_lux = 0;
#define LIGHT_SENSOR_ID 0x39
//SFE_TSL2561 light_sensor;

#define NTP_SERVER "rubidium.darksky.io"
#define NTP_INTERVAL 1800
uint16_t ntp_interval_count = 0;
uint32_t ntp_resync_count = 0;

#define DEBUG_UART  0
#define DEBUG_BAUD  115200

#define GNSS_UART    1
#define GNSS_BAUD    115200
#define GNSS_RXD     17
#define GNSS_TXD     16
#define GNSS_PPS_PIN 18
HardwareSerial UARTGNSS(GNSS_UART);  //using UART1

uint16_t gnss_pps_offset_ms = 0;
bool gnss_detected = 0;
float gnss_lat = 0.0;
float gnss_long = 0.0;
bool gnss_fix_valid = 0;

#define PIC_UART    2
#define PIC_BAUD    115200
#define PIC_RXD     13
#define PIC_TXD     12
#define PIC_PPS_PIN 27
HardwareSerial UARTPIC(PIC_UART);  //using UART2

bool pic_detected = 0;
uint16_t pic_pps_offset_ms = 0;

#define PPS_OUT_PIN 5
#define STATUS_LED_PIN 23

bool pic_gnss_sync = 0;
bool pic_ntp_sync = 0;
bool pic_oc_sync = 0;
bool pic_sched_sync = 0;
uint32_t pic_xtal_freq = 0;
uint32_t pic_oc_count = 0;
uint32_t pic_sync_count = 0;
int32_t pic_cycle_slip = 0;

#define SCHEDULER_HW_TIMER 0
hw_timer_t *scheduler_timer = NULL;

int8_t t1ms = 0;
int8_t t1ms0 = 0;
int8_t t10ms = 0;
int8_t t10ms0 = 0;
int8_t t100ms = 0;
int8_t t100ms0 = 0;
int8_t t100ms1 = 0;
int8_t t100ms2 = 0;
int8_t t100ms3 = 0;
int8_t t1s0 = 0;

bool scheduler_sync = 0;

#define PPS_HW_TIMER 1
hw_timer_t *pps_timer = NULL;

bool pps_sync = 0;

void IRAM_ATTR scheduler_1ms()
{
  t1ms++;
  t1ms0++;
  if(t1ms==10)
  {
    t1ms=0;
    t10ms++;
    t10ms0++;
    if(t10ms==10)
    {
      t10ms=0;
      t100ms++;
      t100ms0++;
      t100ms1++;
      t100ms2++;
      t100ms3++;
      if(t100ms==10)
      {
        t100ms=0;
        t1s0++;
      }
    }
  }
}

void scheduler_reset()
{
  t1ms = 0;
  t1ms0 = 0;
  t10ms = 0;
  t10ms0 = 0;
  t100ms = 0;
  t100ms0 = 0;
  t100ms1 = 0;
  t100ms2 = 0;
  t100ms3 = 0;
  t1s0 = 0;
}

void pps_out()
{
  digitalWrite(PPS_OUT_PIN, 1);
}

void startNTPqueries(void)
{
  updateNTP();
  ntp_resync_count++;
  ntp_interval_count = 0;
}

void fe_build_ntp_string(void)
{
  
}

uint32_t wifi_disconnect_millis = 0;
uint32_t wifi_disconnect_last_millis = 0;
#define WIFI_RECONNECT_INTERVAL_MILLIS 30000

void reconnect_wifi(void)
{
  Serial.println("Reconnecting to WiFi...");
  WiFi.disconnect();
  WiFi.reconnect();
}

#define GNSS_TIMEOUT_LIMIT 300 // in 0.01s counts
uint32_t gnss_pps_micros = 0;
uint16_t gnss_timeout = 0;
bool gnss_new_pps = 0;
uint32_t esp_micros = 0;
extern time_t gnss;

void IRAM_ATTR gnss_pps_in(void)
{
  gnss_pps_micros = micros();
  gnss_pps_offset_ms = UTC.ms();
  if(!gnss_detected) gnss_detected = 1;
  gnss++;
  gnss_new_pps = 1;
  gnss_timeout = 0;
}

#define PIC_TIMEOUT_LIMIT 300 // in 0.01s counts
uint32_t pic_pps_micros = 0;
uint16_t pic_timeout = 0;
bool pic_new_pps = 0;
extern time_t pic;

void IRAM_ATTR pic_pps_in(void)
{
  pic_pps_micros = micros();
  pic_pps_offset_ms = UTC.ms();
  if(!pic_detected) pic_detected = 1;
  pic++;
  pic_new_pps = 1;
  pic_timeout = 0;
}

void setup()
{
  esp_task_wdt_init(WDT_TIMEOUT, true); //enable panic so ESP32 restarts
  esp_task_wdt_add(NULL); //add current thread to WDT watch
  
  Serial.begin(DEBUG_BAUD);
  uint32_t id = 0;
  for(int i=0; i<17; i=i+8) {
    id |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
  }
  id &= 0xFFFFFF;
  char id_buf[6] = {0};
  sprintf(id_buf,"%06X", id);
  String wifi_id = id_buf;
  String wifi_hostname  = String("SMOL_CLOCK_" + wifi_id);
  WiFi.setHostname(wifi_hostname.c_str());
  Serial.println(wifi_hostname);

  UARTGNSS.begin(GNSS_BAUD, SERIAL_8N1, GNSS_RXD, GNSS_TXD);
  UARTPIC.begin(PIC_BAUD, SERIAL_8N1, PIC_RXD, PIC_TXD);

  //UTC.setTime(rtc.getEpoch());
  /*
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQ);

  env_sensor.setI2CAddress(ENV_SENSOR_ID);
  if(env_sensor.beginI2C()) env_sensor_detected = 1;

  light_sensor.begin(LIGHT_SENSOR_ID);
  if(light_sensor.getID(light_sensor_id))
  {
    light_sensor_detected = 1;
    light_sensor.setTiming(light_sensor_gain,light_sensor_int_time,light_sensor_int_time_ms);
    light_sensor.setPowerUp();
  }*/

  setServer(NTP_SERVER);
  setInterval(NTP_INTERVAL);

  WiFi.mode(WIFI_STA);
  wm.setConfigPortalBlocking(false);
  wm.setConfigPortalTimeout(900);
  wm.setConnectTimeout(5);
  if(wm.autoConnect(wifi_hostname.c_str()))
  {
    Serial.println(WiFi.localIP());
    startNTPqueries();
  }
  else
  {
    Serial.println("Configportal running");
    Serial.println(WiFi.localIP());
    wm.setSaveConfigCallback(startNTPqueries);
  }

  scheduler_timer = timerBegin(SCHEDULER_HW_TIMER, 80, true);
  timerAttachInterrupt(scheduler_timer, &scheduler_1ms, true);
  timerAlarmWrite(scheduler_timer, 1000, true);
  timerAlarmEnable(scheduler_timer);

  pinMode(PPS_OUT_PIN, OUTPUT);
  digitalWrite(PPS_OUT_PIN, 0);

  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, 0);

  pinMode(GNSS_PPS_PIN,INPUT);
  attachInterrupt(GNSS_PPS_PIN, gnss_pps_in, RISING);

  pinMode(PIC_PPS_PIN,INPUT);
  attachInterrupt(PIC_PPS_PIN, pic_pps_in, RISING);

  pps_timer = timerBegin(PPS_HW_TIMER, 80, true);
  timerAttachInterrupt(pps_timer, &pps_out, true);
  timerAlarmWrite(pps_timer, 1000000, true);
  timerAlarmEnable(pps_timer);
}

void loop()
{
  if(secondChanged())
  {
    esp_task_wdt_reset();
    esp_micros = micros();
    if(!pps_sync)
    {
      if(timeStatus() != timeNotSet && !UTC.ms())
      {
        timerRestart(pps_timer);
        pps_out();
        pps_sync = 1;
      }
    }
    if(!scheduler_sync)
    {
      if(timeStatus() != timeNotSet && !UTC.ms())
      {
        timerRestart(scheduler_timer);
        scheduler_reset();
        scheduler_sync = 1;
      }
    }
    if(timeStatus() == timeSync) 
    {
      digitalWrite(STATUS_LED_PIN, 1);
    }
    pic_uart_tx_timedata();
    
    /*
    time_t now = UTC.now();
    Serial.print("UTC: ");
    print_iso8601_string(now);
    Serial.println("");
    */
  }

  if(Serial.available())
  {
    char c = Serial.read();
    if(c==0x45) ESP.restart(); // Reset ESP on 'E'
    else if(c==0x57) // on 'W'...
    {
      wm.resetSettings(); // Delete WiFi credentials
      ESP.restart(); // And reset
    }
    else if(c==0x5A) // on 'Z'
    {
      Serial.println("");
      print_pic_time();
      
      if(gnss_detected && timeStatus() != timeNotSet)
      {
        int32_t gnss_offset_ms = gnss_pps_offset_ms;
        int32_t gnss_offset_micros = esp_micros - gnss_pps_micros;
        Serial.print("GNSS offset: ");
        Serial.print((float)(gnss_offset_micros)/1000);
        Serial.println("ms");
      }
      if(pic_detected && timeStatus() != timeNotSet)
      {
        int32_t pic_offset_ms = pic_pps_offset_ms;
        int32_t pic_offset_micros = esp_micros - pic_pps_micros;
        Serial.print("PIC offset: ");
        Serial.print((float)(pic_offset_micros)/1000);
        Serial.println("ms");
      }
      
      print_offset_data();
      print_gnss_data();
      pic_print_rtc();
      print_veml_data();
      print_bme_data();
      print_sync_state_machine();
    }
    pic_uart_tx_userdata(c);
  }

  if(UARTPIC.available())
  {
    pic_uart_rx();
  }

  wm.process();
  events();

  if(WiFi.status() != WL_CONNECTED)
  {
    wifi_disconnect_millis = millis();
    if(wifi_disconnect_millis - wifi_disconnect_last_millis > WIFI_RECONNECT_INTERVAL_MILLIS)
    {
      wifi_disconnect_last_millis = wifi_disconnect_millis;
      reconnect_wifi();
    }
  }

  if(t1ms0)
  {
    t1ms0=0;
    if(digitalRead(PPS_OUT_PIN))
    {
      if(UTC.ms()) 
      {
        digitalWrite(PPS_OUT_PIN, 0);
      }
    }
    if(digitalRead(STATUS_LED_PIN))
    {
      if(UTC.ms()>=100) 
      {
        digitalWrite(STATUS_LED_PIN, 0);
      }
    }
  }
  if(t10ms0)
  {
    t10ms0=0;
    if(gnss_detected)
    {
      if(gnss_timeout<GNSS_TIMEOUT_LIMIT)
      {
        gnss_timeout++;
      }
      else
      {
        gnss_detected = 0;
      }
    }
    if(pic_detected)
    {
      if(pic_timeout<PIC_TIMEOUT_LIMIT)
      {
        pic_timeout++;
      }
      else
      {
        pic_detected = 0;
      }
      pic_data_task();
    }
  }
  if(t100ms0>=1)
  {
    t100ms0=-4;
    /*if(light_sensor_detected)
    {
      unsigned int light_sensor_data0, light_sensor_data1;
      if(light_sensor.getData(light_sensor_data0,light_sensor_data1))
      {
        light_sensor_unsaturated = light_sensor.getLux(light_sensor_gain,light_sensor_int_time_ms,light_sensor_data0,light_sensor_data1,light_sensor_lux);
      }
    }*/
    //pic_uart_tx_sensordata();
  }
  if(t100ms1>=1)
  {
    t100ms1=-9;
    //pic_uart_tx_displaydata();
  }
  if(t100ms2>=1)
  {
    t100ms2=-9;
    pic_uart_tx_netdata();
  }
  if(t100ms3>=87)
  {
    t100ms3=-13;
    //pic_uart_tx_rtcdata();
  }
  if(t1s0)
  {
    t1s0=0;
    ntp_interval_count++;
    if(ntp_interval_count>NTP_INTERVAL)
    {
      updateNTP();
      Serial.println("NTP RESYNC");
      ntp_resync_count++;
      ntp_interval_count = 0;
      scheduler_sync = 0;
      pps_sync = 0;
    }
    /*
    if(env_sensor_detected)
    {
      env_temp_f = env_sensor.readTempC();
      env_pres_f = env_sensor.readFloatPressure();
      env_hum_f = env_sensor.readFloatHumidity();
    }
    */
  }
}
