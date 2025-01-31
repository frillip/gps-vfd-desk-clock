/**
 * Generated 16-bit Bootloader Source File
 * 
 * @file     memory_partition.S
 * 
 * @brief    Reserves a memory locations that the boot loader is not allowed to
 *           link into (the configuration page and the application memory space).
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

/* This code will block out the space reserved by the application so that the 
 * boot code can't link into that area.
 *
 * The value in the address() field defines the start address of the
 * application code.
 *
 * The number on the .space defines the size of this block in PC units (2 per
 * instruction).
 */
#include "boot_config.h"
   
    .section *, code, address(BOOT_CONFIG_PROGRAMMABLE_ADDRESS_LOW), noload, keep
reserved_application_memory:
    .space 0x2AFEA  -  BOOT_CONFIG_PROGRAMMABLE_ADDRESS_LOW, 0x00
