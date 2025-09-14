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
extern bool scheduler_sync;

extern uint32_t fosc_freq;

extern uint32_t pps_count;
extern uint32_t pps_count_diff;
extern uint32_t pps_seq_count;

extern int32_t accumulated_clocks;
extern time_t accumulation_start;
extern time_t accumulation_delta;
extern double accumulated_clocks_diff_avg;
extern int32_t accumulated_clocks_diff[FCYCLE_ACC_AVG_PERIOD];

extern time_t utc;
extern time_t local;
extern time_t rtc;
time_t power_on_time = 0;

extern bool gnss_fix;

extern bool pps_sync;
extern bool scheduler_sync;
extern bool gnss_calendar_sync;
extern bool rtc_sync;

extern bool esp_detected;
extern uint32_t ntp_seq_count;

extern bool display_update_pending;

extern CLOCK_SYNC_STATUS clock_sync_state;

extern uint16_t t10ms_display;

//extern float pdo_mv;
//extern float pps_offset_ns;

void pic_pps_init(void)
{
    IC4_Initialize();
    IC3_Initialize();
    pic_pps_set_latch_cycles(fosc_freq); // Set up our R & RS registers for OC1 & OC2
    OC2_Initialize();
    OC1_Initialize();
    //pic_pps_timer_init();
}

/* 32-bit timer experiment

void pic_pps_timer_init(void)
{
    T5CONbits.TON = 0; // Stop any 16-bit Timer3 operation
    T4CONbits.TON = 0; // Stop any 16/32-bit Timer3 operation
    T4CONbits.T32 = 1; // Enable 32-bit Timer mode
    T4CONbits.TCS = 0; // Select internal instruction cycle clock
    T4CONbits.TGATE = 0; // Disable Gated Timer mode
    T4CONbits.TCKPS = 0b00; // Select 1:1 Prescaler
    TMR5 = 0x00; // Clear 32-bit Timer (msw)
    TMR4 = 0x00; // Clear 32-bit Timer (lsw)
    PR5 = 0x0262; // Load 32-bit period value (msw)
    PR4 = 0x59FF; // Load 32-bit period value (lsw)
    IPC7bits.T5IP = 0x01; // Set Timer3 Interrupt Priority Level
    IFS1bits.T5IF = 0; // Clear Timer3 Interrupt Flag
    IEC1bits.T5IE = 1; // Enable Timer3 interrupt
    T4CONbits.TON = 1; // Start 32-bit Timer
}

void __attribute__((__interrupt__, no_auto_psv)) _T5Interrupt(void)
{
    //STATUS_LED_Toggle();
    IFS1bits.T5IF = 0; //Clear Timer3 interrupt flag
}
*/

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
    OC1R = lsb - 50; // 1.25us
    OC1RS = lsb;
    OC2R = msb;
    OC2RS = msb;
}

uint32_t oc_offset_correction_count_pos = 0;
uint32_t oc_offset_correction_count_neg = 0;

void pic_pps_calculate_oc_stats(void)
{
    oc_count = (((uint32_t)ic4_val)<<16) + ic3_val; // Raw timer
    oc_count_diff = oc_count - oc_count_old; // Difference from last
    oc_offset = pps_count-oc_count; // Calculate the offset between PPS and OC
    while(oc_offset<OC_OFFSET_NEGATIVE_LIMIT)
    {
        oc_offset += fosc_freq; // Account for missing GNSS events
        oc_offset_correction_count_neg++;
    }
    while(oc_offset>OC_OFFSET_POSITIVE_LIMIT)
    {
        oc_offset -= fosc_freq; // Catch for if we're unsync'd and in the middle somewhere
        oc_offset_correction_count_pos++;
    }
    oc_count_old = oc_count; // Store the new value as old
}

extern uint32_t pps_seq_count_old;
extern bool pps_missing;
extern uint32_t pps_missing_count;

void pic_pps_print_stats(void)
{
    printf("\n=== Clock and PPS stats ===\n");
    // Cycles between current and last PPS, and the OC offset from this
    double fosc_freq_f = ((float)fosc_freq * XTAL_FREQ_MHZ)/FCYCLE;
    printf("Crystal freq: %.06fMHz\n", fosc_freq_f);
    printf("PPS D:%lu OC D:%li\n", pps_count_diff, oc_offset);
    printf("OC ADJ+:%lu OC ADJ-:%lu\n", oc_offset_correction_count_pos, oc_offset_correction_count_neg);
    // Raw timer values for both PPS and OC
    printf("PPS C:%lu OC C:%lu\n", pps_count, oc_count);
    // PPS sync status
    printf("PPS S: %i ADJ: %i\n", pps_sync, oc_adjust_in_progress);
    // Scheduler sync status
    printf("SCH S: %i GNSS FIX: %i\n", scheduler_sync, gnss_fix);
    // Accumulated clock data
    printf("CLK D: %li CLK T: %li\n",accumulated_clocks, accumulation_delta);
    double accumulated_clocks_diff_total_avg = accumulated_clocks;
    if(accumulation_delta) accumulated_clocks_diff_total_avg = accumulated_clocks_diff_total_avg / accumulation_delta;
    else accumulated_clocks_diff_total_avg = 0;
    printf("AVG D: %.1f AVG D10: %.1f\n", accumulated_clocks_diff_total_avg, accumulated_clocks_diff_avg);
    
    // Do not print Kalman values
    //extern int32_t filtered_pps_kf;
    //printf("Kal_val: %li\n", filtered_pps_kf);
    
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
    printf("\n");
    printf("OC events: %lu Resync events: %lu\n",total_oc_seq_count, sync_events);
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
    accumulation_start = utc;
    accumulated_clocks = 0;
    oc_offset_correction_count_neg = 0;
    oc_offset_correction_count_pos = 0;
    memset(accumulated_clocks_diff, 0, FCYCLE_ACC_AVG_PERIOD);
    sync_events++;
}

bool oc_adjust_in_progress_ntp = 0;

void pic_pps_resync_ntp(int16_t ntp_offset)
{
    int32_t ntp_offset_cycles = ntp_offset * (fosc_freq / 1000);
    pic_pps_set_latch_cycles(fosc_freq + ntp_offset_cycles);
    oc_adjust_in_progress_ntp = 1;
    sync_events++;
}

void pic_pps_reset_sync_ntp(void)
{
    ntp_seq_count = 0;
    pps_sync = 0;
    scheduler_sync = 0;
}

void pic_pps_resync_oc_only(void)
{
    pic_pps_set_latch_cycles(fosc_freq);
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
                pic_pps_set_latch_cycles(fosc_freq - 0x10000); // Correct our fudge
            }
            else
            {
                pic_pps_set_latch_cycles(fosc_freq); // Reset our OC to 1Hz
                pps_sync = 1; // Indicate we are now sync'd with PPS
            }
            oc_adjust_in_progress = 0; // Clear the adjustment flag
        }
        else if(oc_adjust_in_progress_ntp)
        {
            if(oc_adjust_fudge)
            {
                pic_pps_set_latch_cycles(fosc_freq + oc_offset); // Correct our fudge
            }
            else
            {
                pic_pps_set_latch_cycles(fosc_freq);
                pps_sync = 1;
            }
            oc_adjust_in_progress_ntp = 0;
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
        utc++; // Increment our internal UTC timebase
        rtc++; // Increment RTC timebase
        local++; // Increment local time estimate (DST is recalculated outside the ISR as this comparatively expensive)
        t10ms_display = 0;
        display_update_pending = 0; // display update no longer pending
        total_oc_seq_count++; // Increment oc event counter
        gnss_invalidate_data();
        esp_reset_sync_timer(); // Reset our esp32 sync timer
        ic3_val = IC3BUF; // Read the IC3 timer
        ic4_val = IC4BUF; // Read the IC4 timer
        IFS2bits.IC3IF = 0;
    }
}

void __attribute__ ( ( interrupt, no_auto_psv ) ) _ISR _IC4Interrupt( void )
{	
    if(IFS2bits.IC4IF)
    {
        // Moved to IC3 ISR
        //ic4_val = IC4BUF; // Read the IC4 timer
        IFS2bits.IC4IF = 0;
    }
}
