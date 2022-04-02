#include "pic_pps.h"
#include "scheduler.h"

uint16_t ic3_val = 0;
uint16_t ic3_val_old = 0;
uint16_t ic4_val = 0;
uint16_t ic4_val_old = 0;
uint32_t oc_count = 0;
int32_t oc_offset = 0;
uint32_t oc_count_diff = 0;
uint32_t oc_count_old = 0;
bool oc_event = 0;

bool oc_adjust_in_progress = 0;
bool pps_sync = 0;
bool pps_done = 0;

extern bool print_data;
extern bool rmc_waiting;
extern bool scheduler_sync;

void pic_pps_init(void)
{
    IC4_Initialize();
    IC3_Initialize();
    set_latch_cycles(40000000UL); // Set up our R & RS registers for OC1 & OC2
    OC2_Initialize();
    OC1_Initialize();
}

void OC1_Initialize (void)
{
    // ENFLT0 disabled; ENFLT1 disabled; OCSIDL disabled; OCM Double Compare Continuous Pulse mode; OCFLT1 disabled; OCFLT0 disabled; OCTSEL FOSC/2; TRIGMODE Hardware or Software; 
    OC1CON1 = 0x1C0D;
    // SYNCSEL Self; TRIGSTAT disabled; OCINV disabled; OCTRIG Sync; OC32 enabled; FLTOUT disabled; OCTRIS enabled; FLTMD Cycle; FLTTRIEN disabled; 
    OC1CON2 = 0x13F;
}

void OC2_Initialize (void)
{
    // ENFLT0 disabled; ENFLT1 disabled; OCSIDL disabled; OCM Double Compare Continuous Pulse mode; OCFLT1 disabled; OCFLT0 disabled; OCTSEL FOSC/2; TRIGMODE Hardware or Software; 
    OC2CON1 = 0x1C0D;
    // SYNCSEL Self; TRIGSTAT disabled; OCINV disabled; OCTRIG Sync; OC32 enabled; FLTOUT disabled; OCTRIS disabled; FLTMD Cycle; FLTTRIEN disabled; 
    OC2CON2 = 0x11F;
}

void IC3_Initialize (void)
{
    // ICSIDL disabled; ICM Simple Capture mode: Falling; ICTSEL FOSC/2; ICI Every; 
    IC3CON1 = 0x1C02;
    // SYNCSEL PTGO10; TRIGSTAT disabled; IC32 enabled; ICTRIG Sync; 
    IC3CON2 = 0x10A;
    
    IFS2bits.IC3IF = false; // Clear the interrupt flag
    IEC2bits.IC3IE = true; // Enable interrupt
    IPC9bits.IC3IP = 1; // Set interrupt priority
}

void IC4_Initialize (void)
{
    // ICSIDL disabled; ICM Simple Capture mode: Falling; ICTSEL FOSC/2; ICI Every; 
    IC4CON1 = 0x1C02;
    // SYNCSEL PTGO11; TRIGSTAT disabled; IC32 enabled; ICTRIG Sync; 
    IC4CON2 = 0x10A;
    
    IFS2bits.IC4IF = false; // Clear the interrupt flag
    IEC2bits.IC4IE = true; // Enable interrupt
    IPC9bits.IC4IP = 1; // Set interrupt priority
}

void __attribute__ ( ( interrupt, no_auto_psv ) ) _ISR _IC3Interrupt( void )
{	
    if(IFS2bits.IC3IF)
    {
        // Only sync the scheduler after OC
        if(pps_sync && !scheduler_sync)
        {
            scheduler_align(); // Align the scheduler with our OC
        }
        if(oc_adjust_in_progress)
        {
            set_latch_cycles(40000000); // Reset our OC to 1Hz
            oc_adjust_in_progress = 0; // Clear the adjustment flag
            pps_sync = 1; // Indicate we are now sync'd with PPS
            print_data = 1; // Print stats after resync
        }
        oc_event = 1; // Flag we've just had an OC event
        rmc_waiting = 0; // Invalidate any GPS data that's waiting
        ic3_val = IC3BUF; // Read the IC3 timer
        IFS2bits.IC3IF = 0;
    }
}

void __attribute__ ( ( interrupt, no_auto_psv ) ) _ISR _IC4Interrupt( void )
{	
    if(IFS2bits.IC4IF)
    {
        ic4_val = IC4BUF; // Read the IC4 timer
        IFS2bits.IC4IF = 0;
    }
}

// Adjusts the trigger registers for OC1 and OC2
void set_latch_cycles(uint32_t cycles)
{
    // Split into two 16 bit values for OC1 and OC2
    uint16_t msb = ((cycles - 1) >> 16) & 0xFFFF;
    uint16_t lsb = (cycles - 1) & 0xFFFF;
    OC1R = lsb - 3; // Latch pulse width is 3 cycles
    OC1RS = lsb;
    OC2R = msb;
    OC2RS = msb;
}