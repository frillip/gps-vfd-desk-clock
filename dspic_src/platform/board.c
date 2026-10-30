#include <xc.h>
#include "board.h"

void board_init(void)
{
    // Pin output states, as V2: DISPLAY_BLANK high (tubes blanked until the
    // display starts), the UART TX pins idle high, everything else low.
    LATA = 0x0000;
    LATB = 0x0808;
    LATC = 0x0080;

    // Pin directions, as V2 (see the pin map in board.h)
    TRISA = 0x0397;
    TRISB = 0xE377;
    TRISC = 0x017F;

    // Pull-downs / pull-ups
    CNPDA = 0x0000;
    CNPDB = 0x0000;
    CNPDC = 0x0000;
    CNPUA = 0x0000;
    CNPUB = 0x0000;
    CNPUC = 0x0000;

    // Open drains
    ODCA = 0x0000;
    ODCB = 0x0000;
    ODCC = 0x0000;

    // ADC pins
    ANSELA = 0x0012;
    ANSELB = 0x0000; // RB2 is U2RX: must be digital
    ANSELC = 0x0007;

    // Set PPS
    __builtin_write_OSCCONL(OSCCON & 0xbf); // unlock PPS

    RPOR5bits.RP55R = 0x0001;   // ESP_UART1_TX (RC7) <- U1TX
    RPINR18bits.U1RXR = 0x0036; // ESP_UART1_RX (RC6) -> U1RX
    RPOR4bits.RP42R = 0x0010;   // DISPLAY_LATCH_OC (RB10) <- OC1, the epoch output
    RPOR0bits.RP35R = 0x0003;   // GNSS_UART2_TX (RB3) <- U2TX
    RPINR19bits.U2RXR = 0x0022; // GNSS_UART2_RX (RB2) -> U2RX
    RPOR6bits.RP57R = 0x0008;   // DISPLAY_SPI2_SDO (RC9) <- SDO2
    RPOR6bits.RP56R = 0x0009;   // DISPLAY_SPI2_SCK (RC8) <- SCK2
    RPINR22bits.SCK2R = 0x0038; // DISPLAY_SPI2_SCK (RC8) -> SCK2 input, as V2
    RPOR4bits.RP43R = 0x0012;   // DISPLAY_BLANK (RB11) <- OC3, brightness PWM

    __builtin_write_OSCCONL(OSCCON | 0x40); // lock PPS
}
