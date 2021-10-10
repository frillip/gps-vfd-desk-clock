#include <string.h>
#include "mcc_generated_files/uart2.h"
#include "mcc_generated_files/uart1.h"
#include "gps.h"
#include "tubes.h"

char nmea_buffer[80] = {0};
uint8_t nmea_offset = 0;
uint8_t nmea_bytes = 0;
uint8_t nmea_checksum = 0;
uint8_t nmea_recieved_checksum = 0;

char rmc_buffer[80] = {0};
char rmc_string[7] = "$GNRMC,";
bool rmc_incoming = 0;
bool rmc_waiting = 0;

char gga_buffer[80] = {0};
char gga_string[7] = "$GNGGA,";
bool gga_incoming = 0;
bool gga_waiting = 0;

void rx_gps(void)
{
    char rx_char = 0;
    
    while(UART2_IsRxReady()) 
    {
        rx_char = UART2_Read();
        //UART1_Write(rx_char);
        nmea_buffer[nmea_offset] = rx_char;
        if((rx_char == 0x0a) || (nmea_offset>=79))
        {   
            uint8_t res;
            res = memcmp(nmea_buffer, rmc_string, 7);
            if(res==0)
            {
                rmc_waiting=1;
                memcpy(rmc_buffer, nmea_buffer, 80);
            }
            res = memcmp(nmea_buffer, gga_string, 7);
            if(res==0)
            {
                gga_waiting=1;
                memcpy(gga_buffer, nmea_buffer, 80);
            }
            memset(nmea_buffer, 0, sizeof nmea_buffer);
            nmea_offset=0;
        }
        else nmea_offset++;
    }
}