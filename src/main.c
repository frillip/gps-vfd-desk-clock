#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include "mcc_generated_files/mcc.h"
#include "tubes.h"
#include "gps.h"
#include "gps_pps.h"
#include "pic_pps.h"
#include "pdo.h"
#include "scheduler.h"
#include "ds1307.h"
#include "fe5680a_58.h"
#include "sc16is7x0.h"

//#define DEBUG_ENABLED
#define LOCAL_TIME 1
#define TZ_OFFSET 0
#define DST_ENABLED 1
#define DST_OFFSET 3600

extern bool ubx_waiting;
extern bool ubx_valid;
extern uint16_t ubx_edge_count;
extern uint32_t ubx_rising_ms;
extern uint32_t ubx_rising_ms_old;
extern int32_t ubx_rising_ms_diff;
extern uint32_t ubx_rising_ns;
extern uint32_t ubx_rising_ns_old;
extern int32_t ubx_rising_ns_diff;
extern uint32_t ubx_falling_ms;
extern uint32_t ubx_falling_ms_old;
extern int32_t ubx_falling_ms_diff;
extern uint32_t ubx_falling_ns;
extern uint32_t ubx_falling_ns_old;
extern int32_t ubx_falling_ns_diff;
extern int32_t ubx_ms_diff;
extern int32_t ubx_ns_diff;
extern uint32_t ubx_accuracy_ns;

extern bool rmc_waiting;
uint8_t timezone = 1;

extern uint16_t ic1_val;
extern uint16_t ic1_val_old;
extern uint16_t ic2_val;
extern uint16_t ic2_val_old;
extern uint32_t pps_count;
extern uint32_t pps_count_diff;
extern uint32_t pps_count_old;
extern uint16_t pps_seq_count;

int32_t accumulated_clocks = 0;
time_t accumulation_start = 0;
time_t accumulation_delta = 0;

extern uint16_t ic3_val;
extern uint16_t ic3_val_old;
extern uint16_t ic4_val;
extern uint16_t ic4_val_old;
extern uint32_t oc_count;
extern int32_t oc_offset;
extern uint32_t oc_count_diff;
extern uint32_t oc_count_old;

uint16_t pdo_adc = 0;
float pdo_mv = 0;
float pps_offset_ns = 0;

extern bool oc_adjust_in_progress;
extern bool pps_sync;
extern bool pps_done;
extern bool oc_event;

bool gps_calendar_sync = 0;
int minute = 0;
int old_minute = 0;
extern bool gps_fix;

bool print_data = 0;
bool print_ubx = 0;
bool disable_manual_print = 0;

long double r_val;
uint32_t f_val;
extern char fe5680_config_string[64];
extern enum fe5680_state rb_state;

bool rtc_sync = 0;

extern bool scheduler_sync;
extern bool scheduler_adjust_in_progress;
extern uint8_t t1ms0;
extern uint8_t t10ms0;
extern uint8_t t100ms0;
extern uint8_t t100ms1;

uint8_t bcd2bin(uint8_t val);
uint8_t bin2bcd(uint8_t val);
void incr_clock(void);
void set_latch_cycles(uint32_t);

int main(void)
{
    // initialize the device
    PIN_MANAGER_Initialize();
    DELAY_milliseconds(50);
    // STSEL 1; IREN disabled; PDSEL 8N; UARTEN enabled; RTSMD disabled; USIDL disabled; WAKE disabled; ABAUD disabled; LPBACK disabled; BRGH enabled; URXINV disabled; UEN TX_RX; 
    // Data Bits = 8; Parity = None; Stop Bits = 1;
    U1MODE = (0x8008 & ~(1<<15));  // disabling UARTEN bit
    // UTXISEL0 TX_ONE_CHAR; UTXINV disabled; OERR NO_ERROR_cleared; URXISEL RX_ONE_CHAR; UTXBRK COMPLETED; UTXEN disabled; ADDEN disabled; 
    U1STA = 0x00;
    // BaudRate = 115200; Frequency = 3685000 Hz; BRG 7; 
    U1BRG = 0x07;
    
    U1MODEbits.UARTEN = 1;   // enabling UART ON bit
    U1STAbits.UTXEN = 1;
    printf("\033[2J\033[1;1H"); // Clear the terminal window
    printf("\r\nHELLO!\r\n\r\n"); // And say hello!
    printf("Running on FRC, switching to Rb\r\n");
    printf("Waiting for Rb lock");
    
    SPI2_Initialize();
    display_blink_start(3685000UL);
    while(!OSC_READY_GetValue());
    DELAY_milliseconds(20);
    display_blink_stop();
    
    CLOCK_Initialize();
    INTERRUPT_Initialize();
    UART1_Initialize();
    pic_pps_init();
    gps_pps_init();
    UART2_Initialize();
    I2C1_Initialize();
    SPI2_Initialize();
    pdo_init();
    INTERRUPT_GlobalEnable();
    SYSTEM_CORCONModeOperatingSet(CORCON_MODE_PORVALUES);
    DELAY_milliseconds(100); // Wait for things to wake up
    printf(" locked!\r\n\r\n");
    
    CN_SetInterruptHandler(incr_clock);
    UART2_SetRxInterruptHandler(rx_gps);
    scheduler_init();
    ADC1_ChannelSelect(PDO);
    
    display_init();

    // time_t to store UTC, GPS and RTC time
    time_t utc;
    time_t gps;
    time_t rtc;
    time_t local;
    
    sc16is7x0_init(FRBSET, 9600L);
    r_val = 50255055;
    r_val += 0.433269;
    f_val = 0x32F0AD97;
    // Read RTC for an estimate of current time and display it
    rtc = DS1307_read();
    utc = rtc;
#ifdef DEBUG_ENABLED
    display_mmss(&utc);
#else
    if(LOCAL_TIME)
    {
        local = utc;
        if(DST_ENABLED && isDST(&local)) local += DST_OFFSET;
        local += TZ_OFFSET;
        display_time(&local);
    }
    else display_time(&utc);
#endif
    
    // ISO8601 string buffer
    char buf[32] = {0};
    
    while (1)
    {
        // Every 1ms
        if(t1ms0)
        {
            t1ms0=0;
            // Have we had an OC event?
            if(oc_event)
            {
                // Calculate our PPS and OC stats
                pps_count = (((uint32_t)ic2_val)<<16) + ic1_val; // Raw timer
                pps_count_diff = pps_count-pps_count_old; // Difference from last
                pps_count_old = pps_count; // Store the new value as old
                accumulated_clocks += pps_count_diff;
                while(accumulated_clocks>30000000) accumulated_clocks -= 40000000;
                accumulation_delta = utc - accumulation_start;

                oc_count = (((uint32_t)ic4_val)<<16) + ic3_val; // Raw timer
                oc_count_diff = oc_count - oc_count_old; // Difference from last
                oc_offset = pps_count-oc_count; // Calculate the offset between PPS and OC
                oc_count_old = oc_count; // Store the new value as old
                
                // Do we need to resync?
                if((pps_seq_count>10)&&((oc_offset<-132)||(oc_offset>-92)))
                {
                    pps_sync = 0;
                    scheduler_sync = 0;
                    gps_calendar_sync = 0;
                    rtc_sync = 0;
                }
                // Only sync if required and if we get 40000000 cycles
                if(!pps_sync && pps_count_diff == 40000000)
                {
                    set_latch_cycles(40000000 + oc_offset+107);
                    oc_adjust_in_progress = 1;
                    if(!accumulation_start)
                    {
                        accumulation_start = utc;
                        accumulated_clocks = 0;
                    }
                }

                // Print resulting time to serial
                struct tm *utc_oc;
                utc_oc = gmtime(&utc);
                strftime(buf, 32, "UTC: %Y-%m-%dT%H:%M:%SZ", utc_oc);
                printf(buf);
                printf("\r\n");
                
                // Every minute, print some statistics
                minute =  utc_oc->tm_min;
                if(minute!=old_minute) print_data = 1;
                old_minute = minute;
                
#ifdef DEBUG_ENABLED
                print_data = 1;
#endif
                oc_event = 0;
            }
        }
        // Every 10ms
        if(t10ms0)
        {
            t10ms0=0;
            
            if(ubx_waiting)
            {
                ubx_waiting = 0;
                process_ubx();
                print_ubx = 1;
            }
            
            // Check for any bytes on UART1
            while(U1STAbits.URXDA)
            {
                char c = UART1_Read();
                // print some data if enter has been pressed
                if(c==0x0d && !disable_manual_print)
                {
                    print_data = 1;
                    // Disable spamming in case of cats on keyboards
                    disable_manual_print = 1;
                }
            }
            // Print some statistics if required
            if(print_data)
            {
#ifdef DEBUG_ENABLED
                    printf("\033[2J\033[1;1H"); // Clear the terminal window
#endif
                // Cycles between current and last PPS, and the OC offset from this
                printf("PPS D:%lu OC D:%li\r\n", pps_count_diff, oc_offset);
                // Raw timer values for both PPS and OC
                printf("PPS C:%lu OC C:%li\r\n", pps_count, oc_count);
                // PPS sync status
                printf("PPS S: %i ADJ: %i\r\n", pps_sync, oc_adjust_in_progress);
                // Scheduler sync status
                printf("SCH S: %i GPS FIX: %i\r\n", scheduler_sync, gps_fix);
                // PD output information
                printf("mV: %.0f ns: %.0f\r\n",pdo_mv, pps_offset_ns);
                printf("CLK D: %li CLK T: %li\r\n",accumulated_clocks, accumulation_delta);
                if(print_ubx)
                {
                    print_ubx_data();
                    print_ubx = 0;
                }
                else
                {
                    printf("No new UBX time mark data\r\n");
                }
                
                if(pps_sync && accumulation_delta > 30)
                {
                    long double acc;
                    acc = fe5680_calc_rb_acc(accumulated_clocks, accumulation_delta);
                    long double freq;
                    freq = fe5680_calc_rb_freq(acc);
                    long double new_r_val;
                    new_r_val = fe5680_calc_rb_r_val(freq, f_val);
                    uint32_t new_f_val;
                    new_f_val = fe5680_calc_rb_f_val(freq, f_val);

                    // Stupid double decimal hacks incoming:
                    uint32_t freq_i = freq;
                    float freq_f = freq - freq_i;
                    uint32_t r_val_i = r_val;
                    float r_val_f = r_val - r_val_i;
                    uint32_t new_r_val_i = new_r_val;
                    float new_r_val_f = new_r_val - new_r_val_i;

                    printf("Rb freq: %lu.%03.0fHz Rb acc: %.3lfppt\r\n",freq_i, freq_f*1000, acc*TRILLION);
                    printf("Rb old R: %lu.%03.0fHz Rb old F: %0lX \r\n",r_val_i, r_val_f*1000, f_val);
                    printf("Rb new R: %lu.%03.0fHz Rb new F: %0lX \r\n",new_r_val_i, new_r_val_f*1000, new_f_val);
                }
                else
                {
                    printf("More time required for Rb tuning\r\n");
                }
                
                printf("\r\n");
                print_data = 0;
            }
        }
        // Every 100ms
        if(t100ms0==1)
        {
            t100ms0 = 0;
            STATUS_LED_Toggle(); // Toggle the LED for no reason
            // Do we have a GPS message waiting?
            if(rmc_waiting)
            {
                rmc_waiting=0;
                // Process our waiting GNRMC message into our time_t
                gps = process_rmc();

                // Do we have a valid time from GPS
                if(gps_fix)
                {
                    // Check if we've still got the correct time
                    if(utc!=gps)
                    {
                        // Trigger a re-sync if not
                        gps_calendar_sync = 0;
                        rtc_sync = 0;
                    }

                    //If we're not sync'd
                    if(!gps_calendar_sync)
                    {
                        // Copy GPS time into our UTC calendar
                        utc = gps;
                        // Print resulting time to serial
                        char buf[32] = {0};
                        struct tm *utc_time;
                        utc_time = gmtime(&utc);
                        strftime(buf, 32, "%Y-%m-%dT%H:%M:%SZ", utc_time);
                        printf("GPS calendar sync\r\nTime is now: ");
                        printf(buf);
                        printf("\r\n");

                        // Update our RTC now we have a GPS time
                        if(!rtc_sync)
                        {
                            printf("Writing RTC\r\n");
                            rtc_sync = DS1307_write(utc);
                        }
                        // Our internal calendar is now sync'd with GPS
                        gps_calendar_sync = 1;
                    }
                }
            }
        }
        // Every 1000ms, but at a 900ms offset
        if(t100ms1==9)
        {
            t100ms1 = -1; // Reset to -1 to trigger every 1s at 900ms offset
            // Increment for next PPS and load into display
            utc++;
#ifdef DEBUG_ENABLED
            display_mmss(&utc);
#else
            if(LOCAL_TIME)
            {
                local = utc;
                if(DST_ENABLED && isDST(&local)) local += DST_OFFSET;
                local += TZ_OFFSET;
                display_time(&local);
            }
            else display_time(&utc);
#endif
            // Re-enable manual printing
            disable_manual_print = 0;
            
            if(rb_state==FE5680_INIT)
            {
                fe5680_get_config_string();
                rb_state=FE5680_READ_CONFIG;
            }
            else if(rb_state==FE5680_READ_CONFIG)
            {
                if(fe5680_process_config_string(&fe5680_config_string))
                {
                    r_val = fe5680_get_r_val(&fe5680_config_string);
                    f_val = fe5680_get_f_val(&fe5680_config_string);
                    if((f_val > 0x32E0AD97) && (f_val < 0x3300AD97))
                    {
                        if((r_val > 50155055) && (r_val < 50355055))
                        {
                            rb_state=FE5680_GOT_CONFIG;
                        }
                        else rb_state=FE5680_INIT;
                    }
                    else rb_state=FE5680_INIT;
                }
                else
                {
                    rb_state=FE5680_INIT;
                }
            }
            /*
            else if(rb_state==FE5680_GOT_CONFIG)
            {
                fe5680_set_f_val(f_val);
                rb_state=FE5680_ADJ_FVAL;
            }
            else if(rb_state==FE5680_ADJ_FVAL)
            {
                if(fe5680_get_response()) rb_state=FE5680_RESP_OK;
                else rb_state=FE5680_INIT;
            }
            */
        }
    }
    return 1; 
}

void incr_clock(void)
{
    if(PPS_GetValue())
    {
        // Wait for at least 2us for the capacitor to stop charging
        DELAY_microseconds(2);
        // Trigger a conversion
        AD1CON1bits.SAMP=1;
        // Wait a bit more...
        DELAY_microseconds(2);
        // Stop sampling and convert
        AD1CON1bits.SAMP=0;
        // Wait for conversion to finish
        while(!AD1CON1bits.DONE);
        // Read ADC
        pdo_adc = ADC1BUF0;
        // Convert reading to mV
        pdo_mv = pdo_calculate_mv(pdo_adc);
        // Convert from mV to nanoseconds
        pps_offset_ns =  pdo_calculate_ns(pdo_mv);
    }
}

/**
 End of File
*/