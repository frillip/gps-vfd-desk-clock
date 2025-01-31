/**
 * Generated 16-bit Bootloader Source File
 * 
 * @file     com_adaptor_uart.c
 * 
 * @brief    Connects the UART peripheral interface to the boot loader command
 *           processor.
 *
 * @skipline @version    16-bit Bootloader - 1.26.0
 *
 * @skipline             Device : dsPIC33EP256GP504
*/
/*
    (c) [2025] Microchip Technology Inc. and its subsidiaries.

    Subject to your compliance with these terms, you may use Microchip 
    software and any derivatives exclusively with Microchip products. 
    You are responsible for complying with 3rd party license terms  
    applicable to your use of 3rd party software (including open source  
    software) that may accompany Microchip software. SOFTWARE IS "AS IS." 
    NO WARRANTIES, WHETHER EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS 
    SOFTWARE, INCLUDING ANY IMPLIED WARRANTIES OF NON-INFRINGEMENT,  
    MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT 
    WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE, 
    INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY 
    KIND WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF 
    MICROCHIP HAS BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE 
    FORESEEABLE. TO THE FULLEST EXTENT ALLOWED BY LAW, MICROCHIP?S 
    TOTAL LIABILITY ON ALL CLAIMS RELATED TO THE SOFTWARE WILL NOT 
    EXCEED AMOUNT OF FEES, IF ANY, YOU PAID DIRECTLY TO MICROCHIP FOR 
    THIS SOFTWARE.
*/

#include <string.h>
#include "com_adaptor.h"
#include "boot_config.h"
#include "../uart1.h"

#define INVALID_PEEK_REQUEST_DATA 0x42

struct COM_DATA_STRUCT {
    uint8_t pendingCommand[BOOT_CONFIG_MAX_PACKET_SIZE];
    uint16_t pendingCommandLength;
};

static struct COM_DATA_STRUCT uartComData;


uint8_t BOOT_COM_Peek(uint16_t location)
{
    if(location >= uartComData.pendingCommandLength)
    {
        return INVALID_PEEK_REQUEST_DATA;
    }
    return uartComData.pendingCommand[location];
}

uint16_t BOOT_COM_Read(uint8_t* data, uint16_t length)
{
    uint16_t bytesToRead = BOOT_COM_GetBytesReady();
    
    if(length < bytesToRead)
    {
        bytesToRead = length;
    }
    
    memcpy(data, &uartComData.pendingCommand[0], bytesToRead);
    memmove(&uartComData.pendingCommand[0], &uartComData.pendingCommand[bytesToRead], BOOT_CONFIG_MAX_PACKET_SIZE - bytesToRead);
    uartComData.pendingCommandLength -= bytesToRead;

    return bytesToRead;
};

void BOOT_COM_Write(uint8_t* data, uint16_t length)
{
    while(length)
    {
        if (UART1_IsTxReady())
        {
            UART1_Write(*data++);
            length--;
        }
    }
    while (UART1_IsTxDone()==false);  
};


uint16_t BOOT_COM_GetBytesReady()
{
    static bool initilized=false;
    
    if (!initilized)
    {
        // ======================================================================================================
        // During com_adaptor_initialization the user may want to check the UART for any errors here before 
        // proceeding. Make sure the RX line is either driven high by the transmitter or pulled high via a pullup.
        // Failure to do so could cause the user to encounter frame errors or other line errors which can be 
        // difficult to debug.
        // ======================================================================================================      
        memset(&uartComData,0, sizeof(struct COM_DATA_STRUCT )/sizeof(uint8_t));
        initilized = true;
    }
      
    while ( UART1_IsRxReady() && (uartComData.pendingCommandLength < BOOT_CONFIG_MAX_PACKET_SIZE) )
    {
        uartComData.pendingCommand[uartComData.pendingCommandLength++]= UART1_Read();
    }

    return uartComData.pendingCommandLength;
}