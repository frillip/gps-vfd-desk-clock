#include "ublox_ubx.h"

extern time_t gnss;

char ubx_tim_tm2_buffer[UBX_TIM_TM2_LENGTH] = {0};
char ubx_tim_tm2_string[GNSS_CHECK_BUFFER_SIZE] = {0xB5, 0x62, 0x0D, 0x03, 0x1C, 0x00};
bool ubx_tim_tm2_waiting = 0;
bool ubx_tim_tm2_valid = 0;
uint16_t ubx_tim_tm2_edge_count = 0;
uint16_t ubx_tim_tm2_edge_count_old = 0;
uint32_t ubx_tim_tm2_rising_ms = 0;
uint32_t ubx_tim_tm2_rising_ms_old = 0;
int32_t ubx_tim_tm2_rising_ms_diff = 0;
uint32_t ubx_tim_tm2_rising_ns = 0;
uint32_t ubx_tim_tm2_rising_ns_old = 0;
int32_t ubx_tim_tm2_rising_ns_diff = 0;
uint32_t ubx_tim_tm2_falling_ms = 0;
uint32_t ubx_tim_tm2_falling_ms_old = 0;
int32_t ubx_tim_tm2_falling_ms_diff = 0;
uint32_t ubx_tim_tm2_falling_ns = 0;
uint32_t ubx_tim_tm2_falling_ns_old = 0;
int32_t ubx_tim_tm2_falling_ns_diff = 0;
int32_t ubx_tim_tm2_ms_diff = 0;
int32_t ubx_tim_tm2_ns_diff = 0;
int32_t ubx_tim_tm2_ns_diff_old = 0;
int32_t ubx_tim_tm2_ns_diff_diff = 0;
uint32_t ubx_tim_tm2_accuracy_ns = 0;

char ubx_nav_timeutc_buffer[UBX_NAV_TIMEUTC_LENGTH] = {0};
char ubx_nav_timeutc_string[GNSS_CHECK_BUFFER_SIZE] = {0xB5, 0x62, 0x01, 0x21, 0x14, 0x00};
bool ubx_nav_timeutc_waiting = 0;
bool ubx_nav_timeutc_valid = 0;
uint32_t ubx_nav_timeutc_accuracy_ns = 0;

char ubx_nav_clock_buffer[UBX_NAV_CLOCK_LENGTH] = {0};
char ubx_nav_clock_string[GNSS_CHECK_BUFFER_SIZE] = {0xB5, 0x62, 0x01, 0x22, 0x14, 0x00};
bool ubx_nav_clock_waiting = 0;
uint32_t ubx_nav_clock_tow_ms = 0;
int32_t ubx_nav_clock_bias_ns = 0;
int32_t ubx_nav_clock_drift_nss = 0;
uint32_t ubx_nav_clock_accuracy_ns = 0;
uint32_t ubx_nav_clock_f_accuracy_pss = 0;

char ubx_nav_status_buffer[UBX_NAV_STATUS_LENGTH] = {0};
char ubx_nav_status_string[GNSS_CHECK_BUFFER_SIZE] = {0xB5, 0x62, 0x01, 0x03, 0x10, 0x00};
bool ubx_nav_status_waiting = 0;
uint8_t ubx_nav_status_gpsfix = 0;
bool ubx_nav_status_gpsfixok = 0;

char ubx_nav_posllh_buffer[UBX_NAV_POSLLH_LENGTH] = {0};
char ubx_nav_posllh_string[GNSS_CHECK_BUFFER_SIZE] = {0xB5, 0x62, 0x01, 0x02, 0x1C, 0x00};
bool ubx_nav_posllh_waiting = 0;
int32_t ubx_nav_posllh_lat = 0;
int32_t ubx_nav_posllh_lon = 0;
int32_t ubx_nav_posllh_height = 0;
int32_t ubx_nav_posllh_hmsl = 0;
uint32_t ubx_nav_posllh_hacc = 0;
uint32_t ubx_nav_posllh_vacc = 0;

bool gnss_fix;
bool print_ubx_tim_tm2 = 0;
bool print_ubx_nav_timeutc = 0;
bool print_ubx_nav_clock = 0;
bool print_ubx_nav_status = 0;
bool print_ubx_nav_posllh = 0;

void process_ubx_tim_tm2(void)
{
    ubx_tim_tm2_rising_ms_old = ubx_tim_tm2_rising_ms;
    ubx_tim_tm2_rising_ns_old = ubx_tim_tm2_rising_ns;
    ubx_tim_tm2_falling_ms_old = ubx_tim_tm2_falling_ms;
    ubx_tim_tm2_falling_ns_old = ubx_tim_tm2_falling_ns;
    ubx_tim_tm2_edge_count_old = ubx_tim_tm2_edge_count;
    ubx_tim_tm2_ns_diff_old = ubx_tim_tm2_ns_diff;
    // ublox protocol message UBX-TIM-TM2
    // Byte 0-1 is header 0xB5 0x62 = µb
    // Byte 2 is 0x0D = timing message TIM
    // Byte 3 is 0x03 = Time mark message TM2
    // Byte 4-5 is message length, always 0x1C 0x00 = 28 bytes
    // Byte 6 is channel event occurred on, 0 = EXTINT0, 1 = EXTINT1
    // Byte 7 is bitfield, UBX-13003221 - R26  page 430
    // Byte 8-9 is edge count
    ubx_tim_tm2_edge_count = ubx_tim_tm2_buffer[8] + (ubx_tim_tm2_buffer[9]<<8);
    if(ubx_tim_tm2_edge_count==ubx_tim_tm2_edge_count_old +1)
    {
        ubx_tim_tm2_valid = 1;
    }
    else
    {
        ubx_tim_tm2_valid = 0;
    }
    // Byte 10-11 is week number rising edge
    // Byte 12-13 is week number falling edge
    // Byte 14-17 is rising edge time of week in ms
    memcpy(&ubx_tim_tm2_rising_ms, ubx_tim_tm2_buffer+14, 4);
    // Byte 18-21 is rising edge ms fraction in ns
    memcpy(&ubx_tim_tm2_rising_ns, ubx_tim_tm2_buffer+18, 4);
    // Byte 22-25 is falling edge time of week in ms
    memcpy(&ubx_tim_tm2_falling_ms, ubx_tim_tm2_buffer+22, 4);
    // Byte 26-29 is falling edge ms fraction in ns
    memcpy(&ubx_tim_tm2_falling_ns, ubx_tim_tm2_buffer+26, 4);
    // Byte 30-33 is accuracy estimate in ns
    memcpy(&ubx_tim_tm2_accuracy_ns, ubx_tim_tm2_buffer+30, 4);
    
    //ubx_tim_tm2_rising_ns = ubx_tim_tm2_rising_ns + ((uint32_t)(ubx_tim_tm2_rising_ms)*1000000);
    //ubx_tim_tm2_falling_ns = ubx_tim_tm2_falling_ns + ((uint32_t)(ubx_tim_tm2_falling_ms)*1000000);
    
    ubx_tim_tm2_rising_ms_diff = ubx_tim_tm2_rising_ms - ubx_tim_tm2_rising_ms_old;
    ubx_tim_tm2_rising_ns_diff = ubx_tim_tm2_rising_ns - ubx_tim_tm2_rising_ns_old;
    ubx_tim_tm2_falling_ms_diff = ubx_tim_tm2_falling_ms - ubx_tim_tm2_falling_ms_old;
    ubx_tim_tm2_falling_ns_diff = ubx_tim_tm2_falling_ns - ubx_tim_tm2_falling_ns_old;
    
    ubx_tim_tm2_ms_diff = ubx_tim_tm2_falling_ms - ubx_tim_tm2_rising_ms;
    //if(ubx_tim_tm2_ms_diff < 0) ubx_tim_tm2_ms_diff += 1000;
    
    ubx_tim_tm2_ns_diff = ubx_tim_tm2_falling_ns - ubx_tim_tm2_rising_ns;
    ubx_tim_tm2_ns_diff_diff = ubx_tim_tm2_ns_diff - ubx_tim_tm2_ns_diff_old;
    //if(ubx_tim_tm2_ns_diff < 0) ubx_tim_tm2_ns_diff += 1000000000;
    
    memset(ubx_tim_tm2_buffer, 0, UBX_TIM_TM2_LENGTH);
}

void print_ubx_tim_tm2_data(void)
{
    printf("\r\n=== UBX-TIM-TM2 ===\r\n");
    if(print_ubx_tim_tm2)
    {
        printf("Rm: %lu Fm: %lu Dm: %li\r\n",ubx_tim_tm2_rising_ms, ubx_tim_tm2_falling_ms, ubx_tim_tm2_ms_diff);
        printf("Rmd: %li Fmd: %li\r\n",ubx_tim_tm2_rising_ms_diff, ubx_tim_tm2_falling_ms_diff);
        printf("Rn: %lu Fn: %lu Dn: %li\r\n",ubx_tim_tm2_rising_ns, ubx_tim_tm2_falling_ns, ubx_tim_tm2_ns_diff);
        printf("Rnd: %li Fnd: %li Dnd: %li\r\n",ubx_tim_tm2_rising_ns_diff, ubx_tim_tm2_falling_ns_diff, ubx_tim_tm2_ns_diff_diff);
        printf("Cnt: %u Acc: %luns Val: %i\r\n",ubx_tim_tm2_edge_count, ubx_tim_tm2_accuracy_ns, ubx_tim_tm2_valid);
        print_ubx_tim_tm2 = 0;
    }
    else
    {
        printf("No new data\r\n");
    }
}

time_t process_ubx_nav_timeutc(void)
{
    struct tm gnss_time;
    gnss_time.tm_isdst = 0;
    
    gnss_time.tm_sec = ubx_nav_timeutc_buffer[24];
    gnss_time.tm_min = ubx_nav_timeutc_buffer[23];
    gnss_time.tm_hour = ubx_nav_timeutc_buffer[22];
    gnss_time.tm_mday = ubx_nav_timeutc_buffer[21];
    gnss_time.tm_mon = ubx_nav_timeutc_buffer[20];
    gnss_time.tm_mon -= 1; // tm_mon is zero indexed for no reason
    gnss_time.tm_year = ((uint16_t)ubx_nav_timeutc_buffer[18]&0xFF);
    gnss_time.tm_year += (((uint16_t)ubx_nav_timeutc_buffer[19]&0xFF)<<8)-1900;
    

    memcpy(&ubx_nav_timeutc_accuracy_ns, ubx_nav_timeutc_buffer+10, 4);
    
    ubx_nav_timeutc_valid = (ubx_nav_timeutc_buffer[25]&0x04)>>2;
    memset(ubx_nav_timeutc_buffer, 0, UBX_NAV_TIMEUTC_LENGTH);
    time_t ubx_time;
    ubx_time = mktime(&gnss_time);
    return ubx_time;
}

void print_ubx_nav_timeutc_data(void)
{
    printf("\r\n=== UBX-NAV-TIMEUTC ===\r\n");
    if(print_ubx_nav_timeutc)
    {
        printf("UTC: ");
        ui_print_iso8601_string(gnss);
        printf("\r\nAcc: %luns Val: %i\r\n",ubx_nav_timeutc_accuracy_ns, ubx_nav_timeutc_valid);
    }
    else
    {
        printf("No new data\r\n");
    }
}

void process_ubx_nav_clock(void)
{
    memcpy(&ubx_nav_clock_tow_ms, ubx_nav_clock_buffer+6, 4);
    memcpy(&ubx_nav_clock_bias_ns,ubx_nav_clock_buffer+10, 4);
    memcpy(&ubx_nav_clock_drift_nss,ubx_nav_clock_buffer+14, 4);
    memcpy(&ubx_nav_clock_accuracy_ns,ubx_nav_clock_buffer+18, 4);
    memcpy(&ubx_nav_clock_f_accuracy_pss,ubx_nav_clock_buffer+22, 4);
    
    memset(ubx_nav_clock_buffer, 0, UBX_NAV_CLOCK_LENGTH);
}

void print_ubx_nav_clock_data(void)
{
    printf("\r\n=== UBX-NAV-CLOCK ===\r\n");
    if(print_ubx_nav_clock)
    {
        printf("ToW: %lu\r\n", ubx_nav_clock_tow_ms);
        printf("Bias: %lins Drift: %lins/s\r\n",ubx_nav_clock_bias_ns, ubx_nav_clock_drift_nss);
        printf("Acc: %luns fAcc: %lups/s\r\n",ubx_nav_clock_accuracy_ns, ubx_nav_clock_f_accuracy_pss);
    }
    else
    {
        printf("No new data\r\n");
    }
}

void process_ubx_nav_status(void)
{
    ubx_nav_status_gpsfix = ubx_nav_status_buffer[10];
    ubx_nav_status_gpsfixok = ubx_nav_status_buffer[10]&0x01;
    
    if(ubx_nav_status_gpsfix>=0x03 && ubx_nav_status_gpsfix<=0x05 && ubx_nav_status_gpsfixok)
    {
        gnss_fix = 1;
    }
    else gnss_fix = 0;
    
    memset(ubx_nav_status_buffer, 0, UBX_NAV_STATUS_LENGTH);
}

void print_ubx_nav_status_data(void)
{
    printf("\r\n=== UBX-NAV-STATUS ===\r\n");
    if(print_ubx_nav_status)
    {
        printf("Fix type: %i - ", ubx_nav_status_gpsfix);
        if(ubx_nav_status_gpsfix==0x00) printf("no fix\r\n");
        else if(ubx_nav_status_gpsfix==0x01) printf("dead reckoning only\r\n");
        else if(ubx_nav_status_gpsfix==0x02) printf("2D-fix\r\n");
        else if(ubx_nav_status_gpsfix==0x03) printf("3D-fix\r\n");
        else if(ubx_nav_status_gpsfix==0x04) printf("GNSS + dead reckoning combined\r\n");
        else if(ubx_nav_status_gpsfix==0x05) printf("time only fix\r\n");
        else printf("unknown\r\n");
        printf("Fix ok: %i\r\n", ubx_nav_status_gpsfixok);
    }
    else
    {
        printf("No new data\r\n");
    }
}

void process_ubx_nav_posllh(void)
{
    memcpy(&ubx_nav_posllh_lon, ubx_nav_posllh_buffer+10, 4);
    memcpy(&ubx_nav_posllh_lat, ubx_nav_posllh_buffer+14, 4);
    memcpy(&ubx_nav_posllh_height, ubx_nav_posllh_buffer+18, 4);
    memcpy(&ubx_nav_posllh_hmsl, ubx_nav_posllh_buffer+22, 4);
    memcpy(&ubx_nav_posllh_hacc, ubx_nav_posllh_buffer+26, 4);
    memcpy(&ubx_nav_posllh_vacc, ubx_nav_posllh_buffer+30, 4);

    memset(ubx_nav_posllh_buffer, 0, UBX_NAV_POSLLH_LENGTH);
}

void print_ubx_nav_posllh_data(void)
{
    printf("\r\n=== UBX-NAV-POSLLH ===\r\n");
    if(print_ubx_nav_posllh)
    {
        int16_t lat_d = ubx_nav_posllh_lat / 10000000;
        int32_t lat_p = ubx_nav_posllh_lat - (lat_d * 10000000);
        if(lat_p<0) lat_p = (lat_p ^ 0xFFFFFFFF) +1; // dirty abs() equivalent again
        printf("LAT: %4i.%7li\r\n", lat_d, lat_p);
        
        int16_t lon_d = ubx_nav_posllh_lon / 10000000;
        int32_t lon_p = ubx_nav_posllh_lon - (lon_d * 10000000);
        if(lon_p<0) lon_p = (lon_p ^ 0xFFFFFFFF) +1; // dirty abs() equivalent again
        printf("LON: %4i.%7li\r\n", lon_d, lon_p);
        
        float height_m = ubx_nav_posllh_height;
        height_m = height_m / 1000;
        float hmsl_m = ubx_nav_posllh_hmsl;
        hmsl_m = hmsl_m / 1000;
        printf("Height: %4.0fm aMSL: %4.0fm\r\n", height_m, hmsl_m);
        
        float hacc_m = ubx_nav_posllh_hacc;
        hacc_m = hacc_m / 1000;
        float vacc_m = ubx_nav_posllh_vacc;
        vacc_m = vacc_m / 1000;
        printf("Acc H: %6.3fm V: %6.3fm\r\n" ,hacc_m, vacc_m);
        
    }
    else
    {
        printf("No new data\r\n");
    }
}

bool ubx_gnss_available(void)
{
    if(ubx_nav_timeutc_waiting && ubx_nav_status_waiting && ubx_nav_clock_waiting && ubx_nav_posllh_waiting) return 1;
    else return 0;
}

void ubx_update_gnss_time(void)
{
    ubx_nav_timeutc_waiting = 0;
    gnss = process_ubx_nav_timeutc();
    print_ubx_nav_timeutc = 1;

    ubx_nav_status_waiting = 0;
    process_ubx_nav_status();
    print_ubx_nav_status = 1;

    ubx_nav_clock_waiting = 0;
    process_ubx_nav_clock();
    print_ubx_nav_clock = 1;
    
    ubx_nav_posllh_waiting = 0;
    process_ubx_nav_posllh();
    print_ubx_nav_posllh = 1;
}

bool ubx_gnss_time_valid(void)
{
    if(gnss_fix && ubx_nav_timeutc_valid) return 1;
    else return 0;
}

bool ubx_timemark_waiting(void)
{
    return ubx_tim_tm2_waiting;
}

void ubx_update_timemark(void)
{
    ubx_tim_tm2_waiting = 0;
    process_ubx_tim_tm2();
    print_ubx_tim_tm2 = 1;
}