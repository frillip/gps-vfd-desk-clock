#include "tzinfo.h"

#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFiClientSecure.h>
#include <stdlib.h>

#include "../console/console.h"
#include "../net/root_ca.h"
#include "../pic/pic_link.h"
#include "../pic/pic_report.h"
#include "../settings/user_prefs.h"
#include "../timing/esp_time.h"
#include "../util/time_text.h"
#include "../version.h"

#define FIRST_FIX_WAIT_MS 60000U
#define RETRY_FIRST_S     300U
#define RETRY_MAX_S       3600U
#define RETRY_EMPTY_S     30U  // first retry while no schedule is held at all
#define RETRY_EMPTY_MAX_S 300U // ... doubling to this
#define MOVE_RECHECK_MS   60000U
#define RESEND_MS         600000U
#define HELD_RESEND_MS    5000U
#define REFRESH_MIN_S     900U
#define HTTP_TIMEOUT_MS   8000U
#define NVS_NAMESPACE     "TZINFO"
#define NVS_KEY           "SCHED_V1"

typedef struct
{
    bool have;      // a position to send
    int32_t lat_e7; // rounded to the precision
    int32_t lon_e7;
    int32_t raw_lat_e7; // as the receiver gave it
    int32_t raw_lon_e7;
    int8_t precision;
} Position;

typedef struct
{
    TzSchedule schedule; // held: cached or looked up
    bool looked_up;      // since start
    bool last_ok;
    char last_error[96];
    uint32_t updated_utc;
    uint32_t lookups;
    uint32_t failures;
    uint32_t network_since_ms;
    uint32_t next_lookup_ms;
    uint32_t refresh_s; // after the latest success
    uint32_t retry_s;
    uint32_t empty_retry_s; // ... while no schedule is held
    bool force;             // look up as soon as the network is up: no wait for a fix
    uint32_t last_move_check_ms;
    Position used; // the position of the latest lookup
    bool send_now;
    uint32_t last_sent_ms;
    uint32_t sent;
} TzInfo;

static TzInfo tz;

static int32_t power10(int8_t exponent)
{
    int32_t value = 1;

    while (exponent-- > 0)
    {
        value *= 10;
    }
    return value;
}

// Rounds 1e-7 degrees to precision decimal places, still in 1e-7 degrees.
static int32_t round_e7(int32_t value, int8_t precision)
{
    int32_t step = power10((int8_t)(7 - precision));
    int64_t magnitude = value < 0 ? -(int64_t)value : value;
    int64_t rounded = (magnitude + step / 2) / step * step;

    return (int32_t)(value < 0 ? -rounded : rounded);
}

// A rounded value as "51.5", "-0.1": no floating point, so the digits are
// exactly the rounding.
static void format_e7(char *text, size_t size, int32_t value, int8_t precision)
{
    int64_t magnitude = (value < 0 ? -(int64_t)value : value) / power10((int8_t)(7 - precision));
    char fraction[8];
    int8_t digit;

    for (digit = (int8_t)(precision - 1); digit >= 0; digit--)
    {
        fraction[digit] = (char)('0' + magnitude % 10);
        magnitude /= 10;
    }
    fraction[precision] = '\0';
    snprintf(text, size, "%s%lld%s%s", value < 0 ? "-" : "", (long long)magnitude,
             precision > 0 ? "." : "", fraction);
}

// The PIC reports no GNSS receiver at all: no point waiting for a fix.
static bool receiver_absent(void)
{
    SML_TIME_GNSS_PAYLOAD gnss;

    return pic_link_get(SML_CLASS_TIME, SML_TIME_GNSS, &gnss, PIC_LINK_STATE_TIMEOUT_MS) &&
           (gnss.flags & SML_GNSS_DETECTED) == 0;
}

static Position current_position(void)
{
    Position position = {false, 0, 0, 0, 0, 0};
    SML_TIME_GNSS_PAYLOAD gnss;
    int32_t precision = user_prefs.fields.rtzinfo.gnss_accuracy;

    if (precision < 0 ||
        !pic_link_get(SML_CLASS_TIME, SML_TIME_GNSS, &gnss, PIC_LINK_STATE_TIMEOUT_MS) ||
        (gnss.flags & (SML_GNSS_POSITION_VALID | SML_GNSS_FIX_OK)) !=
            (SML_GNSS_POSITION_VALID | SML_GNSS_FIX_OK))
    {
        return position;
    }
    position.have = true;
    position.precision = (int8_t)precision;
    position.raw_lat_e7 = gnss.latitude_e7;
    position.raw_lon_e7 = gnss.longitude_e7;
    position.lat_e7 = round_e7(gnss.latitude_e7, position.precision);
    position.lon_e7 = round_e7(gnss.longitude_e7, position.precision);
    return position;
}

// Moved since the position used: a whole rounding step away from it (half a
// step of hysteresis, so a clock on a rounding boundary does not flap), or a
// fix now where the last lookup had none.
static bool has_moved(const Position *now, const Position *used)
{
    int64_t step = power10((int8_t)(7 - now->precision));

    if (!now->have)
    {
        return false;
    }
    if (!used->have || used->precision != now->precision)
    {
        return true;
    }
    return llabs((int64_t)now->raw_lat_e7 - used->lat_e7) > step ||
           llabs((int64_t)now->raw_lon_e7 - used->lon_e7) > step;
}

static String lookup_url(const Position *position)
{
    String url =
        String("https://") + user_prefs.fields.rtzinfo.server + user_prefs.fields.rtzinfo.path;

    if (position->have)
    {
        char lat[24];
        char lon[24];

        format_e7(lat, sizeof(lat), position->lat_e7, position->precision);
        format_e7(lon, sizeof(lon), position->lon_e7, position->precision);
        url += String("?lat=") + lat + "&lon=" + lon;
    }
    return url;
}

// One request. True with *result filled; else tz.last_error says why.
static bool lookup(const Position *position, TzSchedule *result)
{
    WiFiClientSecure client;
    HTTPClient http;
    String url = lookup_url(position);
    String body;
    int status;
    TzParseResult parsed;
    char message[64];

    tz.lookups++;
    client.setCACert(root_ca_pem);
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.setConnectTimeout(HTTP_TIMEOUT_MS);
    if (!http.begin(client, url))
    {
        snprintf(tz.last_error, sizeof(tz.last_error), "bad URL");
        return false;
    }
    http.setUserAgent(ESP_USER_AGENT_STRING);
    status = http.GET();
    if (status > 0)
    {
        body = http.getString();
    }
    http.end();
    if (status <= 0)
    {
        snprintf(tz.last_error, sizeof(tz.last_error), "HTTP: %s",
                 HTTPClient::errorToString(status).c_str());
        return false;
    }
    parsed = tz_schedule_parse_v1(body.c_str(), body.length(), result, message, sizeof(message));
    if (status != 200 || parsed != TZ_PARSE_OK)
    {
        snprintf(tz.last_error, sizeof(tz.last_error), "HTTP %d, %s%s%s", status,
                 tz_parse_name(parsed), message[0] != '\0' ? ": " : "", message);
        return false;
    }
    return true;
}

static void save(void)
{
    Preferences store;

    store.begin(NVS_NAMESPACE, false);
    store.putBytes(NVS_KEY, &tz.schedule, sizeof(tz.schedule));
    store.end();
}

static void load(Stream *output)
{
    Preferences store;
    TzSchedule cached;

    store.begin(NVS_NAMESPACE, true);
    bool found = store.getBytesLength(NVS_KEY) == sizeof(cached) &&
                 store.getBytes(NVS_KEY, &cached, sizeof(cached)) == sizeof(cached);
    store.end();
    if (found && cached.valid && cached.count >= 1 && cached.count <= TZ_SCHEDULE_ENTRIES)
    {
        tz.schedule = cached;
        tz.schedule.source = SML_TZ_SOURCE_CACHE;
        output->printf("Time zone: %s (cached)\n", tz.schedule.zone);
    }
}

static void schedule_next(bool ok)
{
    uint32_t delay_s;

    if (ok)
    {
        delay_s = tz.refresh_s;
        tz.retry_s = RETRY_FIRST_S;
        tz.empty_retry_s = RETRY_EMPTY_S;
    }
    else if (!tz.schedule.valid)
    {
        // Nothing to give the PIC, which shows UTC meanwhile: retry soon.
        delay_s = tz.empty_retry_s;
        tz.empty_retry_s =
            tz.empty_retry_s * 2 > RETRY_EMPTY_MAX_S ? RETRY_EMPTY_MAX_S : tz.empty_retry_s * 2;
    }
    else
    {
        delay_s = tz.retry_s;
        tz.retry_s = tz.retry_s * 2 > RETRY_MAX_S ? RETRY_MAX_S : tz.retry_s * 2;
    }
    tz.next_lookup_ms = millis() + delay_s * 1000;
}

// Applies a lookup's result: keeps it, and saves and sends it if it differs.
static void apply(const TzSchedule *result, Stream *output)
{
    uint32_t interval = user_prefs.fields.rtzinfo.interval;
    uint32_t validity = result->refresh_utc > result->generated_utc
                            ? result->refresh_utc - result->generated_utc
                            : REFRESH_MIN_S;
    bool changed = !tz_schedule_same(result, &tz.schedule);

    tz.refresh_s = validity < interval ? validity : interval;
    if (tz.refresh_s < REFRESH_MIN_S)
    {
        tz.refresh_s = REFRESH_MIN_S;
    }
    tz.schedule = *result;
    tz.updated_utc = result->generated_utc;
    if (changed)
    {
        output->printf("Time zone: %s (%s)\n", result->zone,
                       pic_report_tz_source_name(result->source));
        save();
        tz.send_now = true;
    }
}

static bool update(Stream *output, const Position *position)
{
    TzSchedule result;
    bool ok = lookup(position, &result);

    tz.looked_up = true;
    tz.force = false;
    tz.last_ok = ok;
    tz.used = *position;
    if (ok)
    {
        tz.last_error[0] = '\0';
        apply(&result, output);
    }
    else
    {
        tz.failures++;
        output->printf("Time zone lookup failed: %s\n", tz.last_error);
    }
    schedule_next(ok);
    return ok;
}

bool tzinfo_update(Stream *output)
{
    Position position = current_position();

    return update(output, &position);
}

static void print_schedule(Stream *output, const TzSchedule *schedule)
{
    uint8_t index;

    output->printf("Zone:     %s (%s%s)\n", schedule->zone,
                   pic_report_tz_source_name(schedule->source),
                   schedule->ambiguous ? ", AMBIGUOUS" : "");
    output->printf("Issued:   ");
    time_print_utc(output, schedule->generated_utc);
    output->printf(", refresh by ");
    time_print_utc(output, schedule->refresh_utc);
    output->printf("\n");
    for (index = 0; index < schedule->count; index++)
    {
        const TzEntry *entry = &schedule->entries[index];

        output->printf("%s ", index == 0 ? "From:    " : "         ");
        if (entry->utc_seconds == 0)
        {
            output->printf("%-20s", "(before the data)");
        }
        else
        {
            time_print_utc(output, entry->utc_seconds);
        }
        output->printf("  UTC");
        time_print_offset(output, tz_entry_offset(entry));
        output->printf(" (base ");
        time_print_offset(output, entry->base_offset_s);
        output->printf(", DST ");
        time_print_offset(output, entry->dst_offset_s);
        output->printf(" %s)\n", entry->dst_active ? "on" : "off");
    }
    output->printf("Horizon:  ");
    if (schedule->horizon_end_utc == 0xFFFFFFFFUL)
    {
        output->printf("open\n");
    }
    else
    {
        time_print_utc(output, schedule->horizon_end_utc);
        output->printf("\n");
    }
}

bool tzinfo_check(Stream *output)
{
    Position position = current_position();
    TzSchedule result;

    output->printf("GET %s\n", lookup_url(&position).c_str());
    if (!lookup(&position, &result))
    {
        output->printf("Failed: %s\n", tz.last_error);
        return false;
    }
    print_schedule(output, &result);
    output->printf("(not applied: use \"tz lookup\")\n");
    return true;
}

void tzinfo_set_enabled(bool enabled)
{
    user_prefs.fields.rtzinfo.flags.enabled = enabled;
    if (enabled)
    {
        tz.looked_up = false;
        tz.force = true; // at once: whoever enabled it wants to see it work
        tz.send_now = true;
    }
}

void tzinfo_set_server(const char *server)
{
    user_prefs_set_string(user_prefs.fields.rtzinfo.server, server);
}

void tzinfo_set_path(const char *path)
{
    String value = path;

    if (!value.startsWith("/"))
    {
        value = "/" + value;
    }
    user_prefs_set_string(user_prefs.fields.rtzinfo.path, value.c_str());
}

void tzinfo_init(Stream *output)
{
    memset(&tz, 0, sizeof(tz));
    tz.retry_s = RETRY_FIRST_S;
    tz.empty_retry_s = RETRY_EMPTY_S;
    tz.refresh_s = user_prefs.fields.rtzinfo.interval;
    tz.send_now = true;
    load(output);
    // With nothing cached, the PIC has nothing to show but UTC: GeoIP at
    // once, and GNSS refines it when a fix arrives (has_moved()).
    tz.force = !tz.schedule.valid;
}

static bool lookup_due(void)
{
    uint32_t now = millis();
    Position position;

    if (tz.network_since_ms == 0)
    {
        tz.network_since_ms = now == 0 ? 1 : now;
    }
    position = current_position();
    if (!tz.looked_up)
    {
        // Wait a little for a GNSS fix before falling back to GeoIP, unless
        // there is no receiver, no position is ever sent, or it is wanted now.
        return tz.force || position.have || user_prefs.fields.rtzinfo.gnss_accuracy < 0 ||
               receiver_absent() || now - tz.network_since_ms > FIRST_FIX_WAIT_MS;
    }
    if ((int32_t)(now - tz.next_lookup_ms) >= 0)
    {
        return true;
    }
    // Moved (or a fix arrived after a GeoIP lookup): only after a good lookup,
    // and at most once a minute.
    if (tz.last_ok && has_moved(&position, &tz.used) &&
        now - tz.last_move_check_ms > MOVE_RECHECK_MS)
    {
        tz.last_move_check_ms = now;
        return true;
    }
    return false;
}

static void send_state(void)
{
    SML_STAT_UI_PAYLOAD ui;
    SML_TZ_STATE_PAYLOAD state;
    uint32_t now = millis();
    uint32_t utc;
    bool held_clear;
    bool have_now;

    if (!tz.schedule.valid || !pic_link_is_sml())
    {
        return;
    }
    held_clear = pic_link_get(SML_CLASS_STAT, SML_STAT_UI, &ui, PIC_LINK_STATE_TIMEOUT_MS) &&
                 (ui.flags & SML_UI_TZ_HELD) == 0;
    if (!tz.send_now && !(held_clear && now - tz.last_sent_ms > HELD_RESEND_MS) &&
        now - tz.last_sent_ms < RESEND_MS)
    {
        return;
    }
    have_now = esp_time_utc_now(&utc);
    tz_schedule_state(&tz.schedule, have_now, utc, &state);
    pic_link_send(SML_CLASS_TZ, SML_TZ_STATE, &state);
    tz.send_now = false;
    tz.last_sent_ms = now;
    tz.sent++;
}

void tzinfo_service(bool network_up)
{
    if (!user_prefs.fields.rtzinfo.flags.enabled)
    {
        return;
    }
    if (!network_up)
    {
        tz.network_since_ms = 0;
    }
    else if (lookup_due())
    {
        Position position = current_position();

        update(&Serial, &position);
    }
    send_state();
}

const TzSchedule *tzinfo_schedule(void)
{
    return &tz.schedule;
}

bool tzinfo_last_ok(void)
{
    return tz.last_ok;
}

uint32_t tzinfo_updated_utc(void)
{
    return tz.last_ok ? tz.updated_utc : 0;
}

uint32_t tzinfo_refresh_utc(void)
{
    uint32_t utc;

    if (!esp_time_utc_now(&utc) || !tz.looked_up)
    {
        return 0;
    }
    return utc + (uint32_t)((int32_t)(tz.next_lookup_ms - millis()) / 1000);
}

bool tzinfo_local_now(uint32_t *local_seconds)
{
    SML_STAT_UI_PAYLOAD ui;
    uint32_t utc;
    int32_t offset = 0;

    if (!esp_time_utc_now(&utc))
    {
        return false;
    }
    if (pic_link_get(SML_CLASS_STAT, SML_STAT_UI, &ui, PIC_LINK_STATE_TIMEOUT_MS) &&
        (ui.flags & SML_UI_LOCAL_VALID))
    {
        offset = ui.base_offset_s + ((ui.flags & SML_UI_DST_ACTIVE) ? ui.dst_offset_s : 0);
    }
    else if (tz.schedule.valid)
    {
        offset = tz_entry_offset(tz_schedule_entry_at(&tz.schedule, utc));
    }
    *local_seconds = (uint32_t)((int64_t)utc + offset);
    return true;
}

// One line: the zone, its offset now and its next change.
static void print_brief(Stream *output)
{
    uint32_t utc;
    uint8_t index;

    output->printf("Zone:     ");
    if (!tz.schedule.valid)
    {
        output->printf("NONE (lookups %s)", user_prefs.fields.rtzinfo.flags.enabled ? "ON" : "OFF");
    }
    else
    {
        output->printf("%s (%s%s)", tz.schedule.zone, pic_report_tz_source_name(tz.schedule.source),
                       tz.schedule.ambiguous ? ", AMBIGUOUS" : "");
        if (esp_time_utc_now(&utc))
        {
            output->printf(", UTC");
            time_print_offset(output, tz_entry_offset(tz_schedule_entry_at(&tz.schedule, utc)));
            for (index = 0; index < tz.schedule.count; index++)
            {
                if (tz.schedule.entries[index].utc_seconds > utc)
                {
                    output->printf(", changes ");
                    time_print_utc(output, tz.schedule.entries[index].utc_seconds);
                    break;
                }
            }
        }
    }
    if (tz.last_error[0] != '\0')
    {
        output->printf(", last lookup failed: %s", tz.last_error);
    }
    output->printf("\n");
}

void tzinfo_print(Stream *output, bool verbose)
{
    uint32_t now = millis();

    if (!verbose)
    {
        print_brief(output);
        return;
    }

    output->printf("Enabled:  %s, server %s%s\n",
                   user_prefs.fields.rtzinfo.flags.enabled ? "ON" : "OFF",
                   user_prefs.fields.rtzinfo.server, user_prefs.fields.rtzinfo.path);
    output->printf("Location: ");
    if (user_prefs.fields.rtzinfo.gnss_accuracy < 0)
    {
        output->printf("never sent (GeoIP only)\n");
    }
    else
    {
        output->printf("GNSS to %ld decimal places, else GeoIP\n",
                       (long)user_prefs.fields.rtzinfo.gnss_accuracy);
    }
    if (tz.schedule.valid)
    {
        print_schedule(output, &tz.schedule);
    }
    else
    {
        output->printf("No schedule held\n");
    }
    output->printf("Lookups:  %lu, %lu failed", (unsigned long)tz.lookups,
                   (unsigned long)tz.failures);
    if (tz.looked_up)
    {
        int32_t due = (int32_t)(tz.next_lookup_ms - now) / 1000;

        output->printf(", next in %ld s", (long)(due < 0 ? 0 : due));
    }
    output->printf("\n");
    if (tz.last_error[0] != '\0')
    {
        output->printf("Last error: %s\n", tz.last_error);
    }
    output->printf("TZ-STATE: %lu sent\n", (unsigned long)tz.sent);
}
