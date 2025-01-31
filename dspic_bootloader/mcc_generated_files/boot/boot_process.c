/**
 * Generated 16-bit Bootloader Source File
 * 
 * @file     boot_process.c
 * 
 * @brief    Boot loader command processor.  Receives commands from the communications
 *           adapter and executes them appropriately.
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

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "com_adaptor.h"
#include "boot_private.h"
#include "boot_process.h"
#include "boot_config.h"
#include "boot_image.h"


#if ((BOOT_CONFIG_MAX_PACKET_SIZE - SIZE_OF_CMD_STRUCT_0) < MINIMUM_WRITE_BLOCK_SIZE )
#error "The maximum packet size is not large enough to store a full write block plus header. Make the max packet size larger."
#endif


static uint8_t commandArray[BOOT_CONFIG_MAX_PACKET_SIZE];

enum BOOT_COMMAND_RESPONSES
{
    COMMAND_SUCCESS = 0x01,
    UNSUPPORTED_COMMAND = 0xFF,
    BAD_ADDRESS = 0xFE,
    BAD_LENGTH  = 0xFD,
    VERIFY_FAIL = 0xFC
};

enum BOOT_COMMAND
{
    READ_VERSION = 0x00,
    READ_FLASH = 0x01,
    WRITE_FLASH = 0x02,
    ERASE_FLASH = 0x03,
    CALC_CHECKSUM = 0x08,
    RESET_DEVICE = 0x09,
    SELF_VERIFY = 0x0A,
    GET_MEMORY_ADDRESS_RANGE_COMMAND = 0x0B
};



/******************************************************************************/
/* Private Function Prototypes                                                */
/******************************************************************************/

static enum BOOT_COMMAND_RESULT CommandError(enum BOOT_COMMAND_RESPONSES);
static enum BOOT_COMMAND_RESULT ReadVersion(void);

static void ResetDevice(void);
static enum BOOT_COMMAND_RESULT EraseFlash(void);
static enum BOOT_COMMAND_RESULT WriteFlash(void);
static enum BOOT_COMMAND_RESULT ReadFlash(void);
static enum BOOT_COMMAND_RESULT CalculateChecksum(void);
static enum BOOT_COMMAND_RESULT SelfVerify(void);
static enum BOOT_COMMAND_RESULT GetMemoryAddressRange(void);

/******************************************************************************/
/* Public Functions                                                           */
/******************************************************************************/
void BOOT_Initialize() 
{
}


enum BOOT_COMMAND_RESULT BOOT_ProcessCommand(void)
{    
    uint16_t bytes_ready = BOOT_COM_GetBytesReady();
    uint8_t command;
    uint16_t command_length;

    if (bytes_ready == 0u) 
    { 
        return BOOT_COMMAND_NONE; 
    }
    if (bytes_ready < sizeof(struct CMD_STRUCT_0)) 
    { 
        return BOOT_COMMAND_INCOMPLETE; 
    }
    
    command = BOOT_COM_Peek(0);

    // validate the length of the command will not exceed the buffer size
    command_length = BOOT_COM_Peek(1u) + BOOT_COM_Peek(2u)*256u + sizeof(struct CMD_STRUCT_0);
    if ( ( command_length > BOOT_CONFIG_MAX_PACKET_SIZE ) && ( command != ERASE_FLASH ) )
    {
        return CommandError(BAD_LENGTH);
    }

    switch (command)
    {
        
    case WRITE_FLASH:
        return WriteFlash();

    case READ_FLASH:
        return ReadFlash();

    case READ_VERSION:
        return ReadVersion();

    case ERASE_FLASH:
        return EraseFlash();

    case RESET_DEVICE:
        ResetDevice();
        return BOOT_COMMAND_SUCCESS;

    case CALC_CHECKSUM:
        return CalculateChecksum();

    case SELF_VERIFY:
        return SelfVerify();

     
    case GET_MEMORY_ADDRESS_RANGE_COMMAND:
        return GetMemoryAddressRange();

    default:
        return CommandError(UNSUPPORTED_COMMAND);
    }
    
    return BOOT_COMMAND_ERROR;
}

void BOOT_StartApplication()
{
    int (*user_application)(void);
    user_application = (int(*)(void))BOOT_CONFIG_APPLICATION_RESET_ADDRESS;
    user_application();       
}  

/******************************************************************************/
/* Private Functions                                                          */
/******************************************************************************/


static enum BOOT_COMMAND_RESULT CommandError(enum BOOT_COMMAND_RESPONSES errorType)
{
    struct RESPONSE_TYPE_0 response;
    
    (void)BOOT_COM_Read(commandArray, sizeof(struct CMD_STRUCT_0));
    memcpy(&response, commandArray, sizeof(struct CMD_STRUCT_0));
    response.success = errorType;
    BOOT_COM_Write((uint8_t*) & response, sizeof (struct RESPONSE_TYPE_0 ) / sizeof (uint8_t));
    
    return BOOT_COMMAND_ERROR;
}

static enum BOOT_COMMAND_RESULT ReadVersion(void)
{
    struct GET_VERSION_RESPONSE response = {
        .cmd = 0,
        .dataLength = 0,
        .unlockSequence = 0,
        .address = 0,
        .version = BOOT_CONFIG_VERSION,
        .maxPacketLength = BOOT_CONFIG_MAX_PACKET_SIZE,
        .unused1 = 0,
        .deviceId = 0x3456u,
        .unused2 = 0,
        .writeSize = MINIMUM_WRITE_BLOCK_SIZE,
        .unused3 = 0,
        .userRsvdStartSddress = 0,
        .userRsvdEndSddress = 0
    };
    response.eraseSize = BOOT_EraseSizeGet();
    
    (void)BOOT_COM_Read(commandArray, sizeof(struct CMD_STRUCT_0));
    BOOT_COM_Write((uint8_t*) & response, sizeof (struct GET_VERSION_RESPONSE ) / sizeof (uint8_t));

    return BOOT_COMMAND_SUCCESS;
}


static void Reset(void){
    asm ("reset");
}

static void ResetDevice(void)
{
    struct RESPONSE_TYPE_0 response = {
        .cmd = 9,
        .dataLength = 0,
        .unlockSequence = 0,
        .address = 0,

        .success = COMMAND_SUCCESS
    };         
    
    (void)BOOT_COM_Read(commandArray, sizeof(struct CMD_STRUCT_0));

    BOOT_COM_Write((uint8_t*) & response, sizeof (struct RESPONSE_TYPE_0) / sizeof (uint8_t));
    
    Reset();
 }


static enum BOOT_COMMAND_RESULT EraseFlash(void)
{
    struct RESPONSE_TYPE_0 response;
    struct CMD_STRUCT_0 *pCommand = (struct CMD_STRUCT_0*) commandArray; 
    
    (void)BOOT_COM_Read(commandArray, sizeof(struct CMD_STRUCT_0));
            
    memcpy(&response, commandArray, sizeof(struct CMD_STRUCT_0));

    response.success = BAD_ADDRESS;
    if ( BOOT_BlockErase(pCommand->address, pCommand->dataLength, pCommand->unlockSequence) == NVM_SUCCESS)
    {
        response.success = COMMAND_SUCCESS;
    }     
    
    BOOT_COM_Write((uint8_t*) & response, sizeof (struct RESPONSE_TYPE_0) / sizeof (uint8_t));

    /* destroy the unlock key so it isn't sitting around in memory. */
    pCommand->unlockSequence = ~pCommand->unlockSequence;
    
    if(response.success == COMMAND_SUCCESS)
    {
        return BOOT_COMMAND_SUCCESS;
    }
    
    return BOOT_COMMAND_ERROR;
}

static enum BOOT_COMMAND_RESULT WriteFlash(void)
{
    struct CMD_STRUCT_0_WITH_PAYLOAD *pCommand = (struct CMD_STRUCT_0_WITH_PAYLOAD*) commandArray;
    struct RESPONSE_TYPE_0 response;
    uint16_t dataLength;
    
    dataLength = BOOT_COM_Peek(2u)<<8u;
    dataLength |= BOOT_COM_Peek(1u);
    
    if (BOOT_COM_GetBytesReady() < (sizeof(struct CMD_STRUCT_0) + dataLength))
    { 
        return BOOT_COMMAND_INCOMPLETE; 
    }  
    
    (void)BOOT_COM_Read(commandArray, sizeof(struct CMD_STRUCT_0) + dataLength);

    memcpy(&response, commandArray, sizeof(struct CMD_STRUCT_0));
    
    response.success = COMMAND_SUCCESS;


    if (dataLength <= (BOOT_CONFIG_MAX_PACKET_SIZE - sizeof(struct CMD_STRUCT_0)))
    {
        if (BOOT_BlockWrite(pCommand->address, dataLength, &pCommand->data[0], pCommand->unlockSequence) != NVM_SUCCESS)
        {
            response.success = BAD_ADDRESS;   
        } 
    }   
    else
    {
        response.success = BAD_ADDRESS;
    }
    
    BOOT_COM_Write((uint8_t*) & response, sizeof (struct RESPONSE_TYPE_0) / sizeof (uint8_t));
    
    /* destroy the unlock key so it isn't sitting around in memory. */
    pCommand->unlockSequence = ~pCommand->unlockSequence;
    
    if(response.success == COMMAND_SUCCESS)
    {
        return BOOT_COMMAND_SUCCESS;
    }
    
    return BOOT_COMMAND_ERROR;
}
static enum BOOT_COMMAND_RESULT ReadFlash(void)
{
    struct RESPONSE_TYPE_0_WITH_PAYLOAD response;
    struct CMD_STRUCT_0_WITH_PAYLOAD * const pCommand = (struct CMD_STRUCT_0_WITH_PAYLOAD*) commandArray;
    
   (void)BOOT_COM_Read(commandArray, sizeof(struct CMD_STRUCT_0) );

    memcpy(&response, commandArray, sizeof(struct CMD_STRUCT_0));

    if (pCommand->dataLength <= (BOOT_CONFIG_MAX_PACKET_SIZE - sizeof(struct CMD_STRUCT_0) - 1u ))
    {
        NVM_RETURN_STATUS readStatus = BOOT_BlockRead ((uint8_t *)&response.data[0], pCommand->dataLength, pCommand->address );
        if (readStatus == NVM_SUCCESS)
        {    
            response.success = COMMAND_SUCCESS;
        } 
        else
        {
            response.success = BAD_ADDRESS;
            response.dataLength = 0u;        
        }
            
    } 
    else
    {
        response.success = BAD_ADDRESS;
        response.dataLength = 0u;        
    }
    
    BOOT_COM_Write((uint8_t*) & response, (sizeof (struct RESPONSE_TYPE_0) / sizeof (uint8_t)) + response.dataLength  );

    if(response.success == COMMAND_SUCCESS)
    {
        return BOOT_COMMAND_SUCCESS;
    }
    
    return BOOT_COMMAND_ERROR;
}


static enum BOOT_COMMAND_RESULT CalculateChecksum(void)
{
    struct RESPONSE_TYPE_0_2_PAYLOAD response;
    struct CMD_STRUCT_0 *pCommand = (struct CMD_STRUCT_0*) commandArray;
    uint32_t flashData;
    uint32_t checksum = 0;
    uint16_t count;
    
    (void)BOOT_COM_Read(commandArray, sizeof(struct CMD_STRUCT_0) );

    memcpy(&response, commandArray, sizeof(struct CMD_STRUCT_0));
    
    if ( IsLegalRange(pCommand->address, pCommand->address + pCommand->dataLength/2 - 1u))
    {
        for (count = 0; count < pCommand->dataLength; count += 4u)
        {
            BOOT_BlockRead ((uint8_t*)&flashData, 4u,  pCommand->address + (count/2u));
            checksum += (flashData & 0xFFFFu) + ((flashData>>16u) & 0xFFu);;
        }
        response.data = (checksum & 0xFFFFu);

        response.success = COMMAND_SUCCESS;
        BOOT_COM_Write((uint8_t*) & response, (sizeof (struct RESPONSE_TYPE_0_2_PAYLOAD) / sizeof (uint8_t)) );  
    }
    else
    {
        response.success=BAD_ADDRESS;
        response.dataLength = 0;
        BOOT_COM_Write((uint8_t*) & response, (sizeof (struct RESPONSE_TYPE_0) / sizeof (uint8_t)) );  
    }
    
    if(response.success == COMMAND_SUCCESS)
    {
        return BOOT_COMMAND_SUCCESS;
    }
    
    return BOOT_COMMAND_ERROR;
} 

static enum BOOT_COMMAND_RESULT SelfVerify(void)
{
    struct RESPONSE_TYPE_0_2_PAYLOAD response;
    
    (void)BOOT_COM_Read((uint8_t*)&response, sizeof(struct CMD_STRUCT_0) );

    if(BOOT_ImageVerify(DOWNLOAD_IMAGE_NUMBER) == false)
    {
        response.success = VERIFY_FAIL;
    }
    else
    {
        response.success = COMMAND_SUCCESS;
    }

    response.dataLength = 0;
    BOOT_COM_Write((uint8_t*) &response, (sizeof (struct RESPONSE_TYPE_0) / sizeof (uint8_t)) );  
    
    if(response.success == COMMAND_SUCCESS)
    {
        return BOOT_COMMAND_SUCCESS;
    }
    
    return BOOT_COMMAND_ERROR;
} 

static enum BOOT_COMMAND_RESULT GetMemoryAddressRange(void)
{
   struct GET_MEMORY_ADDRESS_RANGE_RESPONSE response = {
        .cmd = 0xB,
        .dataLength = 0x8,
        .unlockSequence = 0x0,
        .address = 0,   
        .success = 1,
        .programFlashStart = EXECUTABLE_IMAGE_FIRST_ADDRESS,
        .programFlashEnd = EXECUTABLE_IMAGE_LAST_ADDRESS
    };
    (void)BOOT_COM_Read(commandArray, sizeof(struct CMD_STRUCT_0));
    BOOT_COM_Write((uint8_t*) & response, sizeof (struct GET_MEMORY_ADDRESS_RANGE_RESPONSE ) / sizeof (uint8_t));

    return BOOT_COMMAND_SUCCESS;
}



   



