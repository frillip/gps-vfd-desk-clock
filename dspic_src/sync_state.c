#include "sync_state.h"

CLOCK_SYNC_STATUS clock_sync_state = SYNC_POWER_ON;
CLOCK_SYNC_STATUS clock_sync_state_old = SYNC_POWER_ON;
CLOCK_SYNC_STATUS clock_sync_state_old2 = SYNC_POWER_ON;
CLOCK_SYNC_STATUS clock_sync_state_last = SYNC_POWER_ON;

uint32_t sync_state_detect_timeout = 0;

extern time_t rtc;
extern time_t esp;
extern time_t ntp;
extern time_t gnss;
extern time_t utc;
extern time_t display;
extern time_t local;

extern bool dst_active;

extern bool rtc_detected;
extern bool rtc_valid;

bool esp_ntp_detected = 0;
extern bool gnss_detected;

extern uint32_t pps_seq_count;

extern bool oc_event;
extern bool ic_event;

bool state_new_oc = 0;
bool state_new_ic = 0;
uint8_t sync_state_eval_time_counter = 0;

extern bool print_data;
extern bool gnss_rmc_waiting;
extern bool scheduler_sync;

extern bool esp_detected;
extern bool esp_ntp_valid;
extern bool esp_wifi_status;

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

extern int32_t esp_time_offset;
extern uint32_t ntp_seq_count;
extern uint32_t total_ntp_seq_count;

CLOCK_SYNC_STATUS sync_check_result = SYNC_SYNC;
CLOCK_SYNC_STATUS last_sync_cause = SYNC_POWER_ON;

CLOCK_SOURCE utc_source = CLOCK_SOURCE_NONE;

bool no_clock_blink = 0;

extern uint8_t esp_data_task_cycle;

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
        sync_state_eval_time_counter = EVAL_TIME_DELAY_INTERVAL;
        state_new_oc = 1;
        pic_pps_calculate_oc_stats();
        local = get_local_time(utc);
        ui_buzzer_interval_beep();
        oc_event=0;
    }
    if(sync_state_eval_time_counter)
    {
        sync_state_eval_time_counter--;
        if(sync_state_eval_time_counter==0)
        {
            sync_state_eval_time();
            esp_data_task_reset_cycle();
            esp_data_task();
#ifdef DEBUG_MESSAGES
            if(esp_ntp_valid)
            {
                esp_print_offset();
            }
#endif
        }
    }
    switch(clock_sync_state)
    {
        case SYNC_NOSYNC_MANUAL:
            recalculate_fosc_freq();
#ifdef DEBUG_MESSAGES
            printf("\nNew Fosc freq: %luHz\n", fosc_freq);
            printf("CLK D: %li CLK T: %li\n\n",accumulated_clocks, accumulation_delta);
#endif
            pic_pps_reset_sync();
            reset_pps_stats();
            sync_state_machine_set_state(SYNC_NOSYNC);
            last_sync_cause = SYNC_NOSYNC_MANUAL;
            break;
        
        case SYNC_NOSYNC_GNSS:
            recalculate_fosc_freq();
#ifdef DEBUG_MESSAGES
            printf("\nGNSS sync loss\n");
#endif
            pic_pps_reset_sync();
            reset_pps_stats();
            sync_state_machine_set_state(SYNC_NOSYNC);
            last_sync_cause = SYNC_NOSYNC_GNSS;
            break;
        
        case SYNC_NOSYNC_FREQ_ONLY:
            recalculate_fosc_freq_short();
#ifdef DEBUG_MESSAGES
            printf("\nLarge short term freq deviation\n");
            printf("\nNew Fosc freq: %luHz\n", fosc_freq);
#endif
            sync_state_print_stats();
            pic_pps_resync_oc_only();
            reset_pps_stats();
            sync_state_machine_set_state(SYNC_MIN_INTERVAL);
            last_sync_cause = SYNC_NOSYNC_FREQ_ONLY;
            break;
        
        case SYNC_NOSYNC_FREQ:
            recalculate_fosc_freq_short();
#ifdef DEBUG_MESSAGES
            printf("\nLarge short term freq deviation\n");
            printf("New Fosc freq: %luHz\n", fosc_freq);
#endif
            sync_state_print_stats();
            pic_pps_reset_sync();
            reset_pps_stats();
            sync_state_machine_set_state(SYNC_NOSYNC);
            last_sync_cause = SYNC_NOSYNC_FREQ;
            break;

        case SYNC_NOSYNC_MAJOR_OC:
#ifdef DEBUG_MESSAGES
            printf("\nOC unsynchronised... resetting\n");
#endif
            if((accumulation_delta > PPS_SEQ_COUNT_MIN) && scheduler_sync)
            {
                recalculate_fosc_freq();
#ifdef DEBUG_MESSAGES
                printf("\nNew Fosc freq: %luHz\n", fosc_freq);
#endif
            }
            sync_state_print_stats();
            pic_pps_reset_sync();
            reset_pps_stats();
            sync_state_machine_set_state(SYNC_NOSYNC);
            last_sync_cause = SYNC_NOSYNC_MAJOR_OC;
            break;
        
        case SYNC_NOSYNC_MAJOR:
            recalculate_fosc_freq();
#ifdef DEBUG_MESSAGES
            printf("\nMajor frequency excursion...\n");
            printf("New Fosc freq: %luHz\n", fosc_freq);
#endif
            sync_state_print_stats();
            pic_pps_reset_sync();
            reset_pps_stats();
            sync_state_machine_set_state(SYNC_NOSYNC);
            last_sync_cause = SYNC_NOSYNC_MAJOR;
            break;
        
        case SYNC_NOSYNC_MINOR_OC:
#ifdef DEBUG_MESSAGES
            printf("\nOC unsynchronised... resetting\n");
#endif
            sync_state_print_stats();
            pic_pps_reset_sync();
            reset_pps_stats();
            sync_state_machine_set_state(SYNC_NOSYNC);
            last_sync_cause = SYNC_NOSYNC_MINOR_OC;
            break;
        
        case SYNC_NOSYNC_MINOR:
            recalculate_fosc_freq();
#ifdef DEBUG_MESSAGES
            printf("\nNew Fosc freq: %luHz\n", fosc_freq);
#endif
            sync_state_print_stats();
            pic_pps_reset_sync();
            reset_pps_stats();
            sync_state_machine_set_state(SYNC_NOSYNC);
            last_sync_cause = SYNC_NOSYNC_MINOR;
            break;
        
        case SYNC_SYNC:
            if(state_new_oc)
            {
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
            if(!ubx_gnss_time_valid())
            {
                sync_state_machine_set_state(sync_select_best_clock());
            }
            break;
        
        case SYNC_INTERVAL:
            if(state_new_oc)
            {
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
            if(!ubx_gnss_time_valid())
            {
                sync_state_machine_set_state(sync_select_best_clock());
            }
            break;
        
        case SYNC_MIN_INTERVAL:
            if(clock_sync_state!=clock_sync_state_old2) reset_pps_stats();
            if(state_new_oc)
            {
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
                if(scheduler_sync) sync_state_machine_set_state(SYNC_MIN_INTERVAL);
                state_new_oc = 0;
            }
            break;
        
        case SYNC_PPS_SYNC:
            if(state_new_oc)
            {
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
                if(!oc_adjust_fudge) sync_state_machine_set_state(SYNC_PPS_SYNC);
                state_new_oc = 0;
            }
            break;
        
        case SYNC_ADJUST_STAGE_1:
            if(state_new_oc)
            {
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
        
        case SYNC_NTP:
            if(state_new_oc)
            {
                int32_t esp_time_offset_adjust = esp_time_offset;
                while(esp_time_offset_adjust>500) esp_time_offset_adjust -= 1000;
                // Problem with NTP burst giving higher than normal offsets
                // which settle after some packets
                // Either a new state where we wait for better accuracy
                // Or the below where we wait a number of NTP cycles before syncing
                // Or might not need this at all if we just tighten
                // ESP_NTP_OFFSET defines so 10ms so any deviation after burst is
                // immediately corrected
                //if(ntp_seq_count > NTP_SEQ_COUNT_MIN)
                //{
                    if((esp_time_offset_adjust > ESP_NTP_OFFSET_MAX_MS)||(esp_time_offset_adjust < ESP_NTP_OFFSET_MIN_MS))
                    {
#ifdef DEBUG_MESSAGES
                        printf("Reset NTP sync\n");
#endif
                        pic_pps_reset_sync_ntp();
                        if(esp_time_offset_adjust<0) esp_time_offset_adjust += 1000;
                        pic_pps_resync_ntp(esp_time_offset_adjust);
                        sync_state_machine_set_state(SYNC_NTP_ADJUST); // Move to different state whilst adjust in progressZ
                    }
                //}
                state_new_oc = 0;
                break;
            }
            if(gnss_detected)
            {
                if(ubx_gnss_time_valid())
                {
#ifdef DEBUG_MESSAGES
                    printf("GNSS FIX ACQUIRED\n");
#endif
                    sync_set_clock_source(CLOCK_SOURCE_GNSS);
                    sync_state_machine_set_state(SYNC_STARTUP);
                }
            }
            if(!esp_detected || !esp_ntp_valid)
            {
#ifdef DEBUG_MESSAGES
                printf("LOST NTP SYNC\n");
#endif
                sync_state_machine_set_state(sync_select_best_clock());
            }
            break;
            
        case SYNC_NTP_ADJUST:
            if(state_new_oc)
            {
                sync_state_machine_set_state(SYNC_NTP);
                state_new_oc = 0;
            }
            break;
            
        case SYNC_NTP_NO_NETWORK:
            if(state_new_oc)
            {
                state_new_oc = 0;
            }
            if(gnss_detected)
            {
                if(ubx_gnss_time_valid())
                {
#ifdef DEBUG_MESSAGES
                    printf("GNSS FIX ACQUIRED\n");
#endif
                    sync_set_clock_source(CLOCK_SOURCE_GNSS);
                    sync_state_machine_set_state(SYNC_STARTUP);
                    break;
                }
            }
            if(esp_ntp_valid)
            {
#ifdef DEBUG_MESSAGES
                printf("NTP SYNC ACQUIRED\n");
#endif
                sync_set_clock_source(CLOCK_SOURCE_NTP);
                sync_state_machine_set_state(SYNC_NTP);
            }
            break;
        
        case SYNC_RTC:
            if(gnss_detected)
            {
                if(ubx_gnss_time_valid())
                {
#ifdef DEBUG_MESSAGES
                    printf("GNSS FIX ACQUIRED\n");
#endif
                    sync_set_clock_source(CLOCK_SOURCE_GNSS);
                    sync_state_machine_set_state(SYNC_STARTUP);
                    break;
                }
            }
            if(esp_detected)
            {
                if(esp_ntp_valid)
                {
#ifdef DEBUG_MESSAGES
                    printf("NTP SYNC ACQUIRED\n");
#endif
                    sync_set_clock_source(CLOCK_SOURCE_NTP);
                    sync_state_machine_set_state(SYNC_NTP);
                    break;
                }
            }
            break;
        
        case SYNC_NO_CLOCK:
            if(state_new_oc)
            {
#ifdef DEBUG_MESSAGES
                printf("NO CLOCK!\n");
#endif
                if(!ui_in_menu())
                {
                    if(no_clock_blink)
                    {
                        ui_set_display_dashes();
                    }
                    else
                    {
                        ui_set_display_off();
                    }
                }
                no_clock_blink = !no_clock_blink;
                state_new_oc = 0;
            }
            if(gnss_detected)
            {
                if(ubx_gnss_time_valid())
                {
#ifdef DEBUG_MESSAGES
                    printf("GNSS FIX ACQUIRED\n");
#endif
                    sync_set_clock_source(CLOCK_SOURCE_GNSS);
                    sync_state_machine_set_state(SYNC_STARTUP);
                    break;
                }
            }
            if(esp_detected)
            {
                if(esp_ntp_valid)
                {
#ifdef DEBUG_MESSAGES
                    printf("NTP SYNC ACQUIRED\n");
#endif
                    sync_set_clock_source(CLOCK_SOURCE_NTP);
                    sync_state_machine_set_state(SYNC_NTP);
                    break;
                }
            }
            break;
        
        case SYNC_GNSS_WAIT_FOR_FIX:
            if(sync_state_detect_timeout < GNSS_FIX_LIMIT)
            {
                sync_state_detect_timeout++;
                if(ubx_gnss_time_valid())
                {
#ifdef DEBUG_MESSAGES
                    printf("GNSS FIX ACQUIRED\n");
#endif
                    sync_state_machine_set_state(SYNC_STARTUP);
                }
            }
            else
            {
#ifdef DEBUG_MESSAGES
                printf("NO GNSS FIX... ");
#endif
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
#ifdef DEBUG_MESSAGES
                    printf("GNSS DETECTED\n");
#endif
                    sync_state_machine_set_state(sync_select_best_clock());
                }
            }
            else
            {
#ifdef DEBUG_MESSAGES
                printf("NO GNSS DETECTED... ");
#endif
                sync_state_machine_set_state(sync_select_best_clock());
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
                    sync_set_clock_source(CLOCK_SOURCE_NTP);
                    sync_state_machine_set_state(SYNC_GNSS_DETECT);
                }
            }
            else
            {
                if(esp_detected) 
                {
#ifdef DEBUG_MESSAGES
                    printf("ESP DETECTED BUT NO NTP SYNC\n");
#endif
                }
                else
                {
#ifdef DEBUG_MESSAGES
                    printf("NO ESP DETECTED\n");
#endif
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
                    if(rtc_valid)
                    {
#ifdef DEBUG_MESSAGES
                        printf("RTC DETECTED\n");
#endif
                        sync_set_clock_source(CLOCK_SOURCE_RTC);
                    }
                    sync_state_machine_set_state(SYNC_NTP_DETECT);
                }
            }
            else
            {
#ifdef DEBUG_MESSAGES
                printf("NO RTC DETECTED\n");
#endif
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
    
    state_new_oc = 0;
    clock_sync_state_old2 = clock_sync_state_old;
    clock_sync_state_old = clock_sync_state;
}

void sync_state_machine_set_state(CLOCK_SYNC_STATUS state)
{
    clock_sync_state_last = clock_sync_state;
    clock_sync_state = state;
    sync_state_detect_timeout = 0;
#ifdef DEBUG_MESSAGES
    printf("\n");
    sync_state_print(clock_sync_state);
    printf("\n\n");
#endif
}

void print_sync_state_machine(void)
{
    printf("\n=== STATE MACHINE ===\n");
    printf("STATE: ");
    sync_state_print(clock_sync_state);
    printf(" OLD: ");
    sync_state_print(clock_sync_state_old2);
    printf(" LAST: ");
    sync_state_print(clock_sync_state_last);
    printf("\nLAST SYNC CAUSE: ");
    sync_state_print(last_sync_cause);
    printf("\n");
}

CLOCK_SYNC_STATUS sync_select_best_clock(void)
{
    if(ubx_gnss_time_valid())
    {
#ifdef DEBUG_MESSAGES
        printf("GNSS MODE\n");
#endif
        sync_set_clock_source(CLOCK_SOURCE_GNSS);
        return SYNC_STARTUP;
    }
    if(esp_ntp_valid)
    {
#ifdef DEBUG_MESSAGES
        printf("NTP MODE\n");
#endif
        sync_set_clock_source(CLOCK_SOURCE_NTP);
        return SYNC_NTP;
    }
    /* Not actually used yet
    else if(esp_ntp_detected)
    {
        printf("NTP MODE - NO NETWORK\n");
        return SYNC_NTP_NO_NETWORK;
    }
    */
    else if(rtc_valid)
    {
#ifdef DEBUG_MESSAGES
        printf("RTC MODE\n");
#endif
        sync_set_clock_source(CLOCK_SOURCE_RTC);
        return SYNC_RTC;
    }
    else
    {
#ifdef DEBUG_MESSAGES
        printf("NO CLOCK DETECTED\n");
#endif
        sync_set_clock_source(CLOCK_SOURCE_NONE);
        return SYNC_NO_CLOCK;
    }
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

        case SYNC_RTC:
            printf("SYNC_RTC");
            break;

        case SYNC_NTP:
            printf("SYNC_NTP");
            break;

        case SYNC_NTP_ADJUST:
            printf("SYNC_NTP_ADJUST");
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
    printf("CLK D: %li CLK T: %li\n",accumulated_clocks, accumulation_delta);
    printf("PPS D:%lu OC D:%li\n\n", pps_count_diff, oc_offset);
    printf("AVG D: %.1f\n", accumulated_clocks_diff_avg);
}

void sync_state_eval_time(void)
{
    if(ubx_gnss_time_valid())
    {
        //Check that UTC and GNSS time match
        if(!gnss_is_calendar_sync(utc))
        {
            gnss_set_calendar();
            rtc_write_from_calendar(utc);
        }
    }
    else if(esp_ntp_valid)
    {
        if(!ntp_is_calendar_sync(utc))
        {
            esp_ntp_set_calendar();
            rtc_write_from_calendar(utc);
        }
    }
    else if(rtc_valid)
    {
        if(!rtc_is_calendar_sync(utc))
        {
            rtc_set_calendar();
        }
    }
    
    local = get_local_time(utc);
    dst_active = isDST(&utc);

#ifdef DEBUG_MESSAGES    
    print_clocks();
#endif
}

void print_clocks(void)
{
    printf("\nUTC:   ");
    ui_print_iso8601_string(utc);
    printf("\n");
    
    printf("GNSS:  ");
    ui_print_iso8601_string(gnss);
    if(!gnss_detected)
    {
        printf(" - MISSING");
    }
    printf("\n");
    
    printf("NTP:   ");
    ui_print_iso8601_string(ntp);
    if(!esp_detected)
    {
        printf(" - MISSING");
    }
    else if(!esp_wifi_status)
    {
        printf(" - NO WIFI");
    }
    else if(!esp_ntp_valid)
    {
        printf(" - NO NTP SYNC");
    }
    printf("\n");

    printf("ESP:   ");
    ui_print_iso8601_string(esp);
    if(!esp_detected)
    {
        printf(" - MISSING");
    }
    printf("\n");
    
    printf("RTC:   ");
    ui_print_iso8601_string(rtc);
    if(!rtc_detected)
    {
        printf(" - MISSING");
    }
    printf("\n");
    
    printf("Local: ");
    ui_print_iso8601_string_local(utc);
    printf("\n");
    
    printf("UTC source: ");
    print_clock_source(utc_source);
    printf("\n");
}

void sync_set_clock_source(CLOCK_SOURCE source)
{
#ifdef DEBUG_MESSAGES
    printf("Switching to ");
    print_clock_source(source);
    printf("\n");
#endif
    switch(source)
    {
        case CLOCK_SOURCE_NONE:
            utc_source = CLOCK_SOURCE_NONE;
            break;
            
        case CLOCK_SOURCE_RTC:
            rtc_set_calendar();
            utc_source = CLOCK_SOURCE_RTC;
            break;
            
        case CLOCK_SOURCE_ESP:
            break;
            
        case CLOCK_SOURCE_NTP:
            esp_ntp_set_calendar();
            utc_source = CLOCK_SOURCE_NTP;
            break;
            
        case CLOCK_SOURCE_GNSS:
            gnss_set_calendar();
            utc_source = CLOCK_SOURCE_GNSS;
            pic_pps_reset_sync();
            reset_pps_stats();
            break;
            
        default:
            print_clock_source(source);
            break;
    }

    ui_display_task();
}

void print_clock_source(CLOCK_SOURCE source)
{
    switch(source)
    {
        case CLOCK_SOURCE_NONE:
            printf("NONE");
            break;
            
        case CLOCK_SOURCE_RTC:
            printf("RTC");
            break;
            
        case CLOCK_SOURCE_ESP:
            printf("ESP");
            break;
            
        case CLOCK_SOURCE_NTP:
            printf("NTP");
            break;
            
        case CLOCK_SOURCE_GNSS:
            printf("GNSS");
            break;
            
        default:
            printf("UNKNOWN SOURCE!");
            break;
    }
}