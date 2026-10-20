#include "pic_report.h"

#include "../timing/ntp.h"
#include "../timing/pps.h"
#include "../util/time_text.h"
#include "pic_link.h"

typedef struct
{
    uint16_t code;
    const char *name;
} Name;

static const Name commands[] = {
    {SML_USER_INFO, "INFO"},
    {SML_USER_REQUEST_MESSAGE, "REQUEST_MESSAGE"},
    {SML_USER_RESYNC, "RESYNC"},
    {SML_USER_RESET, "RESET"},
    {SML_USER_SET_RTC, "SET_RTC"},
    {SML_USER_SET_TZ_OFFSET, "SET_TZ_OFFSET"},
    {SML_USER_SET_TZ_AUTO, "SET_TZ_AUTO"},
    {SML_USER_SET_DST_OFFSET, "SET_DST_OFFSET"},
    {SML_USER_SET_DST_AUTO, "SET_DST_AUTO"},
    {SML_USER_SET_DST_ACTIVE, "SET_DST_ACTIVE"},
    {SML_USER_SET_FOSC, "SET_FOSC"},
    {SML_USER_SET_ALARM_ENABLE, "SET_ALARM_ENABLE"},
    {SML_USER_SET_ALARM, "SET_ALARM"},
    {SML_USER_SET_DELTA, "SET_DELTA"},
    {SML_USER_SET_BEEP, "SET_BEEP"},
    {SML_USER_SET_DISPLAY, "SET_DISPLAY"},
    {SML_USER_SET_BRI_AUTO, "SET_BRI_AUTO"},
    {SML_USER_SET_BRIGHTNESS, "SET_BRIGHTNESS"},
    {SML_USER_SHOW_SETTINGS, "SHOW_SETTINGS"},
    {SML_USER_SHOW_CONFIG, "SHOW_CONFIG"},
    {SML_USER_DEFAULTS, "DEFAULTS"},
    {SML_USER_SAVE, "SAVE"},
    {SML_USER_ENTER_BOOTLOADER, "ENTER_BOOTLOADER"},
    {SML_USER_ESP_RESET, "ESP_RESET"},
    {SML_USER_ESP_WIFI_CLEAR, "ESP_WIFI_CLEAR"},
    {SML_USER_ESP_WIFI_SETUP, "ESP_WIFI_SETUP"},
    {SML_USER_ESP_NTP_RESYNC, "ESP_NTP_RESYNC"},
    {SML_USER_ESP_TZINFO_ENABLE, "ESP_TZINFO_ENABLE"},
    {SML_USER_ESP_TZINFO_UPDATE, "ESP_TZINFO_UPDATE"},
    {SML_USER_ESP_UPDATE_CHECK, "ESP_UPDATE_CHECK"},
    {SML_USER_ESP_DEFAULTS, "ESP_DEFAULTS"},
};

static const char *lookup(const Name *names, size_t count, uint16_t code)
{
    size_t index;

    for (index = 0; index < count; index++)
    {
        if (names[index].code == code)
        {
            return names[index].name;
        }
    }
    return "?";
}


const char *pic_report_command_name(uint16_t command)
{
    return lookup(commands, sizeof(commands) / sizeof(commands[0]), command);
}


const char *pic_report_result_name(uint16_t result)
{
    static const char *const names[] = {"ACK", "NACK", "ACCEPTED"};

    return result < 3 ? names[result] : "?";
}


const char *pic_report_error_name(uint16_t error)
{
    static const char *const names[] = {"none",   "unsupported",    "length", "argument",
                                        "busy",   "stale revision", "schema", "storage",
                                        "verify", "state"};

    return error < 10 ? names[error] : "?";
}


const char *pic_report_source_name(uint8_t source)
{
    static const char *const names[] = {"INIT", "NONE", "RTC", "ESP", "NTP", "GNSS"};

    return source < 6 ? names[source] : "?";
}


const char *pic_report_lock_name(uint8_t lock)
{
    static const char *const names[] = {"UNINITIALISED", "ACQUIRING", "LOCKED", "HOLDOVER",
                                        "FREE_RUNNING"};

    return lock < 5 ? names[lock] : "?";
}


const char *pic_report_tz_source_name(uint8_t source)
{
    static const char *const names[] = {"NONE",   "CACHE",  "GEOIP",  "GNSS",
                                        "MANUAL", "PINNED", "SPECIAL"};

    return source < 7 ? names[source] : "?";
}


static const char *sync_name(uint8_t state)
{
    static const char *const names[] = {"INIT", "FREE_RUN", "ACQUIRE", "LOCKED", "HOLDOVER"};

    return state < 5 ? names[state] : "?";
}


static const char *cause_name(uint8_t cause)
{
    static const char *const names[] = {"none",        "qualified",     "tracking",
                                        "phase lost",  "source lost",   "source back",
                                        "alternative", "better source", "expired"};

    return cause < 9 ? names[cause] : "?";
}


static const char *control_name(uint8_t state)
{
    static const char *const names[] = {"INIT", "ACQUIRE", "STEP", "TRACK"};

    return state < 4 ? names[state] : "?";
}


static const char *fix_name(uint8_t fix)
{
    static const char *const names[] = {"no fix", "dead reckoning", "2D",
                                        "3D",     "GNSS + DR",      "time only"};

    return fix < 6 ? names[fix] : "?";
}


static void print_time(Stream *output)
{
    SML_TIME_AUTH_PAYLOAD auth;
    SML_STAT_UI_PAYLOAD ui;
    PicTime time;
    PpsEdge gnss;
    int64_t ntp_utc;

    output->printf("\n=== PIC time ===\n");
    if (!pic_link_get(SML_CLASS_TIME, SML_TIME_AUTH, &auth, PIC_LINK_STATE_TIMEOUT_MS))
    {
        output->printf("No TIME-AUTH from the PIC%s\n",
                       pic_link_is_sml() ? "" : " (not in SML mode)");
        return;
    }
    output->printf("UTC:    ");
    if (auth.flags & SML_AUTH_UTC_VALID)
    {
        time_print_utc(output, auth.utc_seconds);
        output->printf(" + ");
        time_print_ns(output, auth.utc_phase_ns);
        if (auth.flags & SML_AUTH_UNCERTAINTY_VALID)
        {
            output->printf(", +/-");
            time_print_ns(output, auth.uncertainty_ns);
        }
    }
    else
    {
        output->printf("unknown");
    }
    output->printf("\nSource: %s, %s\n", pic_report_source_name(auth.source),
                   pic_report_lock_name(auth.lock_state));
    if (pic_link_get(SML_CLASS_STAT, SML_STAT_UI, &ui, PIC_LINK_STATE_TIMEOUT_MS) &&
        (ui.flags & SML_UI_LOCAL_VALID) && (auth.flags & SML_AUTH_UTC_VALID))
    {
        int32_t offset = ui.base_offset_s + ((ui.flags & SML_UI_DST_ACTIVE) ? ui.dst_offset_s : 0);

        output->printf("Local:  ");
        time_print_local(output, auth.utc_seconds, offset);
        output->printf(" (base ");
        time_print_offset(output, ui.base_offset_s);
        output->printf(", DST ");
        time_print_offset(output, ui.dst_offset_s);
        output->printf(" %s)%s%s%s\n", (ui.flags & SML_UI_DST_ACTIVE) ? "active" : "inactive",
                       (ui.flags & SML_UI_TZ_HELD) ? "" : " no schedule held",
                       (ui.flags & SML_UI_TZ_STALE) ? " STALE" : "",
                       (ui.flags & SML_UI_TZ_AMBIGUOUS) ? " AMBIGUOUS" : "");
    }
    if (pic_link_time(&time))
    {
        if (ntp_utc_at(time.edge_local_us, &ntp_utc))
        {
            output->printf("Epoch vs NTP: ");
            time_print_ns(output, (time.edge_utc_us - ntp_utc) * 1000);
            output->printf(" (+ = PIC ahead)\n");
        }
        if (pps_gnss_latest(&gnss))
        {
            int64_t difference = time.edge_local_us - gnss.local_us;

            difference = ((difference + 500000) % 1000000 + 1000000) % 1000000 - 500000;
            output->printf("Epoch vs GNSS PPS on the ESP: %lld us\n", (long long)difference);
        }
    }
}


static void print_sync(Stream *output)
{
    SML_TIME_PIC_PAYLOAD pic;
    SML_TIME_CTRL_PAYLOAD ctrl;

    if (pic_link_get(SML_CLASS_TIME, SML_TIME_PIC, &pic, PIC_LINK_STATE_TIMEOUT_MS))
    {
        output->printf("\n=== PIC synchroniser ===\n");
        output->printf("State:  %s (was %s, %s), %lu changes\n", sync_name(pic.sync_state),
                       sync_name(pic.previous_sync_state), cause_name(pic.last_sync_cause),
                       (unsigned long)pic.sync_events);
        output->printf("Epoch:  %lu, ", (unsigned long)pic.epoch_sequence);
        if (pic.flags & SML_PIC_FREQUENCY_VALID)
        {
            output->printf("FCY %lu.%06lu MHz, ", (unsigned long)(pic.fosc_hz / 1000000),
                           (unsigned long)(pic.fosc_hz % 1000000));
        }
        if (pic.flags & SML_PIC_PHASE_VALID)
        {
            output->printf("phase %ld cycles, ", (long)pic.phase_cycles);
        }
        output->printf("%lu missing PPS\n", (unsigned long)pic.missing_pps_events);
    }
    if (pic_link_get(SML_CLASS_TIME, SML_TIME_CTRL, &ctrl, PIC_LINK_STATE_TIMEOUT_MS))
    {
        output->printf("Loop:   %s", control_name(ctrl.controller_state));
        if (ctrl.flags & SML_CTRL_ESTIMATE_VALID)
        {
            output->printf(", oscillator %+.4f ppm", (float)ctrl.frequency_offset_q16 / 65536.0f /
                                                         (float)ctrl.nominal_period_ticks * 1e6f);
        }
        output->printf(", %lu gate rejections\n", (unsigned long)ctrl.gate_rejections);
    }
}


static void print_gnss(Stream *output)
{
    SML_TIME_GNSS_PAYLOAD gnss;

    if (!pic_link_get(SML_CLASS_TIME, SML_TIME_GNSS, &gnss, PIC_LINK_STATE_TIMEOUT_MS))
    {
        return;
    }
    output->printf("\n=== GNSS (PIC) ===\n");
    if (!(gnss.flags & SML_GNSS_DETECTED))
    {
        output->printf("No receiver detected\n");
        return;
    }
    output->printf("Fix:    %s%s, UTC %s, tAcc %lu ns\n", fix_name(gnss.fix_type),
                   (gnss.flags & SML_GNSS_FIX_OK) ? " OK" : "",
                   (gnss.flags & SML_GNSS_UTC_VALID) ? "valid" : "invalid",
                   (unsigned long)gnss.utc_accuracy_ns);
    if (gnss.flags & SML_GNSS_POSITION_VALID)
    {
        output->printf("Position: %.7f %.7f, %.1f m MSL, hAcc %.1f m\n",
                       (float)gnss.latitude_e7 / 1e7f, (float)gnss.longitude_e7 / 1e7f,
                       (float)gnss.height_msl_mm / 1000.0f,
                       (float)gnss.horizontal_accuracy_mm / 1000.0f);
    }
    if (gnss.flags & SML_GNSS_LEAP_VALID)
    {
        output->printf("Leap:   TAI-UTC %d s", gnss.gps_utc_s + 19);
        if (gnss.flags & SML_GNSS_LEAP_EVENT_VALID)
        {
            output->printf(", %+d at ", gnss.leap_change);
            time_print_utc(output, gnss.leap_event_utc);
        }
        output->printf("\n");
    }
}


void pic_report_print_link(Stream *output)
{
    const PicLinkStats *stats = pic_link_stats();
    const SmlParserStats *parser = pic_link_parser_stats();

    output->printf("\n=== PIC link ===\n");
    output->printf("Mode:   %s\n", pic_link_is_sml() ? "SML" : "no SML frames (TEXT or silent)");
    output->printf("Frames: %lu in (%lu text, %lu unknown, %lu wrong length), %lu out\n",
                   (unsigned long)stats->frames, (unsigned long)stats->text_frames,
                   (unsigned long)stats->unknown, (unsigned long)stats->wrong_length,
                   (unsigned long)stats->sent);
    output->printf("Parser: %lu noise bytes, %lu CRC errors, %lu length errors, %lu timeouts\n",
                   (unsigned long)parser->noise_bytes, (unsigned long)parser->crc_errors,
                   (unsigned long)parser->length_errors, (unsigned long)parser->timeouts);
    output->printf("Commands: %lu to the PIC, %lu answered, %lu unanswered; %lu from it\n",
                   (unsigned long)stats->requests, (unsigned long)stats->responses,
                   (unsigned long)stats->unanswered, (unsigned long)stats->commands);
    output->printf("Epoch pairing: %lu paired, %lu not, %lu backlogs\n",
                   (unsigned long)stats->pairings, (unsigned long)stats->unpaired,
                   (unsigned long)stats->backlogs);
}


void pic_report_print(Stream *output)
{
    print_time(output);
    print_sync(output);
    print_gnss(output);
    pic_report_print_link(output);
}
