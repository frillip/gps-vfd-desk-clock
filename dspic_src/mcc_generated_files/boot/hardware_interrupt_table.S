/**
 * Generated 16-bit Bootloader Source File
 * 
 * @file     hardware_interrupt_table.S
 * 
 * @brief    Generates an interrupt remapping table inside the application image.
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

/* This file makes a GOTO table for every remapped interrupt.  The GOTO
 * table will jump to the specified interrupt handler if it is defined in the
 * user code and will go to the DefaultInterrupt() handler otherwise.  Just like
 * the compiler normally would.
 */
    
#include "boot_config.h"
   
/*
 * .macro: defines a macro
 * .weak:  define \NAME i.e InterruptName as weak
 * .equiv: Assign the Interrupt to DefaultInterrupt if Interrupt definition does
 *         not exist
 * goto:   goto the Interrupt defined in the application if defined.
 */
    .equ remap_offset, 4
    
    .macro remap_to_application NAME
    .weak __\NAME
    .global __\NAME
    .equiv __\NAME, __remapped_DefaultInterrupt
    .section remapped\NAME, code, address(BOOT_CONFIG_USER_IVT_TABLE_ADDRESS+remap_offset), keep
    goto __\NAME
    .equ remap_offset, remap_offset + 4
    .endm
    
    .macro keep_in_bootloader NAME
    .endm
    
    .macro remap_to_application_default NAME
    .endm
    
    /* First entry in the remap table is the default interrupt, and it is
     * required and not implemented in hardware vectors, so we will implement
     * it here. */
    .weak __remapped_DefaultInterrupt
    .global __remapped_DefaultInterrupt
    .section __remapped_DefaultInterrupt, code, address(BOOT_CONFIG_USER_IVT_TABLE_ADDRESS), keep
    goto __DefaultInterrupt
    
    /* All other entries are hardware vectors and can be selectively forwarded
     * by the user.  Pull them in via the interrupt configuration file. */
    #define REMAP_TABLE
    .include "mcc_generated_files/boot/interrupts.S"
