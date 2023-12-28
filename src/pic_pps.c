#include "pic_pps.h"

uint16_t ic3_val = 0;
uint16_t ic3_val_old = 0;
uint16_t ic4_val = 0;
uint16_t ic4_val_old = 0;
uint32_t oc_count = 0;
int32_t oc_offset = 0;
uint32_t oc_count_diff = 0;
uint32_t oc_count_old = 0;
bool oc_event = 0;
uint32_t total_oc_seq_count = 0;
uint32_t sync_events = 0;

bool oc_adjust_in_progress = 0;
bool oc_adjust_fudge = 0;
bool pps_sync = 0;
bool pps_done = 0;

extern bool print_data;
extern bool rmc_waiting;
extern bool scheduler_sync;

extern uint32_t fosc_freq;

extern uint32_t pps_count;
extern uint32_t pps_count_diff;
extern uint32_t pps_seq_count;

extern int32_t accumulated_clocks;
extern time_t accumulation_start;
extern time_t accumulation_delta;

extern time_t utc;
time_t power_on_time = 0;

extern bool gnss_fix;

extern bool pps_sync;
extern bool scheduler_sync;
extern bool gnss_calendar_sync;
extern bool rtc_sync;

extern bool display_update_pending;

//extern float pdo_mv;
//extern float pps_offset_ns;

void pic_pps_init(void)
{
    IC4_Initialize();
    IC3_Initialize();
    pic_pps_set_latch_cycles(fosc_freq); // Set up our R & RS registers for OC1 & OC2
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

// Adjusts the trigger registers for OC1 and OC2
void pic_pps_set_latch_cycles(uint32_t cycles)
{
    // Split into two 16 bit values for OC1 and OC2
    uint16_t msb = ((cycles - 1) >> 16) & 0xFFFF;
    uint16_t lsb = (cycles - 1) & 0xFFFF;
    if(!msb)
    {
        msb = 1; // Make sure we have SOMETHING in MSB
        oc_adjust_fudge = 1; // Set the fudge flag so we can correct
    }
    OC1R = lsb - 50; // - 5000; // Latch pulse width is 5000 cycles
    OC1RS = lsb;
    OC2R = msb;// -1; // Investigate Latch/Blank/GPIO weirdness --- dodgy soldering. CHECK YOUR JOINTS, PEOPLE!!
    OC2RS = msb;
}

void pic_pps_calculate_oc_stats(void)
{
    oc_count = (((uint32_t)ic4_val)<<16) + ic3_val; // Raw timer
    oc_count_diff = oc_count - oc_count_old; // Difference from last
    oc_offset = pps_count-oc_count; // Calculate the offset between PPS and OC
    oc_count_old = oc_count; // Store the new value as old
}

void pic_pps_print_stats(void)
{
    printf("\r\n=== Clock and PPS stats ===\r\n");
    // Cycles between current and last PPS, and the OC offset from this
    double fosc_freq_f = ((float)fosc_freq * 10)/FCYCLE;
    printf("Crystal freq: %.06fMHz\r\n", fosc_freq_f);
    printf("PPS D:%lu OC D:%li\r\n", pps_count_diff, oc_offset);
    // Raw timer values for both PPS and OC
    printf("PPS C:%lu OC C:%li\r\n", pps_count, oc_count);
    // PPS sync status
    printf("PPS S: %i ADJ: %i\r\n", pps_sync, oc_adjust_in_progress);
    // Scheduler sync status
    printf("SCH S: %i GNSS FIX: %i\r\n", scheduler_sync, gnss_fix);
    // Accumulated clock data
    printf("CLK D: %li CLK T: %li\r\n",accumulated_clocks, accumulation_delta);
    
    uint32_t run_time = utc - power_on_time;
    uint16_t days = (run_time/86400);
    run_time = run_time - ((uint32_t)days*86400);
    uint8_t hours = (run_time/3600);
    run_time = run_time - ((uint32_t)hours*3600);
    uint8_t minutes = ((uint16_t)run_time/60);
    uint8_t seconds = ((uint16_t)run_time%60);
    
    printf("Up ");
    if(days) printf("%u days, ",days);
    printf("%02u:%02u:%02u since ",hours,minutes,seconds);
    ui_print_iso8601_string(power_on_time);
    printf("\r\n");
    printf("OC events: %lu Resync events: %lu\r\n",total_oc_seq_count, sync_events);
}

void pic_pps_evaluate_sync(void)
{
    if(accumulated_clocks > FCYCLE_ACC_LIM_POSITIVE || accumulated_clocks < FCYCLE_ACC_LIM_NEGATIVE)
    {
        if((accumulation_delta > FCYCLE_ACC_INTERVAL_MIN) && scheduler_sync)
        {
            recalculate_fosc_freq();
            printf("\r\nNew Fosc freq: %luHz\r\n", fosc_freq);
            printf("CLK D: %li CLK T: %li\r\n\r\n",accumulated_clocks, accumulation_delta);
            pic_pps_reset_sync();
            reset_pps_stats();
        }
        else if((accumulated_clocks > FCYCLE_ACC_RESET_POSITIVE) || (accumulated_clocks < FCYCLE_ACC_RESET_NEGATIVE))
        {
            recalculate_fosc_freq();
            printf("\r\nMajor frequency excursion...\r\n");
            printf("New Fosc freq: %luHz\r\n", fosc_freq);
            printf("CLK D: %li CLK T: %li\r\n\r\n",accumulated_clocks, accumulation_delta);
            pic_pps_reset_sync();
            reset_pps_stats();
        }
        else if(((oc_offset + OC_OFFSET_MAX) > FCYCLE_ACC_RESET_POSITIVE) || ((oc_offset + OC_OFFSET_MIN) < FCYCLE_ACC_RESET_NEGATIVE))
        {
            if(!oc_adjust_fudge && !oc_adjust_in_progress)
            {
                printf("\r\nOC unsynchronised... resetting\r\n");
                printf("CLK D: %li CLK T: %li\r\n",accumulated_clocks, accumulation_delta);
                printf("PPS D:%lu OC D:%li\r\n\r\n", pps_count_diff, oc_offset);
                if((accumulation_delta > FCYCLE_ACC_INTERVAL_MIN) && scheduler_sync)
                {
                    recalculate_fosc_freq();
                    printf("\r\nNew Fosc freq: %luHz\r\n", fosc_freq);
                }
                pic_pps_reset_sync();
                reset_pps_stats();
            }
        }
    }
}

bool pic_pps_manual_resync_available(void)
{
    if(accumulation_delta>PPS_MANUAL_RESYNC_INTERVAL) return 1;
    else return 0;
}

bool pic_pps_resync_required(void)
{
    return (!pps_sync && pps_seq_count>PPS_SEQ_COUNT_MIN);
}

void pic_pps_resync(void)
{
    pic_pps_set_latch_cycles(fosc_freq + oc_offset);
    oc_adjust_in_progress = 1;
    if(!accumulation_start)
    {
        accumulation_start = utc;
        accumulated_clocks = 0;
    }
    sync_events++;
}

void pic_pps_reset_sync(void)
{
    pps_sync = 0;
    scheduler_sync = 0;
    gnss_calendar_sync = 0;
    rtc_sync = 0;
}

void __attribute__ ( ( interrupt, no_auto_psv ) ) _ISR _IC3Interrupt( void )
{	
    if(IFS2bits.IC3IF)
    {
        // Only sync the scheduler after OC
        if(pps_sync && !scheduler_sync)
        {
            scheduler_align(fosc_freq); // Align the scheduler with our OC
        }
        if(oc_adjust_in_progress)
        {
            // Protect against bullshit maths
            if(fosc_freq>FCYCLE_UPPER_LIM||fosc_freq<FCYCLE_LOWER_LIM) fosc_freq = FCYCLE;
            if(oc_adjust_fudge)
            {
                pic_pps_set_latch_cycles(fosc_freq + oc_offset); // Correct our fudge
            }
            else
            {
                pic_pps_set_latch_cycles(fosc_freq); // Reset our OC to 1Hz
                pps_sync = 1; // Indicate we are now sync'd with PPS
            }
            oc_adjust_in_progress = 0; // Clear the adjustment flag
        }
        else if(oc_adjust_fudge)
        {
            // Protect against bullshit maths
            if(fosc_freq>FCYCLE_UPPER_LIM||fosc_freq<FCYCLE_LOWER_LIM) fosc_freq = FCYCLE;
            pic_pps_set_latch_cycles(fosc_freq); // Reset our OC to 1Hz
            oc_adjust_fudge = 0;
            pps_sync = 1; // Indicate we are now sync'd with PPS 
        }
        oc_event = 1; // Flag we've just had an OC event
        display_update_pending = 0; // display update no longer pending
        total_oc_seq_count++; // Increment oc event counter
        rmc_waiting = 0; // Invalidate any GNSS data that's waiting
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
