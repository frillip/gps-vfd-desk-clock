#include <Arduino.h>
#include <esp32-hal-timer.h>

#define GNSS_UART    1
#define GNSS_BAUD    115200
#define GNSS_RXD     17
#define GNSS_TXD     16
#define GNSS_PPS_PIN 18

#define GNSS_TIMEOUT_LIMIT 300 // in 0.01s counts

void gnss_pps_init(void);
void ARDUINO_ISR_ATTR gnss_pps_in(void);
bool gnss_is_detected(void);
void gnss_timeout_incr(void);
void print_gnss_pps_offset(void);

void gnss_uart_init(void);
bool gnss_uart_char_available(void);