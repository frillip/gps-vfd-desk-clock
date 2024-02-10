#include "sync_state.h"

CLOCK_SYNC_STATUS clock_sync_state = SYNC_POWER_ON;
CLOCK_SYNC_STATUS clock_sync_state_old = SYNC_POWER_ON;
CLOCK_SYNC_STATUS clock_sync_state_old2 = SYNC_POWER_ON;
CLOCK_SYNC_STATUS clock_sync_state_last = SYNC_POWER_ON;

uint32_t sync_state_detect_timeout = 0;

extern time_t rtc;
extern time_t gnss;
extern time_t utc;
extern time_t local;
extern int32_t tz_offset;
extern int32_t dst_offset;

extern bool rtc_detected;
//extern bool esp_ntp_detected;
bool esp_ntp_detected = 0;
extern bool gnss_detected;

extern uint32_t pps_seq_count;

extern bool oc_event;
extern bool ic_event;

bool state_new_oc = 0;
bool state_new_ic = 0;

extern bool print_data;
extern bool gnss_rmc_waiting;
extern bool scheduler_sync;

extern bool esp_detected;
extern bool esp_ntp_valid;

extern uint32_t fosc_freq;

extern uint32_t pps_count;
extern uint32_t pps_count_diff;
extern uint32_t pps_seq_count;

extern int32_t accumulated_clocks;
extern time_t accumulation_start;
extern time_t accumulation_delta;
extern double accumulated_clocks_diff_avg;

uint32_t state_machine_oc_seq_count_last = 0;
extern uint32_t total_oc_seq_count;
extern bool pps_sync;
extern bool oc_adjust_fudge;

extern bool scheduler_sync;

extern int32_t oc_offset;

extern uint32_t fosc_freq;

extern uint32_t sync_events;

extern int16_t esp_time_offset;

CLOCK_SYNC_STATUS sync_check_result = SYNC_SYNC;
CLOCK_SYNC_STATUS last_sync_cause = SYNC_POWER_ON;

void sync_state_machine(void)
{
    if(ic_event && ubx_gnss_time_valid())
    {
        calculate_pps_stats();
        state_new_ic = 1;
        ic_event=0;
    }
    if(oc_event)
    {
        ClrWdt();
        if(esp_detected)
        {
            //esp_tx_time();
        }
        printf("UTC: ");
        ui_print_iso8601_string(utc);
        printf("\r\n");
        state_new_oc = 1;
        ui_buzzer_interval_beep();
        oc_event=0;
    }
    switch(clock_sync_state)
    {
        case SYNC_NOSYNC_MANUAL:
            recalculate_fosc_freq();
            printf("\r\nNew Fosc freq: %luHz\r\n", fosc_freq);
            printf("CLK D: %li CLK T: %li\r\n\r\n",accumulated_clocks, accumulation_delta);
            pic_pps_reset_sync();
            reset_pps_stats();
            sync_state_machine_set_state(SYNC_NOSYNC);
            last_sync_cause = SYNC_NOSYNC_MANUAL;
            break;
        
        case SYNC_NOSYNC_GNSS:
            recalculate_fosc_freq();
            printf("\r\nGNSS sync loss\r\n");
            pic_pps_reset_sync();
            reset_pps_stats();
            sync_state_machine_set_state(SYNC_NOSYNC);
            last_sync_cause = SYNC_NOSYNC_GNSS;
            break;
        
        case SYNC_NOSYNC_FREQ_ONLY:
            recalculate_fosc_freq_short();
            printf("\r\nLarge short term freq deviation\r\n");
            printf("\r\nNew Fosc freq: %luHz\r\n", fosc_freq);
            sync_state_print_stats();
            pic_pps_resync_oc_only();
            reset_pps_stats();
            sync_state_machine_set_state(SYNC_MIN_INTERVAL);
            last_sync_cause = SYNC_NOSYNC_FREQ_ONLY;
            break;
        
        case SYNC_NOSYNC_FREQ:
            recalculate_fosc_freq_short();
            printf("\r\nLarge short term freq deviation\r\n");
            printf("New Fosc freq: %luHz\r\n", fosc_freq);
            sync_state_print_stats();
            pic_pps_reset_sync();
            reset_pps_stats();
            sync_state_machine_set_state(SYNC_NOSYNC);
            last_sync_cause = SYNC_NOSYNC_FREQ;
            break;

        case SYNC_NOSYNC_MAJOR_OC:
            printf("\r\nOC unsynchronised... resetting\r\n");
            if((accumulation_delta > PPS_SEQ_COUNT_MIN) && scheduler_sync)
            {
                recalculate_fosc_freq();
                printf("\r\nNew Fosc freq: %luHz\r\n", fosc_freq);
            }
            sync_state_print_stats();
            pic_pps_reset_sync();
            reset_pps_stats();
            sync_state_machine_set_state(SYNC_NOSYNC);
            last_sync_cause = SYNC_NOSYNC_MAJOR_OC;
            break;
        
        case SYNC_NOSYNC_MAJOR:
            recalculate_fosc_freq();
            printf("\r\nMajor frequency excursion...\r\n");
            printf("New Fosc freq: %luHz\r\n", fosc_freq);
            sync_state_print_stats();
            pic_pps_reset_sync();
            reset_pps_stats();
            sync_state_machine_set_state(SYNC_NOSYNC);
            last_sync_cause = SYNC_NOSYNC_MAJOR;
            break;
        
        case SYNC_NOSYNC_MINOR_OC:
            printf("\r\nOC unsynchronised... resetting\r\n");
            sync_state_print_stats();
            pic_pps_reset_sync();
            reset_pps_stats();
            sync_state_machine_set_state(SYNC_NOSYNC);
            last_sync_cause = SYNC_NOSYNC_MINOR_OC;
            break;
        
        case SYNC_NOSYNC_MINOR:
            recalculate_fosc_freq();
            printf("\r\nNew Fosc freq: %luHz\r\n", fosc_freq);
            sync_state_print_stats();
            pic_pps_reset_sync();
            reset_pps_stats();
            sync_state_machine_set_state(SYNC_NOSYNC);
            last_sync_cause = SYNC_NOSYNC_MINOR;
            break;
        
        case SYNC_SYNC:
            if(state_new_oc)
            {
                pic_pps_calculate_oc_stats();
                sync_check_result = pic_pps_evaluate_sync();
                switch (sync_check_result)
                {
                    case SYNC_NOSYNC_FREQ_ONLY:
                        sync_state_machine_set_state(SYNC_NOSYNC_FREQ_ONLY);
                        break;
                        
                    case SYNC_NOSYNC_FREQ:
                        sync_state_machine_set_state(SYNC_NOSYNC_FREQ);
                        break;
                        
                    case SYNC_NOSYNC_MAJOR_OC:
                        sync_state_machine_set_state(SYNC_NOSYNC_MAJOR_OC);
                        break;
                        
                    case SYNC_NOSYNC_MAJOR:
                        sync_state_machine_set_state(SYNC_NOSYNC_MAJOR);
                    break;
                    
                    case SYNC_NOSYNC_MINOR_OC:
                        sync_state_machine_set_state(SYNC_NOSYNC_MINOR_OC);
                        break;
                        
                    case SYNC_NOSYNC_MINOR:
                        sync_state_machine_set_state(SYNC_NOSYNC_MINOR);
                    break;

                    default:
                        break;
                }
                struct tm *time_struct;
                time_struct = gmtime(&utc);
                if(time_struct->tm_sec==0)
                {
                    if(!(time_struct->tm_min%15)) rtc_write_from_calendar(utc);
                }
                state_new_oc = 0;
            }
            break;
        
        case SYNC_INTERVAL:
            if(state_new_oc)
            {
                pic_pps_calculate_oc_stats();
                sync_check_result = pic_pps_evaluate_sync();
                switch (sync_check_result)
                {
                    case SYNC_NOSYNC_FREQ_ONLY:
                        sync_state_machine_set_state(SYNC_NOSYNC_FREQ_ONLY);
                        break;
                        
                    case SYNC_NOSYNC_FREQ:
                        sync_state_machine_set_state(SYNC_NOSYNC_FREQ);
                        break;
                        
                    case SYNC_NOSYNC_MAJOR_OC:
                        sync_state_machine_set_state(SYNC_NOSYNC_MAJOR_OC);
                        break;
                        
                    case SYNC_NOSYNC_MAJOR:
                        sync_state_machine_set_state(SYNC_NOSYNC_MAJOR);
                    break;

                    default:
                        break;
                }
                if(pps_seq_count > FCYCLE_ACC_INTERVAL_MIN)
                {
                    sync_state_machine_set_state(SYNC_SYNC);
                }
                state_new_oc = 0;
            }
            break;
        
        case SYNC_MIN_INTERVAL:
            if(clock_sync_state!=clock_sync_state_old2) reset_pps_stats();
            if(state_new_oc)
            {
                pic_pps_calculate_oc_stats();
                if(pps_seq_count > PPS_SEQ_COUNT_MIN)
                {
                    sync_state_machine_set_state(SYNC_INTERVAL);
                }
                state_new_oc = 0;
            }
            break;
        
        case SYNC_SCHED_SYNC:
            if(state_new_oc)
            {
                pic_pps_calculate_oc_stats();
                if(scheduler_sync) sync_state_machine_set_state(SYNC_MIN_INTERVAL);
                state_new_oc = 0;
            }
            break;
        
        case SYNC_PPS_SYNC:
            if(state_new_oc)
            {
                pic_pps_calculate_oc_stats();
                if(pps_sync) 
                {
                    rtc_write_from_calendar(utc);
                    sync_state_machine_set_state(SYNC_SCHED_SYNC);
                }
                state_new_oc = 0;
            }
            break;
        
        case SYNC_ADJUST_STAGE_2:
            if(state_new_oc)
            {
                pic_pps_calculate_oc_stats();
                if(!oc_adjust_fudge) sync_state_machine_set_state(SYNC_PPS_SYNC);
                state_new_oc = 0;
            }
            break;
        
        case SYNC_ADJUST_STAGE_1:
            if(state_new_oc)
            {
                pic_pps_calculate_oc_stats();
                pic_pps_resync();
                if(oc_adjust_fudge) sync_state_machine_set_state(SYNC_ADJUST_STAGE_2);
                else sync_state_machine_set_state(SYNC_PPS_SYNC);
                state_new_oc = 0;
            }
            break;
        
        case SYNC_NOSYNC:
            sync_state_machine_set_state(SYNC_ADJUST_STAGE_1);
            break;
        
        case SYNC_STARTUP:
            if(pps_seq_count > PPS_SEQ_COUNT_MIN)
            {
                sync_state_machine_set_state(SYNC_NOSYNC);
            }
            break;
        
        case SYNC_NTP_ONLY:
            if(state_new_oc)
            {
                int16_t esp_time_offset_adjust = esp_time_offset;
                while(esp_time_offset_adjust>500) esp_time_offset_adjust -= 1000;
                if((esp_time_offset_adjust > ESP_NTP_OFFSET_MAX_MS)||(esp_time_offset_adjust < ESP_NTP_OFFSET_MIN_MS))
                {
                    printf("Reset NTP sync\r\n");
                    printf("%i ",esp_time_offset_adjust);
                    pic_pps_reset_sync_ntp();
                    if(esp_time_offset_adjust<0) esp_time_offset_adjust += 1000;
                    pic_pps_resync_ntp(esp_time_offset_adjust);
                    sync_state_machine_set_state(SYNC_NTP_ONLY_ADJUST); // Move to different state whilst adjust in progress
                }
                if(esp_ntp_valid)
                {
                    if(!ntp_is_calendar_sync(utc))
                    {
                        esp_ntp_set_calendar();
                    }
                }
                state_new_oc = 0;
            }
            if(gnss_detected)
            {
                if(ubx_gnss_time_valid())
                {
                    printf("GNSS FIX ACQUIRED\r\n");
                    pic_pps_reset_sync();
                    reset_pps_stats();
                    sync_state_machine_set_state(SYNC_STARTUP);
                }
            }
            break;
            
        case SYNC_NTP_ONLY_ADJUST:
            if(state_new_oc)
            {
                if(esp_ntp_valid)
                {
                    if(!ntp_is_calendar_sync(utc))
                    {
                        esp_ntp_set_calendar();
                    }
                }
                sync_state_machine_set_state(SYNC_NTP_ONLY);
                state_new_oc = 0;
            }
            break;
            
        case SYNC_NTP_NO_NETWORK:
            if(state_new_oc)
            {
                //printf("Shouldn't be here yet...\r\n");
                state_new_oc = 0;
            }
            if(gnss_detected)
            {
                if(ubx_gnss_time_valid())
                {
                    printf("GNSS FIX ACQUIRED\r\n");
                    pic_pps_reset_sync();
                    reset_pps_stats();
                    sync_state_machine_set_state(SYNC_STARTUP);
                }
            }
            if(esp_ntp_valid)
            {
                printf("NTP SYNC ACQUIRED\r\n");
                sync_state_machine_set_state(SYNC_NTP_ONLY);
            }
            break;
        
        case SYNC_RTC_ONLY:
            if(gnss_detected)
            {
                if(ubx_gnss_time_valid())
                {
                    printf("GNSS FIX ACQUIRED\r\n");
                    sync_state_machine_set_state(SYNC_STARTUP);
                }
            }
            break;
        
        case SYNC_NO_CLOCK:
            if(state_new_oc)
            {
                printf("NO CLOCK!\r\n");
                state_new_oc = 0;
            }
            break;
        
        case SYNC_GNSS_WAIT_FOR_FIX:
            if(sync_state_detect_timeout < GNSS_FIX_LIMIT)
            {
                sync_state_detect_timeout++;
                if(ubx_gnss_time_valid())
                {
                    printf("GNSS FIX ACQUIRED\r\n");
                    sync_state_machine_set_state(SYNC_STARTUP);
                }
            }
            else
            {
                printf("NO GNSS FIX... ");
                if(esp_ntp_valid)
                {
                    printf("NTP ONLY MODE\r\n");
                    sync_state_machine_set_state(SYNC_NTP_ONLY);
                }
                else if(esp_ntp_detected)
                {
                    printf("NTP ONLY MODE - NO NETWORK\r\n");
                    sync_state_machine_set_state(SYNC_NTP_NO_NETWORK);
                }
                else if(rtc_detected)
                {
                    printf("RTC ONLY MODE\r\n");
                    sync_state_machine_set_state(SYNC_RTC_ONLY);
                }
                else
                {
                    printf("NO CLOCK DETECTED\r\n");
                    sync_state_machine_set_state(SYNC_NO_CLOCK);
                }
            }
            break;
        
        case SYNC_GNSS_DETECT:
            if(sync_state_detect_timeout < GNSS_DETECT_LIMIT)
            {
                sync_state_detect_timeout++;
                if(clock_sync_state!=clock_sync_state_old2)
                {
                    gnss_init(); // Only do this the first loop
                }
                
                if(gnss_detected)
                {
                    printf("GNSS DETECTED\r\n");
                    sync_state_machine_set_state(SYNC_GNSS_WAIT_FOR_FIX);
                }
            }
            else
            {
                printf("NO GNSS DETECTED... ");
                if(esp_ntp_valid)
                {
                    printf("NTP ONLY MODE\r\n");
                    sync_state_machine_set_state(SYNC_NTP_ONLY);
                }
                else if(esp_ntp_detected)
                {
                    printf("NTP ONLY MODE - NO NETWORK\r\n");
                    sync_state_machine_set_state(SYNC_NTP_NO_NETWORK);
                }
                else if(rtc_detected)
                {
                    printf("RTC ONLY MODE\r\n");
                    sync_state_machine_set_state(SYNC_RTC_ONLY);
                }
                else
                {
                    printf("NO CLOCK DETECTED\r\n");
                    sync_state_machine_set_state(SYNC_NO_CLOCK);
                }
            }
            break;
        
        case SYNC_NTP_DETECT:
            if(sync_state_detect_timeout < ESP_DETECT_LIMIT)
            {
                sync_state_detect_timeout++;
                if(clock_sync_state!=clock_sync_state_old2)
                {
                    esp_ntp_init(); // Only do this the first loop
                }
                if(esp_ntp_valid)
                {
                    esp_ntp_set_calendar();
                    sync_state_machine_set_state(SYNC_GNSS_DETECT);
                }
            }
            else
            {
                if(esp_detected) printf("ESP DETECTED BUT NO NTP SYNC\r\n");
                else
                {
                    printf("NO ESP DETECTED\r\n");
                    esp_stop_sync_timer();
                }
                sync_state_machine_set_state(SYNC_GNSS_DETECT);
            }
            break;
        
        case SYNC_RTC_DETECT:
            if(sync_state_detect_timeout < RTC_DETECT_LIMIT)
            {
                sync_state_detect_timeout++;
                rtc_get_calendar();
                if(rtc_detected)
                {
                    rtc_set_calendar();
                    printf("RTC DETECTED\r\n");
                    sync_state_machine_set_state(SYNC_NTP_DETECT);
                }
            }
            else
            {
                printf("NO RTC DETECTED\r\n");
                sync_state_machine_set_state(SYNC_NTP_DETECT);
            }
            break;
        
        case SYNC_POWER_ON:
            gnss_pps_init();
            pic_pps_init();
            display_init();
            sync_state_machine_set_state(SYNC_RTC_DETECT);
            break;
        
        default:
            // I have no idea what to do if we end up here...
            break;
    }
    clock_sync_state_old2 = clock_sync_state_old;
    clock_sync_state_old = clock_sync_state;
}

void sync_state_machine_set_state(CLOCK_SYNC_STATUS state)
{
    clock_sync_state_last = clock_sync_state;
    clock_sync_state = state;
    sync_state_detect_timeout = 0;
    printf("\r\n");
    sync_state_print(clock_sync_state);
    printf("\r\n");
}

void print_sync_state_machine(void)
{
    printf("\r\n=== STATE MACHINE ===\r\n");
    printf("STATE: ");
    sync_state_print(clock_sync_state);
    printf(" OLD: ");
    sync_state_print(clock_sync_state_old2);
    printf(" LAST: ");
    sync_state_print(clock_sync_state_last);
    printf("\r\nLAST SYNC CAUSE: ");
    sync_state_print(last_sync_cause);
    printf("\r\n");
}

void sync_state_print(CLOCK_SYNC_STATUS sync_state)
{
    switch(sync_state)
    {
        case SYNC_POWER_ON:
            printf("SYNC_POWER_ON");
            break;
            
        case SYNC_RTC_DETECT:
            printf("SYNC_RTC_DETECT");
            break;
            
        case SYNC_NTP_DETECT:
            printf("SYNC_NTP_DETECT");
            break;
            
        case SYNC_GNSS_DETECT:
            printf("SYNC_GNSS_DETECT");
            break;
            
        case SYNC_GNSS_WAIT_FOR_FIX:
            printf("SYNC_GNSS_WAIT_FOR_FIX");
            break;
            
        case SYNC_STARTUP:
            printf("SYNC_STARTUP");
            break;
            
        case SYNC_NOSYNC:
            printf("SYNC_NOSYNC");
            break;
            
        case SYNC_ADJUST_STAGE_1:
            printf("SYNC_ADJUST_STAGE_1");
            break;
            
        case SYNC_ADJUST_STAGE_2:
            printf("SYNC_ADJUST_STAGE_2");
            break;
            
        case SYNC_PPS_SYNC:
            printf("SYNC_PPS_SYNC");
            break;
            
        case SYNC_SCHED_SYNC:
            printf("SYNC_SCHED_SYNC");
            break;
            
        case SYNC_MIN_INTERVAL:
            printf("SYNC_MIN_INTERVAL");
            break;
            
        case SYNC_INTERVAL:
            printf("SYNC_INTERVAL");
            break;
            
        case SYNC_SYNC:
            printf("SYNC_SYNC");
            break;
            
        case SYNC_NOSYNC_MINOR:
            printf("SYNC_NOSYNC_MINOR");
            break;
            
        case SYNC_NOSYNC_MINOR_OC:
            printf("SYNC_NOSYNC_MINOR_OC");
            break;
            
        case SYNC_NOSYNC_MAJOR:
            printf("SYNC_NOSYNC_MAJOR");
            break;
            
        case SYNC_NOSYNC_MAJOR_OC:
            printf("SYNC_NOSYNC_MAJOR_OC");
            break;
            
        case SYNC_NOSYNC_FREQ:
            printf("SYNC_NOSYNC_FREQ");
            break;
            
        case SYNC_NOSYNC_FREQ_ONLY:
            printf("SYNC_NOSYNC_FREQ_ONLY");
            break;
            
        case SYNC_NOSYNC_GNSS:
            printf("SYNC_NOSYNC_GNSS");
            break;
            
        case SYNC_NOSYNC_MANUAL:
            printf("SYNC_NOSYNC_MANUAL");
            break;

        case SYNC_NO_CLOCK:
            printf("SYNC_NO_CLOCK");
            break;

        case SYNC_RTC_ONLY:
            printf("SYNC_RTC_ONLY");
            break;

        case SYNC_NTP_ONLY:
            printf("SYNC_NTP_ONLY");
            break;

        case SYNC_NTP_ONLY_ADJUST:
            printf("SYNC_NTP_ONLY_ADJUST");
            break;
            
        case SYNC_NTP_NO_NETWORK:
            printf("SYNC_NTP_NO_NETWORK");
            break;
            
        default:
            printf("UNKNOWN SYNC STATE!");
            break;
    }
}

CLOCK_SYNC_STATUS pic_pps_evaluate_sync(void)
{
    if((oc_offset > FCYCLE_ACC_LIM_POSITIVE) || (oc_offset < FCYCLE_ACC_LIM_NEGATIVE))
    {
        if(accumulation_delta > FCYCLE_ACC_INTERVAL_MIN)
        {
            if(((accumulated_clocks) > (accumulation_delta * FCYCLE_ACC_LIM_MULTIPLE)))
            {
                return SYNC_NOSYNC_MINOR;
            }
            else if(((accumulated_clocks^0xFFFFFFFF)+1) > (accumulation_delta * FCYCLE_ACC_LIM_MULTIPLE)) // Dirty hack to get abs() equivalent
            {
                return SYNC_NOSYNC_MINOR;
            }
            else
            {
                return SYNC_NOSYNC_MINOR_OC;
            }
        }
        else if((accumulated_clocks > FCYCLE_ACC_RESET_POSITIVE) || (accumulated_clocks < FCYCLE_ACC_RESET_NEGATIVE))
        {
            return SYNC_NOSYNC_MAJOR;
        }
        else if(((oc_offset + OC_OFFSET_MAX) > (FCYCLE_ACC_RESET_POSITIVE + FCYCLE_ACC_LIM_POSITIVE)) || ((oc_offset + OC_OFFSET_MIN) < (FCYCLE_ACC_RESET_NEGATIVE + FCYCLE_ACC_LIM_NEGATIVE)))
        {
            if(pps_count_diff)
            {
                return SYNC_NOSYNC_MAJOR_OC;
            }
        }
    }
    if(accumulation_delta > (FCYCLE_ACC_AVG_PERIOD * FCYCLE_ACC_AVG_PERIOD_MULTIPLE))
    {
        if((accumulated_clocks_diff_avg > FCYCLE_ACC_AVG_POSITIVE) || (accumulated_clocks_diff_avg < FCYCLE_ACC_AVG_NEGATIVE))
        {
            if((accumulated_clocks > (FCYCLE_ACC_LIM_POSITIVE / 4)) || (accumulated_clocks < (FCYCLE_ACC_LIM_NEGATIVE / 4)))
            {
                return SYNC_NOSYNC_FREQ;
            }
            else
            {
                if(sync_events==1) return SYNC_NOSYNC_FREQ; // Do full sync if it's the first sync
                return SYNC_NOSYNC_FREQ_ONLY;
            }
        }
    }
    return SYNC_SYNC;
}

void sync_state_print_stats(void)
{
    printf("CLK D: %li CLK T: %li\r\n",accumulated_clocks, accumulation_delta);
    printf("PPS D:%lu OC D:%li\r\n\r\n", pps_count_diff, oc_offset);
    printf("AVG D: %.1f\r\n", accumulated_clocks_diff_avg);
}