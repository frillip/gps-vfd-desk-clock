
/**
  OC2 Generated Driver API Source File

  @Company
    Microchip Technology Inc.

  @File Name
    oc2.c

  @Summary
    This is the generated source file for the OC2 driver using PIC24 / dsPIC33 / PIC32MM MCUs

  @Description
    This source file provides APIs for driver for OC2.
    Generation Information :
        Product Revision  :  PIC24 / dsPIC33 / PIC32MM MCUs - 1.170.0
        Device            :  dsPIC33EP512GP502
    The generated drivers are tested against the following:
        Compiler          :  XC16 v1.61
        MPLAB 	          :  MPLAB X v5.45
*/

/*
    (c) 2020 Microchip Technology Inc. and its subsidiaries. You may use this
    software and any derivatives exclusively with Microchip products.

    THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER
    EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY IMPLIED
    WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS FOR A
    PARTICULAR PURPOSE, OR ITS INTERACTION WITH MICROCHIP PRODUCTS, COMBINATION
    WITH ANY OTHER PRODUCTS, OR USE IN ANY APPLICATION.

    IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE,
    INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND
    WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS
    BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE. TO THE
    FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL CLAIMS IN
    ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF FEES, IF ANY,
    THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.

    MICROCHIP PROVIDES THIS SOFTWARE CONDITIONALLY UPON YOUR ACCEPTANCE OF THESE
    TERMS.
*/

/**
  Section: Included Files
*/

#include "oc2.h"

/** OC Mode.

  @Summary
    Defines the OC Mode.

  @Description
    This data type defines the OC Mode of operation.

*/

static uint16_t         gOC2Mode;

/**
  Section: Driver Interface
*/


void OC2_Initialize (void)
{
    // ENFLT0 disabled; ENFLT1 disabled; OCSIDL disabled; OCM Double Compare Continuous Pulse mode; OCFLT1 disabled; OCFLT0 disabled; OCTSEL FOSC/2; TRIGMODE Only Software; 
    OC2CON1 = 0x1C05;
    // SYNCSEL IC1; TRIGSTAT disabled; OCINV enabled; OCTRIG Sync; OC32 enabled; FLTOUT disabled; OCTRIS disabled; FLTMD Cycle; FLTTRIEN disabled; 
    OC2CON2 = 0x1110;
    // OC2RS 610; 
    OC2RS = 0x262;
    // OC2R 610; 
    OC2R = 0x262;
	
    gOC2Mode = OC2CON1bits.OCM;
}

void __attribute__ ((weak)) OC2_CallBack(void)
{
    // Add your custom callback code here
}

void OC2_Tasks( void )
{	
    if(IFS0bits.OC2IF)
    {
		// OC2 callback function 
		OC2_CallBack();
        IFS0bits.OC2IF = 0;
    }
}



void OC2_Start( void )
{
    OC2CON1bits.OCM = gOC2Mode;
}


void OC2_Stop( void )
{
    OC2CON1bits.OCM = 0;
}

void OC2_SecondaryValueSet( uint16_t secVal )
{
   
    OC2RS = secVal;
}


void OC2_PrimaryValueSet( uint16_t priVal )
{
   
    OC2R = priVal;
}

bool OC2_IsCompareCycleComplete( void )
{
    return(IFS0bits.OC2IF);
}


bool OC2_FaultStatusGet( OC2_FAULTS faultNum )
{
    bool status;
    /* Return the status of the fault condition */
   
    switch(faultNum)
    { 
        case OC2_FAULT0:
			status = OC2CON1bits.OCFLT0;
            break;
        case OC2_FAULT1:
			status = OC2CON1bits.OCFLT1;
            break;
        default :
            break;

    }
    return(status);
}


void OC2_FaultStatusClear( OC2_FAULTS faultNum )
{
    
    switch(faultNum)
    { 
        case OC2_FAULT0:
			OC2CON1bits.OCFLT0 = 0;
                break;
        case OC2_FAULT1:
			OC2CON1bits.OCFLT1 = 0;
                break;
        default :
                break;
    }    
}


void OC2_ManualTriggerSet( void )
{
    OC2CON2bits.TRIGSTAT= true; 
}

bool OC2_TriggerStatusGet( void )
{
    return( OC2CON2bits.TRIGSTAT );
}


void OC2_TriggerStatusClear( void )
{
    /* Clears the trigger status */
    OC2CON2bits.TRIGSTAT = 0;
}

/**
 End of File
*/
