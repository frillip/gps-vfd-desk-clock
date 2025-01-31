/**
 * Generated 16-bit Bootloader Source File
 * 
 * @file     boot_demo.c
 * 
 * @brief    Example boot loader implementation.  This file determines:
 *           - when to stay in boot loader mode
 *           - when/how an application image is verified
 *           - when to jump to an application
 *           - when to load a backup application image
 *           - when to load a newly downloaded image (if there is a separate download image slot) 
 *
 *           It is intended that the user will replace/modify this file to meet their
 *           design needs.  This file provides an example implementation.
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
#include "boot_application_header.h"
#include "boot_image.h"
#include "boot_process.h"

#define DOWNLOADED_IMAGE    0u
#define EXECUTION_IMAGE     0u

static bool inBootloadMode = false;
static bool executionImageRequiresValidation = true;
static bool executionImageValid = false;

static bool EnterBootloadMode(void);

void BOOT_DEMO_Initialize(void)
{    
    
}


void BOOT_DEMO_Tasks(void)
{
    if(inBootloadMode == false)
    {
        if( EnterBootloadMode() == true )
        {
            inBootloadMode = true;
        }
        else
        {
            if( executionImageRequiresValidation == true )
            {
                executionImageValid = BOOT_ImageVerify(EXECUTION_IMAGE);
            }

            if(executionImageValid == false)
            {
                inBootloadMode = true;
            }

            if(inBootloadMode == false)
            {
                /* NOTE: Return all interrupt bits to their reset state before
                 * starting an application image. DO NOT disable the global 
                 * interrupt bit. All interrupt bits must be returned to their
                 * reset state.  The global interrupt enable bit resets to
                 * enabled, so should be returned to enabled before starting an
                 * application.  Most peripheral interrupt bits are disabled on
                 * reset.  All of these should be disabled before starting the 
                 * application.  Keep in mind that some software stacks may
                 * pull in and enable additional peripherals (e.g. - timers).  
                 */

                /* NOTE: Disable all peripherals before starting the application.
                 * Any peripheral left running could cause interrupt flags or bits
                 * to be set before the application software is initialized and
                 * can lead to unexpected system issues.
                 */

                #warning "Return device to reset state before starting the application.  Click on this warning for additional information to consider."
				 
                BOOT_StartApplication();
            }
        }
    }
    
    (void)BOOT_ProcessCommand();
}

static bool EnterBootloadMode(void)
{
    #warning "Update this function to return 'true' when you want to stay in the boot loader, and 'false' when you want to allow a release to the application code"
 
    /* NOTE: This might be a a push button status on power up, a command from a peripheral, 
     * or whatever is specific to your boot loader implementation */    

    return false;
}
