#include "gnss_pps_esp.h"

HardwareSerial UARTGNSS(GNSS_UART);  //using UART1

uint16_t gnss_pps_offset_ms = 0;
bool gnss_detected = 0;
float gnss_lat = 0.0;
float gnss_long = 0.0;
bool gnss_fix_valid = 0;

uint32_t gnss_pps_micros = 0;
uint16_t gnss_timeout = 0;
bool gnss_new_pps = 0;
extern uint32_t esp_micros;
extern time_t gnss;


void gnss_pps_init(void)
{
  pinMode(GNSS_PPS_PIN,INPUT);
  attachInterrupt(GNSS_PPS_PIN, gnss_pps_in, RISING);
}


void ARDUINO_ISR_ATTR gnss_pps_in(void)
{
  gnss_pps_micros = micros();
  if(!gnss_detected) gnss_detected = 1;
  gnss++;
  gnss_new_pps = 1;
  gnss_timeout = 0;
}


bool gnss_is_detected(void)
{
  return gnss_detected;
}


void gnss_timeout_incr(void)
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

void print_gnss_pps_offset(Stream *output)
{
  int32_t gnss_offset_ms = gnss_pps_offset_ms;
  int32_t gnss_offset_micros = esp_micros - gnss_pps_micros;
  output->printf("GNSS offset: ");
  float gnss_offset_display = gnss_offset_micros;
  if(gnss_offset_display < -1000000) gnss_offset_display += 1000000;
  if(gnss_offset_display > 1000000) gnss_offset_display -= 1000000;
  if((gnss_offset_display < 1000) && (gnss_offset_display > -1000 ))
  {
    output->printf("%3.0fus\n", gnss_offset_display);
  }
  else
  {
    gnss_offset_display = gnss_offset_display / 1000;
    output->printf("%6.2fms\n", gnss_offset_display);
  }
}

void gnss_uart_init(void)
{
  UARTGNSS.begin(GNSS_BAUD, SERIAL_8N1, GNSS_RXD, GNSS_TXD);
}

bool gnss_uart_char_available(void)
{
  return UARTGNSS.available();
}

