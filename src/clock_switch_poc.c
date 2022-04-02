/*
 * File:   main.c
 * Author: phil
 *
 * Created on 20 October 2021, 11:36
 */

// FICD
#pragma config ICS = PGD1               // ICD Communication Channel Select bits (Communicate on PGEC1 and PGED1)
#pragma config JTAGEN = OFF             // JTAG Enable bit (JTAG is disabled)

// FPOR
#pragma config ALTI2C1 = OFF            // Alternate I2C1 pins (I2C1 mapped to SDA1/SCL1 pins)
#pragma config ALTI2C2 = OFF            // Alternate I2C2 pins (I2C2 mapped to SDA2/SCL2 pins)
#pragma config WDTWIN = WIN25           // Watchdog Window Select bits (WDT Window is 25% of WDT period)

// FWDT
#pragma config WDTPOST = PS32768        // Watchdog Timer Postscaler bits (1:32,768)
#pragma config WDTPRE = PR128           // Watchdog Timer Prescaler bit (1:128)
#pragma config PLLKEN = ON              // PLL Lock Enable bit (Clock switch to PLL source will wait until the PLL lock signal is valid.)
#pragma config WINDIS = OFF             // Watchdog Timer Window Enable bit (Watchdog Timer in Non-Window mode)
#pragma config FWDTEN = OFF             // Watchdog Timer Enable bit (Watchdog timer enabled/disabled by user software)

// FOSC
#pragma config POSCMD = EC              // Primary Oscillator Mode Select bits (EC (External Clock) Mode)
#pragma config OSCIOFNC = OFF           // OSC2 Pin Function bit (OSC2 is clock output)
#pragma config IOL1WAY = OFF            // Peripheral pin select configuration (Allow multiple reconfigurations)
#pragma config FCKSM = CSECMD           // Clock Switching Mode bits (Clock switching is enabled,Fail-safe Clock Monitor is disabled)

// FOSCSEL
#pragma config FNOSC = FRC              // Oscillator Source Selection (Internal Fast RC (FRC))
#pragma config IESO = OFF               // Two-speed Oscillator Start-up Enable bit (Start up with user-selected oscillator source)

// FGS
#pragma config GWRP = OFF               // General Segment Write-Protect bit (General Segment may be written)
#pragma config GCP = OFF                // General Segment Code-Protect bit (General Segment Code protect is Disabled)

// #pragma config statements should precede project file includes.
// Use project enums instead of #define for ON and OFF.

#include <xc.h>

/*
 * Setup the clock to run at 70 MIPS from 10MHz crystal
 */
#define XTL   (10000000L)   /* external 10MHz clock */
#define FSRC  (XTL)
#define PLL_N1 (2L)         /* PLLPRE  CLKDIV<4:0> range 2 to 33 */
#define PLL_M  (32L)        /* PLLDIV  PLLFBD<8:0> range 2 to 513 */
#define PLL_N2 (2L)         /* PLLPOST CLKDIV<7:6> range 2, 4 or 8 */
#define FOSC (FSRC*PLL_M/(PLL_N1*PLL_N2))
#define FCYC (FOSC/2L)

#define FRC   (7370000L)    /* nominal fast RC frequency */
#define FRC_FSRC  (FRC)
#define FRC_FOSC (FRC_FSRC)
#define FRC_FCYC (FRC_FOSC/2L)
#define FRC_PLL_N1 (2L)     /* PLLPRE  CLKDIV<4:0> range 2 to 33 */
#define FRC_PLL_M  (87L)    /* PLLDIV  PLLFBD<8:0> range 2 to 513 */
#define FRC_PLL_N2 (2L)     /* PLLPOST CLKDIV<7:6> range 2, 4 or 8 */
#define FRCPLL_FOSC (FRC_FSRC*FRC_PLL_M/(FRC_PLL_N1*FRC_PLL_N2))
#define FRCPLL_FCYC (FRCPLL_FOSC/2L)

int tmr_cont = 0;

int main(void) {
    /****************************************************************************
     * Setting the Output Latch SFR(s)
     ***************************************************************************/
    LATA = 0x0000;
    LATB = 0x6880;

    /****************************************************************************
     * Setting the GPIO Direction SFR(s)
     ***************************************************************************/
    TRISA = 0x0017;
    TRISB = 0x934F;

    /****************************************************************************
     * Setting the Weak Pull Up and Weak Pull Down SFR(s)
     ***************************************************************************/
    CNPDA = 0x0001;
    CNPDB = 0x1400;
    CNPUA = 0x0000;
    CNPUB = 0x6000;

    /****************************************************************************
     * Setting the Open Drain SFR(s)
     ***************************************************************************/
    ODCA = 0x0000;
    ODCB = 0x0000;

    /****************************************************************************
     * Setting the Analog/Digital Configuration SFR(s)
     ***************************************************************************/
    ANSELA = 0x0002;
    ANSELB = 0x0000;
    
    //TMR2 0; 
    TMR2 = 0x00;
    //Period = 0.001 s; Frequency = 40000000 Hz; PR2 4999; 
    PR2 = 0x1387;
    //TCKPS 1:8; T32 16 Bit; TON enabled; TSIDL disabled; TCS FOSC/2; TGATE disabled; 
    T2CON = 0x8030;
    IFS0bits.T2IF = 0;
    IEC0bits.T2IE = 1;
    
        //TMR2 0; 
    TMR3 = 0x00;
    //Period = 0.001 s; Frequency = 40000000 Hz; PR2 4999; 
    PR3 = 0xFFFE;
    //TCKPS 1:8; T32 16 Bit; TON enabled; TSIDL disabled; TCS FOSC/2; TGATE disabled; 
    T3CON = 0x8030;
    IFS0bits.T3IF = 0;
    IEC0bits.T3IE = 1;
    
    _LATB13 = 1;
    tmr_cont = 0;
    TMR3 = 0x00;
    while(!tmr_cont);
    tmr_cont = 0;
    
    while(!_RA0)
    {
        _LATB13 = 0;
    }
    
    tmr_cont = 0;
    TMR3 = 0x00;
    while(!tmr_cont);
    tmr_cont = 0;
    _LATB13 = 1; // Running FRC
    
    /* Test if possible to change clock source */
    if(OSCCONbits.CLKLOCK == 0 )
    {
        register unsigned int CS_Timeout;
    
        /* Select FRC as the CPU clock source */
        _LATB13 ^= 1; // Running FRC
        __builtin_write_OSCCONH(0b000);
    
        /* request clock switch */
        __builtin_write_OSCCONL(OSCCON | _OSCCON_OSWEN_MASK);
    
        /* wait for oscillator switch to complete */
        for(CS_Timeout=60000; CS_Timeout; --CS_Timeout) if(!OSCCONbits.OSWEN) break;
        
        _LATB13 ^= 1; // Running FRC
                /* configure PLL register */
        tmr_cont = 0;
        TMR3 = 0x00;
        while(!tmr_cont);
        tmr_cont = 0;
        CLKDIVbits.DOZE = 0;
        CLKDIVbits.DOZEN = 0;
        CLKDIVbits.PLLPRE = FRC_PLL_N1-2; /* set crystal oscillator prescale */
        #if   FRC_PLL_N2==2
          CLKDIVbits.PLLPOST=0; /* N2=2 */
        #elif FRC_PLL_N2==4
          CLKDIVbits.PLLPOST=1; /* N2=4 */
        #elif FRC_PLL_N2==8
          CLKDIVbits.PLLPOST=3; /* N2=8 */
        #else
          #error invalid PLL_N2 paramenter
        #endif
        PLLFBDbits.PLLDIV = FRC_PLL_M-2; /* set PLL multiplier */
    
        /* select FRC with PLL as the CPU clock source */
        __builtin_write_OSCCONH(0b001);
    
        /* request clock switch */
        __builtin_write_OSCCONL(OSCCON | _OSCCON_OSWEN_MASK);
    
        /* wait for oscillator switch to complete */
        for(CS_Timeout=60000; CS_Timeout; --CS_Timeout) if(!OSCCONbits.OSWEN) break;
        _LATB13 ^= 1; // Running FRC + PLL
        tmr_cont = 0;
        TMR3 = 0x00;
        while(!tmr_cont);
        tmr_cont = 0;
    
        /* configure PLL register */
        _LATB13 ^= 1;
        CLKDIVbits.DOZE = 0;
        CLKDIVbits.DOZEN = 0;
        CLKDIVbits.PLLPRE = PLL_N1-2; /* set crystal oscillator prescale */
        #if   PLL_N2==2
          CLKDIVbits.PLLPOST=0; /* N2=2 */
        #elif PLL_N2==4
          CLKDIVbits.PLLPOST=1; /* N2=4 */
        #elif PLL_N2==8
          CLKDIVbits.PLLPOST=3; /* N2=8 */
        #else
          #error invalid PLL_N2 paramenter
        #endif
        PLLFBDbits.PLLDIV = PLL_M-2; /* set PLL multiplier */
    
        /* select primary oscillator with PLL as the CPU clock source */
        __builtin_write_OSCCONH(0b011);
    
        /* request clock switch */
        __builtin_write_OSCCONL(OSCCON | _OSCCON_OSWEN_MASK);
    
        /* wait for oscillator switch to complete */
        for(CS_Timeout=60000; CS_Timeout; --CS_Timeout) if(!OSCCONbits.OSWEN) break;
        _LATB13 ^= 1; // Running EC PLL
        tmr_cont = 0;
        TMR3 = 0x00;
        while(!tmr_cont);
        tmr_cont = 0;
        _LATB13 ^= 1; // Running ECPLL
    
        /* lock in this clock source */
        __builtin_write_OSCCONL(OSCCON | _OSCCON_LOCK_MASK);
        
        

    }
    _LATB13 ^= 1; // Running locked 
    while(1);
    return 0;
}

void __attribute__ ( ( interrupt, no_auto_psv ) ) _T2Interrupt (  )
{
    _LATB5 ^= 1;
    IFS0bits.T2IF = 0;
}

void __attribute__ ( ( interrupt, no_auto_psv ) ) _T3Interrupt (  )
{
    tmr_cont = 1;
    IFS0bits.T3IF = 0;
}