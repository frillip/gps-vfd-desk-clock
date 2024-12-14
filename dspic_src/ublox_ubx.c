#include "ublox_ubx.h"

extern time_t utc;
extern time_t gnss;

extern bool gnss_detected;


UBX_NAV_CLOCK ubx_nav_clock_buffer;
UBX_HEADER ubx_nav_clock_string = {
                                    .fields.preamble_sync_1 = UBX_HEADER_PREAMBLE_1,
                                    .fields.preamble_sync_2 =  UBX_HEADER_PREAMBLE_2,
                                    .fields.class = UBX_HEADER_CLASS_NAV,
                                    .fields.id = UBX_HEADER_ID_NAV_CLOCK,
                                    .fields.length = sizeof(struct _ubx_nav_clock_payload)
};
bool ubx_nav_clock_waiting = 0;
uint32_t ubx_nav_clock_tow_ms = 0;
int32_t ubx_nav_clock_bias_ns = 0;
int32_t ubx_nav_clock_drift_nss = 0;
uint32_t ubx_nav_clock_accuracy_ns = 0;
uint32_t ubx_nav_clock_f_accuracy_pss = 0;


UBX_NAV_POSLLH ubx_nav_posllh_buffer;
UBX_HEADER ubx_nav_posllh_string = {
                                    .fields.preamble_sync_1 = UBX_HEADER_PREAMBLE_1,
                                    .fields.preamble_sync_2 =  UBX_HEADER_PREAMBLE_2,
                                    .fields.class = UBX_HEADER_CLASS_NAV,
                                    .fields.id = UBX_HEADER_ID_NAV_POSLLH,
                                    .fields.length = sizeof(struct _ubx_nav_posllh_payload)
};
bool ubx_nav_posllh_waiting = 0;
int32_t ubx_nav_posllh_lat = 0;
int32_t ubx_nav_posllh_lon = 0;
int32_t ubx_nav_posllh_height = 0;
int32_t ubx_nav_posllh_hmsl = 0;
uint32_t ubx_nav_posllh_hacc = 0;
uint32_t ubx_nav_posllh_vacc = 0;


UBX_NAV_STATUS ubx_nav_status_buffer;
UBX_HEADER ubx_nav_status_string = {
                                    .fields.preamble_sync_1 = UBX_HEADER_PREAMBLE_1,
                                    .fields.preamble_sync_2 =  UBX_HEADER_PREAMBLE_2,
                                    .fields.class = UBX_HEADER_CLASS_NAV,
                                    .fields.id = UBX_HEADER_ID_NAV_STATUS,
                                    .fields.length = sizeof(struct _ubx_nav_status_payload)
};
bool ubx_nav_status_waiting = 0;
UBX_NAV_STATUS_GPSFIX ubx_nav_status_gpsfix = 0;
bool ubx_nav_status_gpsfixok = 0;


UBX_NAV_TIMEUTC ubx_nav_timeutc_buffer;
UBX_HEADER ubx_nav_timeutc_string = {
                                    .fields.preamble_sync_1 = UBX_HEADER_PREAMBLE_1,
                                    .fields.preamble_sync_2 =  UBX_HEADER_PREAMBLE_2,
                                    .fields.class = UBX_HEADER_CLASS_NAV,
                                    .fields.id = UBX_HEADER_ID_NAV_TIMEUTC,
                                    .fields.length = sizeof(struct _ubx_nav_timeutc_payload)
};
bool ubx_nav_timeutc_waiting = 0;
bool ubx_nav_timeutc_valid = 0;
uint32_t ubx_nav_timeutc_accuracy_ns = 0;



UBX_TIM_TM2 ubx_tim_tm2_buffer;
UBX_HEADER ubx_tim_tm2_string = {
                                    .fields.preamble_sync_1 = UBX_HEADER_PREAMBLE_1,
                                    .fields.preamble_sync_2 =  UBX_HEADER_PREAMBLE_2,
                                    .fields.class = UBX_HEADER_CLASS_TIM,
                                    .fields.id = UBX_HEADER_ID_TIM_TM2,
                                    .fields.length = sizeof(struct _ubx_tim_tm2_payload)
};
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


bool gnss_fix;
bool print_ubx_tim_tm2 = 0;
bool print_ubx_nav_timeutc = 0;
bool print_ubx_nav_clock = 0;
bool print_ubx_nav_status = 0;
bool print_ubx_nav_posllh = 0;


void process_ubx_nav_clock(void)
{
    ubx_nav_clock_tow_ms = ubx_nav_clock_buffer.fields.payload.itow;
    ubx_nav_clock_bias_ns = ubx_nav_clock_buffer.fields.payload.clkb;
    ubx_nav_clock_drift_nss = ubx_nav_clock_buffer.fields.payload.clkd;
    ubx_nav_clock_accuracy_ns = ubx_nav_clock_buffer.fields.payload.tacc;
    ubx_nav_clock_f_accuracy_pss = ubx_nav_clock_buffer.fields.payload.facc;
    
    memset(ubx_nav_clock_buffer.raw, 0, sizeof(ubx_nav_clock_buffer));
}

void print_ubx_nav_clock_data(void)
{
    printf("\n=== UBX-NAV-CLOCK ===\n");
    if(print_ubx_nav_clock)
    {
        printf("ToW: %lu\n", ubx_nav_clock_tow_ms);
        printf("Bias: %lins Drift: %lins/s\n",ubx_nav_clock_bias_ns, ubx_nav_clock_drift_nss);
        printf("Acc: %luns fAcc: %lups/s\n",ubx_nav_clock_accuracy_ns, ubx_nav_clock_f_accuracy_pss);
    }
    else
    {
        printf("No new data\n");
    }
}


void process_ubx_nav_posllh(void)
{
    ubx_nav_posllh_lon = ubx_nav_posllh_buffer.fields.payload.lon;
    ubx_nav_posllh_lat = ubx_nav_posllh_buffer.fields.payload.lat;
    ubx_nav_posllh_height = ubx_nav_posllh_buffer.fields.payload.height;
    ubx_nav_posllh_hmsl = ubx_nav_posllh_buffer.fields.payload.hmsl;
    ubx_nav_posllh_hacc = ubx_nav_posllh_buffer.fields.payload.hacc;
    ubx_nav_posllh_vacc = ubx_nav_posllh_buffer.fields.payload.vacc;

    memset(ubx_nav_posllh_buffer.raw, 0, sizeof(ubx_nav_posllh_buffer));
}

void print_ubx_nav_posllh_data(void)
{
    printf("\n=== UBX-NAV-POSLLH ===\n");
    if(print_ubx_nav_posllh)
    {
        int16_t lat_d = ubx_nav_posllh_lat / 10000000;
        int32_t lat_p = ubx_nav_posllh_lat - (lat_d * 10000000);
        if(lat_p<0) lat_p = (lat_p ^ 0xFFFFFFFF) +1; // dirty abs() equivalent again
        printf("LAT: %4i.%7li\n", lat_d, lat_p);
        
        int16_t lon_d = ubx_nav_posllh_lon / 10000000;
        int32_t lon_p = ubx_nav_posllh_lon - (lon_d * 10000000);
        if(lon_p<0) lon_p = (lon_p ^ 0xFFFFFFFF) +1; // dirty abs() equivalent again
        printf("LON: %4i.%7li\n", lon_d, lon_p);
        
        double height_m = ubx_nav_posllh_height;
        height_m = height_m / 1000;
        double hmsl_m = ubx_nav_posllh_hmsl;
        hmsl_m = hmsl_m / 1000;
        printf("Height: %4.0fm aMSL: %4.0fm\n", height_m, hmsl_m);
        
        double hacc_m = ubx_nav_posllh_hacc;
        hacc_m = hacc_m / 1000;
        double vacc_m = ubx_nav_posllh_vacc;
        vacc_m = vacc_m / 1000;
        printf("Acc H: %6.3fm V: %6.3fm\n" ,hacc_m, vacc_m);
        
    }
    else
    {
        printf("No new data\n");
    }
}


void process_ubx_nav_status(void)
{
    ubx_nav_status_gpsfix = ubx_nav_status_buffer.fields.payload.gpsfix;
    ubx_nav_status_gpsfixok = ubx_nav_status_buffer.fields.payload.flags.gpsfixok;
    
    if(ubx_nav_status_gpsfixok)
    {
        if((ubx_nav_status_gpsfix == GPSFIX_3D) || (ubx_nav_status_gpsfix == GPSFIX_GPS_DR) || (ubx_nav_status_gpsfix == GPSFIX_TIME_ONLY))
        gnss_fix = 1;
    }
    else gnss_fix = 0;
    
    memset(ubx_nav_status_buffer.raw, 0, sizeof(ubx_nav_status_buffer));
}

void print_ubx_nav_status_data(void)
{
    printf("\n=== UBX-NAV-STATUS ===\n");
    if(print_ubx_nav_status)
    {
        printf("Fix type: 0x%02X - ", ubx_nav_status_gpsfix);
        switch (ubx_nav_status_gpsfix)
        {
            case GPSFIX_NO_FIX:
                printf("No fix\n");
                break;
                
            case GPSFIX_DR_ONLY:
                printf("Dead reckoning only\n");
                break;

            case GPSFIX_2D:
                printf("2D-fix\n");
                break;

            case GPSFIX_3D:
                printf("3D-fix\n");
                break;

            case GPSFIX_GPS_DR:
                printf("GNSS + dead reckoning combined\n");
                break;

            case GPSFIX_TIME_ONLY:
                printf("Time only fix\n");
                break;
                
            default:
                printf("Unknown\n");
                break;
        }
        printf("Fix ok: %i\n", ubx_nav_status_gpsfixok);
    }
    else
    {
        printf("No new data\n");
    }
}


time_t process_ubx_nav_timeutc(void)
{
    struct tm gnss_time;
    gnss_time.tm_isdst = 0;
    
    gnss_time.tm_sec = ubx_nav_timeutc_buffer.fields.payload.sec;
    gnss_time.tm_min = ubx_nav_timeutc_buffer.fields.payload.min;
    gnss_time.tm_hour = ubx_nav_timeutc_buffer.fields.payload.hour;
    gnss_time.tm_mday = ubx_nav_timeutc_buffer.fields.payload.day;
    gnss_time.tm_mon = ubx_nav_timeutc_buffer.fields.payload.month - 1; // tm_mon is zero indexed for no reason
    gnss_time.tm_year = ubx_nav_timeutc_buffer.fields.payload.year - 1900; //tm_year is indexed from 1900
    
    ubx_nav_timeutc_accuracy_ns = ubx_nav_timeutc_buffer.fields.payload.tacc;
    
    ubx_nav_timeutc_valid = ubx_nav_timeutc_buffer.fields.payload.valid.validutc;
    memset(ubx_nav_timeutc_buffer.raw, 0, sizeof(ubx_nav_timeutc_buffer));
    time_t ubx_time;
    ubx_time = mktime(&gnss_time);
    return ubx_time;
}

void print_ubx_nav_timeutc_data(void)
{
    printf("\n=== UBX-NAV-TIMEUTC ===\n");
    if(print_ubx_nav_timeutc)
    {
        printf("UTC: ");
        ui_print_iso8601_string(gnss);
        printf("\nAcc: %luns Val: %i\n",ubx_nav_timeutc_accuracy_ns, ubx_nav_timeutc_valid);
    }
    else
    {
        printf("No new data\n");
    }
}


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
    ubx_tim_tm2_edge_count = ubx_tim_tm2_buffer.fields.payload.count;
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
    ubx_tim_tm2_rising_ms = ubx_tim_tm2_buffer.fields.payload.towmsr;
    // Byte 18-21 is rising edge ms fraction in ns
    ubx_tim_tm2_rising_ns = ubx_tim_tm2_buffer.fields.payload.towsubmsr;
    // Byte 22-25 is falling edge time of week in ms
    ubx_tim_tm2_falling_ms = ubx_tim_tm2_buffer.fields.payload.towmsf;
    // Byte 26-29 is falling edge ms fraction in ns
    ubx_tim_tm2_falling_ns = ubx_tim_tm2_buffer.fields.payload.towsubmsf;
    // Byte 30-33 is accuracy estimate in ns
    ubx_tim_tm2_accuracy_ns = ubx_tim_tm2_buffer.fields.payload.accest;
    
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
    
    memset(ubx_tim_tm2_buffer.raw, 0, sizeof(ubx_tim_tm2_buffer));
}

void print_ubx_tim_tm2_data(void)
{
    printf("\n=== UBX-TIM-TM2 ===\n");
    if(print_ubx_tim_tm2)
    {
        printf("Rm: %lu Fm: %lu Dm: %li\n",ubx_tim_tm2_rising_ms, ubx_tim_tm2_falling_ms, ubx_tim_tm2_ms_diff);
        printf("Rmd: %li Fmd: %li\n",ubx_tim_tm2_rising_ms_diff, ubx_tim_tm2_falling_ms_diff);
        printf("Rn: %lu Fn: %lu Dn: %li\n",ubx_tim_tm2_rising_ns, ubx_tim_tm2_falling_ns, ubx_tim_tm2_ns_diff);
        printf("Rnd: %li Fnd: %li Dnd: %li\n",ubx_tim_tm2_rising_ns_diff, ubx_tim_tm2_falling_ns_diff, ubx_tim_tm2_ns_diff_diff);
        printf("Cnt: %u Acc: %luns Val: %i\n",ubx_tim_tm2_edge_count, ubx_tim_tm2_accuracy_ns, ubx_tim_tm2_valid);
        print_ubx_tim_tm2 = 0;
    }
    else
    {
        printf("No new data\n");
    }
}


void print_ubx_data(void)
{
    if(gnss_detected)
    {
        print_gnss_pps_info();
        print_ubx_nav_status_data();
        print_ubx_nav_timeutc_data();
        print_ubx_nav_clock_data();
        print_ubx_nav_posllh_data();
        print_ubx_tim_tm2_data();
    }
    else
    {
        printf("\n=== NO GNSS DETECTED ===\n");
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

extern bool esp_gnss_data_updated;

void ubx_data_task(void)
{
    if(ubx_gnss_available())
    {
        ubx_update_gnss_time();
        esp_gnss_data_updated = 1;
    }
        
    // Is there new time mark data available
    if(ubx_timemark_waiting()) ubx_update_timemark();
}

void ubx_invalidate_data(void)
{
    ubx_tim_tm2_waiting = 0; // Invalidate GNSS data
    ubx_nav_timeutc_waiting = 0;
    ubx_nav_clock_waiting = 0;
    ubx_nav_status_waiting = 0;
    ubx_nav_posllh_waiting = 0;
}