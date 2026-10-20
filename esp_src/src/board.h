/*
 * V2 board: ESP32 pin map. Pins are named for their function.
 *
 *   PIC link    UART2: RX GPIO13, TX GPIO12 (the PIC's UART1, SML)
 *   PIC epoch   GPIO27: the PIC's disciplined epoch output (RB10), rising edge
 *   ESP PPS     GPIO5: the ESP's NTP second, to the PIC (RB15)
 *   GNSS PPS    GPIO18: the receiver's PPS, through unpopulated solder jumpers
 *   Status LED  GPIO23: lit for 100 ms on each ESP PPS while NTP is synced
 *   Console     UART0 through the CP2102N
 */
#ifndef BOARD_H
#define BOARD_H

#define BOARD_CONSOLE_BAUD 921600

#define BOARD_PIC_UART      2
#define BOARD_PIC_BAUD      921600
#define BOARD_PIC_RX_PIN    13
#define BOARD_PIC_TX_PIN    12
#define BOARD_PIC_EPOCH_PIN 27

#define BOARD_ESP_PPS_PIN    5
#define BOARD_GNSS_PPS_PIN   18
#define BOARD_STATUS_LED_PIN 23

#endif // BOARD_H
