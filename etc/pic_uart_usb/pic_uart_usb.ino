#include <HardwareSerial.h>

#define DEBUG_UART  0
#define DEBUG_BAUD  115200

#define PIC_UART    2
#define PIC_BAUD    115200
#define PIC_RXD     13
#define PIC_TXD     12
#define PIC_PPS_PIN 27

HardwareSerial UARTPIC(PIC_UART);

void setup() {
  Serial.begin(DEBUG_BAUD);
  UARTPIC.begin(PIC_BAUD, SERIAL_8N1, PIC_RXD, PIC_TXD);
}

void loop()
{
  if(Serial.available())
  {
    UARTPIC.write(Serial.read());
  }

  if(UARTPIC.available())
  {
    Serial.write(UARTPIC.read());
  }
}
