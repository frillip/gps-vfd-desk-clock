/**
 * Generated 16-bit Bootloader Interface Header File
 * 
 * @file     boot_image.h
 * 
 * @brief    Defines interface for the accessing application image(s)
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


#ifndef BOOT_IMAGE_H
#define BOOT_IMAGE_H

#include <stdbool.h>
#include <stdint.h>
#include "boot_config.h"

#define DOWNLOAD_IMAGE_NUMBER 1u
#define BOOT_IMAGE_COUNT 2
#define BOOT_IMAGE_SIZE ((unsigned long)BOOT_CONFIG_DOWNLOAD_HIGH - (unsigned long)BOOT_CONFIG_DOWNLOAD_LOW + (unsigned long)2)

#define EXECUTABLE_IMAGE_FIRST_ADDRESS BOOT_CONFIG_PROGRAMMABLE_ADDRESS_LOW
#define EXECUTABLE_IMAGE_LAST_ADDRESS ((unsigned long)BOOT_CONFIG_PROGRAMMABLE_ADDRESS_LOW + (unsigned long)BOOT_IMAGE_SIZE - (unsigned long)2)

enum BOOT_IMAGE{
    BOOT_IMAGE_0 = 0,
    BOOT_IMAGE_1 = 1,
};

typedef enum NVM_RETURN_STATUS {
    NVM_SUCCESS,
    NVM_INVALID_LENGTH,
    NVM_INVALID_ADDRESS,
    NVM_WRITE_ERROR,
    NVM_READ_ERROR,
}NVM_RETURN_STATUS; 


bool IsLegalRange(uint32_t startRangeToCheck, uint32_t endRangeToCheck);
bool BOOT_ImageErase(enum BOOT_IMAGE image);
void BOOT_CopyLock(void);
void BOOT_CopyUnlock(void);
bool BOOT_ImageCopy(enum BOOT_IMAGE destinationImage, enum BOOT_IMAGE sourceImage);

bool BOOT_ImageVerify(enum BOOT_IMAGE image);
uint32_t BOOT_ImageAddressGet(enum BOOT_IMAGE image, uint32_t addressInExecutableImage);
uint16_t BOOT_EraseSizeGet();
NVM_RETURN_STATUS BOOT_BlockWrite(uint32_t nvmAddress, uint32_t lengthInBytes, uint8_t *sourceData, uint32_t key);
NVM_RETURN_STATUS BOOT_BlockRead (uint8_t *destinationData, uint32_t lengthInBytes, uint32_t nvmAddress);
NVM_RETURN_STATUS BOOT_BlockErase (uint32_t nvmAddress, uint32_t lengthInBytes, uint32_t key);

// Functions to read RAM data stored in flash in 16 bit values in each instruction location.
NVM_RETURN_STATUS BOOT_Read32Data (uint32_t *destinationData,  uint32_t nvmAddress);
NVM_RETURN_STATUS BOOT_Read16Data (uint16_t *destinationData,  uint32_t nvmAddress);
#endif