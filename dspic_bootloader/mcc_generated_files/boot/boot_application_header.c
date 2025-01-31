/**
 * Generated 16-bit Bootloader Source File
 * 
 * @file     boot_application_header.c
 * 
 * @brief    Gives boot loader access to the information stored in the header
 *           of an application image.
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
#include "boot_image.h"
#include "boot_application_header.h"
#include "boot_config.h"



#define APPLICATION_RESET_REMAP_SIZE 4

/* Offset from the start of the application image to the application details section */
#define APPLICATION_DETAILS_OFFSET (BOOT_CONFIG_VERIFICATION_APPLICATION_HEADER_SIZE + APPLICATION_RESET_REMAP_SIZE)

enum BOOT_APPLICATION_DETAIL_ID_PRIVATE
{
    START_OF_APPLICATION_DETAIL_ID = 0x0000,
    END_OF_APPLICATION_DETAIL_ID = 0x0001
};   

/*
 * APPLICATION_DETAIL_PREFIX_SIZE is 6u
 * 2 bytes for the Detail ID
 * 4 bytes for the Detail Length
 */
#define APPLICATION_DETAIL_PREFIX_SIZE 6u

struct APPLICATION_DETAIL
{
    uint16_t id;
    uint32_t dataLength;
    uint32_t dataAddress;
};

static void ApplicationDetailPrefixGet(struct APPLICATION_DETAIL *entry, uint32_t address)
{
    BOOT_Read16Data (&entry->id,  address);
    BOOT_Read32Data (&entry->dataLength,  address + 2u);

    entry->dataAddress = address + APPLICATION_DETAIL_PREFIX_SIZE;
}

static bool IsEven(uint32_t address)
{
    return (address & 0x01) == 0;
}

static bool ApplicationDetailDataRead(struct APPLICATION_DETAIL *entry, uint16_t* dataBuffer, size_t bufferLength)
{
    size_t dataRemaining = entry->dataLength;
    uint32_t dataAddress = entry->dataAddress;
    bool valid = IsEven(dataRemaining);

    if(valid)
    {
        if(dataRemaining > bufferLength)
        {
            dataRemaining = bufferLength;
        }

        while(dataRemaining)
        {
            BOOT_Read16Data (dataBuffer,  dataAddress);
            dataBuffer++;
            dataAddress += 2;

            dataRemaining -= 2;
        }
    }

    return valid;
}

bool BOOT_ApplicationDetailGet(enum BOOT_IMAGE image, enum BOOT_APPLICATION_DETAIL_ID id, uint16_t* dataBuffer, size_t bufferLength)
{
    struct APPLICATION_DETAIL entry;
    uint32_t nextApplicationDetailAddress = BOOT_ImageAddressGet(image, BOOT_CONFIG_APPLICATION_IMAGE_APPLICATION_HEADER_ADDRESS + APPLICATION_DETAILS_OFFSET);
    bool entryFound = false;

    ApplicationDetailPrefixGet(&entry, nextApplicationDetailAddress);

    /* Verify the start of the expanded header */
    if((entry.id == START_OF_APPLICATION_DETAIL_ID) && (entry.dataLength == 2))
    {
        uint16_t countOfApplicationDetails;

        if(ApplicationDetailDataRead(&entry, &countOfApplicationDetails, 2))
        {
            unsigned int i;

            for(i=1; i<countOfApplicationDetails; i++)
            {
                nextApplicationDetailAddress += APPLICATION_DETAIL_PREFIX_SIZE + entry.dataLength;
                ApplicationDetailPrefixGet(&entry, nextApplicationDetailAddress);

                if(entry.id == id)
                {
                    entryFound = ApplicationDetailDataRead(&entry, dataBuffer, bufferLength);
                }

                if(entryFound == true)
                {
                    break;
                }
            }
        }
    }

    return entryFound;
}

bool BOOT_VersionNumberGet(enum BOOT_IMAGE image, uint32_t *versionNumber)
{
    memset(versionNumber, 0x00, sizeof(uint32_t));
    return BOOT_ApplicationDetailGet(image, BOOT_APPLICATION_DETAIL_ID_VERSION_NUMBER, (uint16_t*)versionNumber, sizeof(uint32_t));
}



