/*
 * V2 board pin map. Code elsewhere uses the names here, which say what a pin
 * does, never the port name: change a pin and only this file and board.c
 * change.
 *
 *   Pin   Name                 Use                                   Driven by
 *   ----  -------------------  ------------------------------------  ----------------
 *   RA0   UI_SWITCH            alarm / DST switch, active low        input
 *   RA2   CLK_IN               40 MHz oscillator input               oscillator
 *   RA7   UI_BUTTON            push button, active low               input
 *   RA8   GNSS_PPS             receiver PPS (RPI24)                  IC1/IC2, IOC
 *   RA10  STATUS_LED           status LED                            GPIO
 *   RB2   GNSS_UART2_RX        from the receiver's TX (RP34)         UART2
 *   RB3   GNSS_UART2_TX        to the receiver's RX (RP35)           UART2
 *   RB5   PGED                 programming / debug data              ICSP
 *   RB6   PGEC                 programming / debug clock             ICSP
 *   RB7   UI_BUZZER            buzzer, active high                   GPIO
 *   RB8   I2C1_SCL             sensor and RTC bus clock              I2C1
 *   RB9   I2C1_SDA             sensor and RTC bus data               I2C1
 *   RB10  DISPLAY_LATCH_OC     the epoch output (RP42): latches the  OC1
 *                              tubes on the second, read back by
 *                              IC3/IC4 and timed by the receiver
 *   RB11  DISPLAY_BLANK        tube drivers' BLANK, high = blanked   OC3 (PWM) / GPIO
 *   RB12  DISPLAY_LATCH_GPIO   ORed with DISPLAY_LATCH_OC: latch now GPIO
 *   RB13  CAPTURE_SYNC_STROBE  not connected on the board: the       GPIO, briefly
 *                              start-up capture timebase check
 *                              pulses it (RPI45)
 *   RB14  PWR_STAT             power status, input                   input
 *   RB15  ESP_PPS              the ESP's PPS (RP47)                  IOC, IC1/IC2 for NTP
 *   RC6   ESP_UART1_RX         from the ESP32 (RP54)                 UART1
 *   RC7   ESP_UART1_TX         to the ESP32 (RP55)                   UART1
 *   RC8   DISPLAY_SPI2_SCK     tube drivers' clock (RP56)            SPI2
 *   RC9   DISPLAY_SPI2_SDO     tube drivers' data (RP57)             SPI2
 *
 * There is no direct console: UART1 is the link to the ESP32, which for now
 * acts as a serial bridge, so printf output (ui/console.c) reaches a terminal
 * through the ESP.
 */
#ifndef BOARD_H
#define BOARD_H

#include <xc.h>

// ---- Status and user interface ---------------------------------------------
#define STATUS_LED_SET_HIGH() (_LATA10 = 1)
#define STATUS_LED_SET_LOW()  (_LATA10 = 0)
#define STATUS_LED_TOGGLE()   (_LATA10 ^= 1)
#define STATUS_LED_SET(on)    (_LATA10 = (on) ? 1 : 0)

#define UI_BUTTON_IS_PRESSED() (_RA7 == 0) // pulled to ground when pressed
#define UI_SWITCH_IS_ON()      (_RA0 == 0) // pulled to ground when on

#define UI_BUZZER_SET_HIGH() (_LATB7 = 1)
#define UI_BUZZER_SET_LOW()  (_LATB7 = 0)

#define PWR_STAT_GET_VALUE() _RB14

// ---- Display ----------------------------------------------------------------
// The epoch output, read back. High during the 100 ms epoch pulse, while the
// tube drivers' latch is open.
#define DISPLAY_LATCH_OC_GET_VALUE() _RB10

#define DISPLAY_LATCH_GPIO_SET_HIGH() (_LATB12 = 1)
#define DISPLAY_LATCH_GPIO_SET_LOW()  (_LATB12 = 0)

// The value the BLANK pin takes while OC3 is off.
#define DISPLAY_BLANK_SET_HIGH() (_LATB11 = 1)
#define DISPLAY_BLANK_SET_LOW()  (_LATB11 = 0)

// ---- Timing -----------------------------------------------------------------
// Both PPS inputs are watched by change notification (timing/pps_monitor.c).
#define GNSS_PPS_GET_VALUE()     _RA8
#define GNSS_PPS_CHANGE_ENABLE() (CNENAbits.CNIEA8 = 1)
#define ESP_PPS_GET_VALUE()      _RB15
#define ESP_PPS_CHANGE_ENABLE()  (CNENBbits.CNIEB15 = 1)

#define CAPTURE_SYNC_STROBE_SET_HIGH()           (_LATB13 = 1)
#define CAPTURE_SYNC_STROBE_SET_LOW()            (_LATB13 = 0)
#define CAPTURE_SYNC_STROBE_SET_DIGITAL_INPUT()  (_TRISB13 = 1)
#define CAPTURE_SYNC_STROBE_SET_DIGITAL_OUTPUT() (_TRISB13 = 0)

/*
 * Remappable-input (RPINRx) numbers of the timing signals. The capture inputs
 * are switched between these by timing/pps_capture.c; the OC1 output mapping
 * is fixed and set in board_init().
 */
#define GNSS_PPS_RPIN            0x18U // RA8  = RPI24 -> IC1/IC2
#define DISPLAY_LATCH_OC_RPIN    0x2AU // RB10 = RP42  -> IC3/IC4 (epoch output loopback)
#define CAPTURE_SYNC_STROBE_RPIN 0x2DU // RB13 = RPI45 -> all ICs, start-up check only
#define ESP_PPS_RPIN             0x2FU // RB15 = RP47  -> IC1/IC2 while NTP is selected

void board_init(void);

#endif // BOARD_H
