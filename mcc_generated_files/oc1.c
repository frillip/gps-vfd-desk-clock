
/**
  OC1 Generated Driver API Source File

  @Company
    Microchip Technology Inc.

  @File Name
    oc1.c

  @Summary
    This is the generated source file for the OC1 driver using PIC24 / dsPIC33 / PIC32MM MCUs

  @Description
    This source file provides APIs for driver for OC1.
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

#include "oc1.h"

/** OC Mode.

  @Summary
    Defines the OC Mode.

  @Description
    This data type defines the OC Mode of operation.

*/

static uint16_t         gOC1Mode;

/**
  Section: Driver Interface
*/


void OC1_Initialize (void)
{
    // ENFLT0 disabled; ENFLT1 disabled; OCSIDL disabled; OCM Double Compare Continuous Pulse mode; OCFLT1 disabled; OCFLT0 disabled; OCTSEL FOSC/2; TRIGMODE Hardware or Software; 
    OC1CON1 = 0x1C0D;
    // SYNCSEL Self; TRIGSTAT disabled; OCINV disabled; OCTRIG Sync; OC32 enabled; FLTOUT disabled; OCTRIS enabled; FLTMD Cycle; FLTTRIEN disabled; 
    OC1CON2 = 0x13F;
    // OC1RS 23039; 
    OC1RS = 0x59FF;
    // OC1R 23036; 
    OC1R = 0x59FC;
	
    gOC1Mode = OC1CON1bits.OCM;
}

void __attribute__ ((weak)) OC1_CallBack(void)
{
    // Add your custom callback code here
}

void OC1_Tasks( void )
{	
    if(IFS0bits.OC1IF)
    {
		// OC1 callback function 
		OC1_CallBack();
        IFS0bits.OC1IF = 0;
    }
}



void OC1_Start( void )
{
    OC1CON1bits.OCM = gOC1Mode;
}


void OC1_Stop( void )
{
    OC1CON1bits.OCM = 0;
}

void OC1_SecondaryValueSet( uint16_t secVal )
{
   
    OC1RS = secVal;
}


void OC1_PrimaryValueSet( uint16_t priVal )
{
   
    OC1R = priVal;
}

bool OC1_IsCompareCycleComplete( void )
{
    return(IFS0bits.OC1IF);
}


bool OC1_FaultStatusGet( OC1_FAULTS faultNum )
{
    bool status;
    /* Return the status of the fault condition */
   
    switch(faultNum)
    { 
        case OC1_FAULT0:
			status = OC1CON1bits.OCFLT0;
            break;
        case OC1_FAULT1:
			status = OC1CON1bits.OCFLT1;
            break;
        default :
            break;

    }
    return(status);
}


void OC1_FaultStatusClear( OC1_FAULTS faultNum )
{
    
    switch(faultNum)
    { 
        case OC1_FAULT0:
			OC1CON1bits.OCFLT0 = 0;
                break;
        case OC1_FAULT1:
			OC1CON1bits.OCFLT1 = 0;
                break;
        default :
                break;
    }    
}


void OC1_ManualTriggerSet( void )
{
    OC1CON2bits.TRIGSTAT= true; 
}

bool OC1_TriggerStatusGet( void )
{
    return( OC1CON2bits.TRIGSTAT );
}


void OC1_TriggerStatusClear( void )
{
    /* Clears the trigger status */
    OC1CON2bits.TRIGSTAT = 0;
}

/**
 End of File
*/
