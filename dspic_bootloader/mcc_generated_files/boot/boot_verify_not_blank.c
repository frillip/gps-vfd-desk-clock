/**
 * Generated 16-bit Bootloader Source File
 * 
 * @file     boot_verify_not_blank.c
 * 
 * @brief    Verifies an application image via a simple "not blank" check.  It
 *           looks at the reset address of the application image to determine if
 *           it is blank or not.
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


#include <stdbool.h>
#include <stdint.h>
#include "boot_config.h"
#include "boot_image.h"


bool BOOT_ImageVerify(enum BOOT_IMAGE image)
{
    /* We are doing a blank check, so let's just test the first address of
     * memory to see if it is blank or not.  The first address is programmed 
     * last by the example Unified Bootloader Host Application (UBHA) tool so
     * if this address is programmed, then the device programming should be
     * complete.
     * 
     * NOTE: This method only checks that the first address of memory is
     * programmed.  It does not check the integrity of any of the program data.
     * To select a more complete verification method, go to the bootloader
     * MCC module and make a different verification method selection.
     */

    uint32_t data;

    if( image >= BOOT_IMAGE_COUNT )
    {
        return false;
    }

    BOOT_BlockRead ((uint8_t*)&data, 4, BOOT_ImageAddressGet(image, BOOT_CONFIG_APPLICATION_RESET_ADDRESS));
    return (data != 0x00FFFFFF);
}

bool BOOT_Verify(void)
{
    return BOOT_ImageVerify(0);
}
