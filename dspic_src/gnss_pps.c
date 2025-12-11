#include "gnss_pps.h"

uint16_t ic1_val = 0;
uint16_t ic1_val_old = 0;
uint16_t ic2_val = 0;
uint16_t ic2_val_old = 0;
uint32_t pps_count = 0;
uint32_t pps_count_diff = 0;
uint32_t pps_count_old = 0;
uint32_t pps_seq_count = 0;
uint32_t pps_seq_count_old = 0;
uint32_t gnss_pps_count = 0;
bool pps_missing = 0;
uint32_t pps_missing_count = 0;
int32_t accumulated_clocks = 0;
int32_t accumulated_clocks_old = 0;
int32_t accumulated_clocks_diff[FCYCLE_ACC_AVG_PERIOD] = {};
uint8_t accumulated_clocks_diff_index = 0;
double accumulated_clocks_diff_avg = 0.0;
time_t accumulation_start = 0;
time_t accumulation_delta = 0;

bool ic_event = 0;

extern time_t utc;
extern time_t gnss;
extern uint16_t counter;
extern uint32_t fosc_freq;

KalmanFilter pps_kf;
KalmanFilter_f pps_kf_f;
int32_t filtered_pps_kf;
float filtered_pps_kf_f;

int32_t filtered_pps_buffer[MAX_PPS_SAMPLES];
int32_t pps_buffer[MAX_PPS_SAMPLES];
uint16_t pps_index = 0;

extern EEPROM_DATA_STRUCT settings;
extern bool fosc_pending_eeprom_write;

void gnss_pps_init(void)
{
    // Init IC2 first
    IC2_Initialize();
    // Then IC1
    IC1_Initialize();
    
    kalman_init(&pps_kf, XTAL_FREQ, 50, 5);
    kalman_init_f(&pps_kf_f, XTAL_FREQ, 50, 5);
}

void IC1_Initialize (void)
{
    // ICSIDL disabled; ICM Simple Capture mode: Rising; ICTSEL FOSC/2; ICI Every; 
    IC1CON1 = 0x1C03;
    // SYNCSEL PTGO8; TRIGSTAT disabled; IC32 enabled; ICTRIG Sync; 
    IC1CON2 = 0x10A;
    IFS0bits.IC1IF = false; // Clear the interrupt flag
    IEC0bits.IC1IE = true; // Enable interrupt
    IPC0bits.IC1IP = 1; // Set interrupt priority
}

void IC2_Initialize (void)
{
    // ICSIDL disabled; ICM Simple Capture mode: Rising; ICTSEL FOSC/2; ICI Every; 
    IC2CON1 = 0x1C03;
    // SYNCSEL PTGO9; TRIGSTAT disabled; IC32 enabled; ICTRIG Sync; 
    IC2CON2 = 0x10A;
    IFS0bits.IC2IF = false; // Clear the interrupt flag
    IEC0bits.IC2IE = true; // Enable interrupt
    IPC1bits.IC2IP = 1; // Set interrupt priority
}

void calculate_pps_stats(void)
{
    // Calculate our PPS and OC stats
    pps_count = (((uint32_t)ic2_val)<<16) + ic1_val; // Raw timer
    pps_count_diff = pps_count-pps_count_old; // Difference from last
    pps_count_old = pps_count; // Store the new value as old
    accumulated_clocks += pps_count_diff;
    while(accumulated_clocks>FCYCLE_POSITIVE_SUM) accumulated_clocks -= fosc_freq;
    while(accumulated_clocks<FCYCLE_NEGATIVE_SUM) accumulated_clocks += fosc_freq; // Should not happen, but if GPS is evaluated to be significantly 'early' we end up here.
    if(accumulation_delta)
    {
        accumulated_clocks_diff[accumulated_clocks_diff_index] = accumulated_clocks - accumulated_clocks_old;
        accumulated_clocks_diff_index++;
        if(accumulated_clocks_diff_index == 10) accumulated_clocks_diff_index = 0;
        uint8_t i;
        accumulated_clocks_diff_avg = 0;
        for (i=0;i<FCYCLE_ACC_AVG_PERIOD;i++)
        {
            accumulated_clocks_diff_avg += (float)accumulated_clocks_diff[i] / 10;
        }
    }
    accumulated_clocks_old = accumulated_clocks;
    if(accumulation_start) accumulation_delta = utc - accumulation_start;
    else accumulation_delta = 0;
    
    if(pps_seq_count > PPS_SEQ_COUNT_MIN)
    {
        pps_index++;
        if (pps_index < MAX_PPS_SAMPLES)
        {
            pps_index = 0;
        }
        pps_buffer[pps_index++] = pps_count_diff;

        // Do not calculate kalman filtered pps values
        // filtered_pps_kf = kalman_update(&pps_kf, (int32_t)pps_count_diff);
        // filtered_pps_kf_f = kalman_update_f(&pps_kf_f, (int32_t)pps_count_diff);
        // filtered_pps_buffer[pps_index++] = filtered_pps_kf;
    }
    
    // Do not print kalman filtered pps values
    // printf("D: %li K: %li K_f: %6.0f\n", pps_count_diff, filtered_pps_kf, filtered_pps_kf_f);
    
    ic1_val = 0;
    ic2_val = 0;
}

void reset_pps_stats(void)
{
    pps_seq_count = 0;
    pps_seq_count_old = 0;
    pps_missing = 0;
    //pps_missing_count = 0;
    accumulation_start = utc;
    accumulated_clocks = 0;
    accumulation_delta = 0;
    memset(accumulated_clocks_diff, 0, FCYCLE_ACC_AVG_PERIOD);
    accumulated_clocks_diff_index = 0;
    accumulated_clocks_diff_avg = 0.0;
    kalman_init(&pps_kf, fosc_freq, 5, 50);
    kalman_init_f(&pps_kf_f, fosc_freq, 5, 50);
    filtered_pps_kf = fosc_freq;
    filtered_pps_kf_f = fosc_freq;
    memset(pps_buffer, 0 ,sizeof(pps_buffer));
    memset(filtered_pps_buffer, 0 ,sizeof(filtered_pps_buffer));
    pps_index = 0;
}

void recalculate_fosc_freq(void)
{
    long double new_fosc_freq_f = fosc_freq;
    uint32_t new_fosc_freq = 0;
    new_fosc_freq_f = new_fosc_freq_f * accumulation_delta;
    new_fosc_freq_f = new_fosc_freq_f + accumulated_clocks;
    new_fosc_freq_f = new_fosc_freq_f / accumulation_delta; // accumulation_delta should never be 0 at this point
    new_fosc_freq = (new_fosc_freq_f + 0.5); //DIRTY ROUNDL() EQUIVALENT
    fosc_freq = new_fosc_freq;
    if(fosc_freq>FCYCLE_UPPER_LIM||fosc_freq<FCYCLE_LOWER_LIM) fosc_freq = FCYCLE;
    
    //Save new value to EEPROM
    settings.fields.pps.fosc_freq = fosc_freq;
    fosc_pending_eeprom_write = 1;
}

void recalculate_fosc_freq_short(void)
{
    fosc_freq = fosc_freq + (accumulated_clocks_diff_avg + 0.5); //DIRTY ROUNDL() EQUIVALENT
    if(fosc_freq>FCYCLE_UPPER_LIM||fosc_freq<FCYCLE_LOWER_LIM) fosc_freq = FCYCLE;
        
    //Save new value to EEPROM
    settings.fields.pps.fosc_freq = fosc_freq;
    fosc_pending_eeprom_write = 1;
}

// IC1 ISR
void __attribute__ ( ( interrupt, no_auto_psv ) ) _ISR _IC1Interrupt( void )
{	
    if(IFS0bits.IC1IF)
    {
        pps_seq_count_old = pps_seq_count;
        pps_seq_count++; // Increment our PPS counter
        gnss_pps_count++;
        gnss++;
        ic_event = 1;    // Flag we've had an IC event on GNSS
        ic1_val = IC1BUF; // Read the IC1 timer
        ic2_val = IC2BUF; // Read the IC2 timer
        IFS0bits.IC1IF = 0; // Clear the interrupt flag
    }
}

// IC2 ISR
void __attribute__ ( ( interrupt, no_auto_psv ) ) _ISR _IC2Interrupt( void )
{	
    if(IFS0bits.IC2IF)
    {
        // Moved to IC1 ISR
        //ic2_val = IC2BUF; // Read the IC2 timer
        IFS0bits.IC2IF = 0; // Clear the interrupt flag
    }
}

void print_gnss_pps_info(void)
{
    extern uint32_t oc_offset;
    
    printf("\n=== GNSS PPS ===\n");
    printf("PPS SEQ: %lu TOTAL:%lu\n", pps_seq_count, gnss_pps_count);
    printf("MISSING: %u COUNT:%lu\n", pps_missing, pps_missing_count);
    uint32_t oc_offset_ns = oc_offset * 25;
    if(oc_offset_ns>1000)
    {
        uint32_t oc_offset_us = (oc_offset_ns) / 1000;
        uint32_t oc_offset_100ns = ((oc_offset_ns + 50) / 100) - (oc_offset_us * 10);
        printf("Relative offset: %lu.%luus\n", oc_offset_us, oc_offset_100ns);
    }
    else
    {
        printf("Relative offset: %luns\n", oc_offset_ns);
    }
}