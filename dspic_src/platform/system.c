#include "system.h"

// Configuration bits

// FICD
// ICD Communication Channel Select bits: Communicate on PGEC2 and PGED2
#pragma config ICS = PGD2
// JTAG Enable bit: JTAG is disabled
#pragma config JTAGEN = OFF

// FPOR
// Alternate I2C1 pins: I2C1 mapped to ASDA1/ASCL1 (RB9/RB8), where the RTC
// and sensors are wired. As V2. OFF would put I2C1 on SDA1/SCL1 instead.
#pragma config ALTI2C1 = ON
// Alternate I2C2 pins: I2C2 mapped to SDA2/SCL2 pins
#pragma config ALTI2C2 = OFF
// Watchdog Window Select bits: WDT Window is 25% of WDT period
#pragma config WDTWIN = WIN25

// FWDT
// Watchdog Timer Postscaler bits: 1:32768
#pragma config WDTPOST = PS32768
// Watchdog Timer Prescaler bit: 1:128
#pragma config WDTPRE = PR128
// PLL Lock Enable bit: Clock switch to PLL source will wait until the PLL lock signal is valid.
#pragma config PLLKEN = ON
// Watchdog Timer Window Enable bit: Watchdog Timer in Non-Window mode
#pragma config WINDIS = OFF
// Watchdog Timer Enable bit: Watchdog timer enabled/disabled by user software
#pragma config FWDTEN = OFF

// FOSC
// Primary Oscillator Mode Select bits: EC (External Clock) Mode
#pragma config POSCMD = EC
// OSC2 Pin Function bit: OSC2 is general purpose digital I/O pin
#pragma config OSCIOFNC = ON
// Peripheral pin select configuration: Allow multiple reconfigurations
#pragma config IOL1WAY = OFF
// Clock Switching Mode bits: Clock switching is enabled,Fail-safe Clock Monitor is disabled
#pragma config FCKSM = CSECMD

// FOSCSEL
// Oscillator Source Selection: FRC
#pragma config FNOSC = FRC
// Two-speed Oscillator Start-up Enable bit: Start up with user-selected oscillator source
#pragma config IESO = OFF

// FGS
// General Segment Write-Protect bit: General Segment may be written
#pragma config GWRP = OFF
// General Segment Code-Protect bit: General Segment Code protect is Disabled
#pragma config GCP = OFF

static void clock_init(void);

// RCON as found at start-up: why the PIC last reset.
static uint16_t reset_cause;

// RCON's reset-cause bits (the rest are controls and status: SWDTEN,
// VREGS, VREGSF, SLEEP, IDLE).
#define RCON_CAUSES 0xC2D3U // TRAPR IOPUWR CM EXTR SWR WDTO BOR POR

void system_init(void)
{
    // First, before anything can reset again: the bits stay set until
    // cleared, so clearing them now makes the next reset's cause its own.
    reset_cause = RCON;
    RCON = (uint16_t)(RCON & ~RCON_CAUSES);
    board_init();
    clock_init();
    // Interrupt priorities are set by the module that owns each interrupt:
    // console_init() for UART1, timing/ for the timers and captures.
    __builtin_enable_interrupts();
}


static void clock_init(void)
{
    // FRCDIV FRC/2; PLLPRE 8; DOZE 1:8; PLLPOST 1:4; DOZEN disabled; ROI disabled;
    CLKDIV = 0x3146;
    // TUN Center frequency;
    OSCTUN = 0x00;
    // ROON disabled; ROSEL FOSC; RODIV 0; ROSSLP disabled;
    REFOCON = 0x00;
    // PLLDIV 62;
    PLLFBD = 0x3E;
    // AD1MD enabled; T3MD enabled; T4MD enabled; T1MD enabled; U2MD enabled; T2MD enabled; U1MD enabled; SPI2MD enabled; SPI1MD enabled; C1MD enabled; T5MD enabled; I2C1MD enabled;
    PMD1 = 0x00;
    // IC4MD enabled; IC3MD enabled; OC1MD enabled; IC2MD enabled; OC2MD enabled; IC1MD enabled; OC3MD enabled; OC4MD enabled;
    PMD2 = 0x00;
    // CMPMD enabled; CRCMD enabled; I2C2MD enabled;
    PMD3 = 0x00;
    // CTMUMD enabled; REFOMD enabled;
    PMD4 = 0x00;
    // PTGMD enabled; DMA0MD enabled;
    PMD7 = 0x00;
    // CF no clock failure; NOSC PRIPLL; CLKLOCK unlocked; OSWEN Switch is Complete;
    __builtin_write_OSCCONH((uint8_t)(0x03));
    __builtin_write_OSCCONL((uint8_t)(0x01));
    // Wait for Clock switch to occur
    while (OSCCONbits.OSWEN != 0)
        ;
    while (OSCCONbits.LOCK != 1)
        ;
}


void delay_ms(uint16_t milliseconds)
{
    while (milliseconds--)
    {
        __delay_ms(1);
    }
}


void delay_us(uint16_t microseconds)
{
    while (microseconds >= 32)
    {
        __delay_us(32);
        microseconds -= 32;
    }

    while (microseconds--)
    {
        __delay_us(1);
    }
}


uint16_t system_reset_cause(void)
{
    return reset_cause;
}


void system_reset(void)
{
    __asm__ volatile("reset");
    while (1)
        ;
}
