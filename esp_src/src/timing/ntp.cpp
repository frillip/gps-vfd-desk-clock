#include "ntp.h"

#include <Network.h>
#include <esp_timer.h>
#include <sys/time.h>

#include "../console/console.h"
#include "../settings/user_prefs.h"
#include "../util/time_text.h"
#include "esp_clock.h"
#include "ntp_client.h"
#include "pps.h"

#define LEARN_FIRST_S     64U
#define POLL_MIN_S        128U // adaptive range, once learnt: from here ...
#define POLL_START_S      256U
#define POLL_QUIET_US     500LL // ... longer after POLL_QUIET_RUN corrections under this
#define POLL_QUIET_RUN    4U
#define POLL_LOUD_US      2000LL // ... shorter after one over this
#define RETRY_FIRST_S     60U
#define SYNC_SLACK_S      300U
#define FREE_RUN_AFTER_S  86400U
#define PPS_SYNC_LIMIT_NS 1000000 // phase within which the edge marks its second

typedef struct
{
    bool polled;             // a poll has been started since the network came up
    int64_t next_poll_us;    // local time of the next poll
    uint32_t max_interval_s; // ntp.interval: the longest poll interval
    uint32_t interval_s;     // the interval in use, adaptive below that
    uint32_t quiet;          // consecutive small corrections
    uint32_t retry_s;        // current retry delay after a failure
    uint32_t learn_step;     // 64, 128, 256 s polls done
    NtpPollResult last;      // the latest poll, good or bad
    bool have_last;
    bool report_ready;
    SML_TIME_NTP_PAYLOAD report;
    uint32_t edges;
    int64_t last_edge_utc_us;
    uint32_t dns_failures;
} Ntp;

static Ntp ntp;

static int64_t now_us(void)
{
    return esp_timer_get_time();
}


void ntp_init(void)
{
    esp_clock_init();
    ntp_client_init();
    ntp.max_interval_s = user_prefs.fields.ntp.interval;
    ntp.interval_s = POLL_START_S < ntp.max_interval_s ? POLL_START_S : ntp.max_interval_s;
    ntp.retry_s = RETRY_FIRST_S;
}


void ntp_resync(void)
{
    ntp.next_poll_us = now_us();
}


bool ntp_is_valid(void)
{
    return esp_clock_is_valid();
}


static uint32_t update_age_s(void)
{
    int64_t age = now_us() - esp_clock_status()->last_update_local_us;

    return age < 0 ? 0 : (uint32_t)(age / 1000000);
}


bool ntp_is_synced(void)
{
    return esp_clock_is_valid() && update_age_s() <= 2U * ntp.max_interval_s + SYNC_SLACK_S;
}


bool ntp_utc_at(int64_t local_us, int64_t *utc_us)
{
    if (!esp_clock_is_valid())
    {
        return false;
    }
    *utc_us = esp_clock_utc_us(local_us);
    return true;
}


bool ntp_utc_now(uint32_t *utc_seconds)
{
    int64_t utc;

    if (!ntp_utc_at(now_us(), &utc))
    {
        return false;
    }
    *utc_seconds = (uint32_t)(utc / 1000000);
    return true;
}


// The system clock follows the model, for anything using time() or TLS.
static void set_system_time(void)
{
    int64_t utc = esp_clock_utc_us(now_us());
    struct timeval value;

    value.tv_sec = (time_t)(utc / 1000000);
    value.tv_usec = (suseconds_t)(utc % 1000000);
    settimeofday(&value, NULL);
}


/*
 * The poll interval adapts between POLL_MIN_S and ntp.interval: it doubles
 * after POLL_QUIET_RUN corrections in a row under POLL_QUIET_US, and halves
 * after one over POLL_LOUD_US. Short polls while the crystal wanders (warm
 * up, temperature) keep each correction small enough to slew gently; long
 * ones once it is steady spare the server.
 */
static void adapt_interval(int64_t error_us)
{
    uint32_t longest = ntp.max_interval_s;
    uint32_t shortest = POLL_MIN_S < longest ? POLL_MIN_S : longest;

    if (error_us < 0)
    {
        error_us = -error_us;
    }
    if (error_us > POLL_LOUD_US)
    {
        ntp.interval_s /= 2;
        ntp.quiet = 0;
    }
    else if (error_us < POLL_QUIET_US && ++ntp.quiet >= POLL_QUIET_RUN)
    {
        ntp.interval_s *= 2;
        ntp.quiet = 0;
    }
    if (ntp.interval_s > longest)
    {
        ntp.interval_s = longest;
    }
    if (ntp.interval_s < shortest)
    {
        ntp.interval_s = shortest;
    }
}


static uint32_t next_delay_s(bool good)
{
    uint32_t delay_s;

    if (!good)
    {
        delay_s = ntp.retry_s;
        ntp.retry_s = ntp.retry_s * 2 > ntp.interval_s ? ntp.interval_s : ntp.retry_s * 2;
        return delay_s;
    }
    ntp.retry_s = RETRY_FIRST_S;
    if (esp_clock_status()->frequency_samples < 2 && ntp.learn_step < 3)
    {
        delay_s = LEARN_FIRST_S << ntp.learn_step++;
        return delay_s < ntp.interval_s ? delay_s : ntp.interval_s;
    }
    return ntp.interval_s;
}


static void schedule_next(bool good)
{
    ntp.next_poll_us = now_us() + (int64_t)next_delay_s(good) * 1000000;
}


static void start_pps(void)
{
    int64_t utc = esp_clock_utc_us(now_us());
    int64_t next_second = (utc / 1000000 + 1) * 1000000;

    pps_output_start(esp_clock_local_us(next_second), esp_clock_period_adjust_q16());
}


static const char *poll_failure(const NtpPollResult *poll)
{
    if (poll->dns_failed)
    {
        return "DNS lookup failed";
    }
    if (poll->last_result == NTP_REPLY_OK)
    {
        return "no reply";
    }
    return ntp_reply_name(poll->last_result);
}


static void handle_poll(const NtpPollResult *poll)
{
    const EspClockStatus *status = esp_clock_status();
    EspClockAction action;

    ntp.last = *poll;
    ntp.have_last = true;
    if (!poll->ok)
    {
        console_printf("NTP: poll of %s failed (%s %s)\n", user_prefs.fields.ntp.server,
                       poll_failure(poll), poll->kiss);
        schedule_next(false);
        return;
    }
    // A gentle correction is spread over about two polls: the next sample
    // replaces what is left of it.
    action = esp_clock_update(&poll->sample, now_us(), (int64_t)ntp.interval_s * 2000000);
    set_system_time();
    if (action == ESP_CLOCK_STEPPED)
    {
        console_printf("NTP: clock set from %s, step %lld us (%u steps)\n", poll->address,
                       (long long)status->last_error_us, (unsigned)status->steps);
        start_pps();
    }
    else if (action == ESP_CLOCK_SLEWED_FAST)
    {
        console_printf("NTP: correcting %lld us at %d ppm\n", (long long)status->slew_us,
                       ESP_CLOCK_FAST_SLEW_PPM);
    }
    if (status->frequency_samples >= 2 && action != ESP_CLOCK_STEPPED)
    {
        adapt_interval(status->last_error_us);
    }
    schedule_next(true);
}


static void build_report(const PpsEdge *edge)
{
    SML_TIME_NTP_PAYLOAD *report = &ntp.report;
    const EspClockStatus *status = esp_clock_status();
    int64_t utc = esp_clock_utc_us(edge->local_us);
    int64_t second = (utc + 500000) / 1000000;
    int64_t phase_ns = (utc - second * 1000000) * 1000;
    uint32_t age_s = update_age_s();
    int64_t age_ms = (now_us() - status->last_update_local_us) / 1000;

    memset(report, 0, sizeof(*report));
    report->flags = SML_NTP_TIME_VALID | SML_NTP_UNCERTAINTY_VALID;
    if (status->updates != 0)
    {
        report->flags |= SML_NTP_UPDATE_VALID;
    }
    if (phase_ns < PPS_SYNC_LIMIT_NS && phase_ns > -PPS_SYNC_LIMIT_NS)
    {
        report->flags |= SML_NTP_PPS_SYNC | SML_NTP_SCHEDULER_SYNC;
    }
    if (esp_clock_correcting(edge->local_us))
    {
        report->flags |= SML_NTP_CORRECTING;
    }
    report->source = ntp_is_synced() ? SML_SOURCE_NTP : SML_SOURCE_ESP;
    if (ntp_is_synced())
    {
        report->lock_state = status->frequency_samples != 0 ? SML_LOCK_LOCKED : SML_LOCK_ACQUIRING;
    }
    else
    {
        report->lock_state = age_s > FREE_RUN_AFTER_S ? SML_LOCK_FREE_RUNNING : SML_LOCK_HOLDOVER;
    }
    report->utc_seconds = (uint32_t)second;
    report->utc_phase_ns = (int32_t)phase_ns;
    report->uncertainty_ns = esp_clock_uncertainty_ns(edge->local_us);
    report->pps_sequence = edge->sequence;
    report->last_update_utc = (uint32_t)(esp_clock_utc_us(status->last_update_local_us) / 1000000);
    report->update_age_ms = age_ms > 65535 ? 65535 : (uint16_t)(age_ms < 0 ? 0 : age_ms);
    report->update_interval_s = ntp.interval_s;
    report->round_trip_us = (uint32_t)status->last.delay_us;
    report->last_offset_us = (int32_t)status->last_error_us;
    // Positive: the ESP's clock runs fast.
    report->frequency_estimate_ppt = -status->frequency_ppb * 1000;
    ntp.report_ready = true;
    ntp.last_edge_utc_us = utc;
}


static void service_pps(void)
{
    PpsEdge edge;

    if (!pps_output_take(&edge) || !esp_clock_is_valid())
    {
        return;
    }
    ntp.edges++;
    build_report(&edge);
    // The next edge: the next whole UTC second after this one.
    pps_output_plan(esp_clock_local_us(((int64_t)ntp.report.utc_seconds + 1) * 1000000),
                    esp_clock_period_adjust_q16());
}


void ntp_service(bool network_up)
{
    NtpPollResult poll;

    if (ntp.max_interval_s != user_prefs.fields.ntp.interval)
    {
        ntp.max_interval_s = user_prefs.fields.ntp.interval;
        if (ntp.interval_s > ntp.max_interval_s)
        {
            ntp.interval_s = ntp.max_interval_s;
        }
    }
    if (ntp_client_take(&poll))
    {
        handle_poll(&poll);
    }
    service_pps();

    if (!network_up)
    {
        ntp.polled = false; // poll as soon as it is back
        return;
    }
    if ((!ntp.polled || now_us() >= ntp.next_poll_us) && !ntp_client_busy())
    {
        IPAddress address;

        ntp.polled = true;
        // Resolved here, in the loop task: see ntp_client.h.
        if (!Network.hostByName(user_prefs.fields.ntp.server, address) || address.type() != IPv4 ||
            (uint32_t)address == 0)
        {
            NtpPollResult failed;

            memset(&failed, 0, sizeof(failed));
            failed.dns_failed = true;
            ntp.dns_failures++;
            handle_poll(&failed);
            return;
        }
        if (ntp_client_poll((uint32_t)address[0] << 24 | (uint32_t)address[1] << 16 |
                            (uint32_t)address[2] << 8 | address[3]))
        {
            ntp.next_poll_us = now_us() + (int64_t)ntp.max_interval_s * 1000000; // until the result
        }
    }
}


bool ntp_take_report(SML_TIME_NTP_PAYLOAD *payload)
{
    if (!ntp.report_ready)
    {
        return false;
    }
    *payload = ntp.report;
    ntp.report_ready = false;
    return true;
}


void ntp_print(Stream *output)
{
    const EspClockStatus *status = esp_clock_status();
    const NtpClientStats *stats = ntp_client_stats();
    const PpsOutputStats *pps = pps_output_stats();
    uint32_t utc;

    output->printf("Server:    %s", user_prefs.fields.ntp.server);
    if (ntp.have_last && ntp.last.address[0] != '\0')
    {
        output->printf(" (%s)", ntp.last.address);
    }
    output->printf("\nInterval:  %lu s (adaptive, at most %lu s)\n", (unsigned long)ntp.interval_s,
                   (unsigned long)ntp.max_interval_s);
    if (!esp_clock_is_valid())
    {
        output->printf("Clock:     not set\n");
    }
    else
    {
        ntp_utc_now(&utc);
        output->printf("Clock:     ");
        time_print_utc(output, utc);
        output->printf(" %s, +/-", ntp_is_synced() ? "synced" : "NOT SYNCED");
        time_print_ns(output, esp_clock_uncertainty_ns(now_us()));
        output->printf("\nUpdated:   ");
        time_print_duration(output, update_age_s());
        output->printf(" ago, stratum %u, round trip ", status->last.stratum);
        time_print_ns(output, status->last.delay_us * 1000);
        output->printf(", correction ");
        time_print_ns(output, status->last_error_us * 1000);
        output->printf("\nFrequency: %+.3f ppm (%lu estimates, latest from %u samples)\n",
                       -(float)status->frequency_ppb / 1000.0f,
                       (unsigned long)status->frequency_samples, status->frequency_used);
        output->printf("Slewing:   ");
        time_print_ns(output, (status->slew_us) * 1000);
        if (now_us() < status->slew_end_us)
        {
            output->printf(" %s, done in ", status->fast ? "fast" : "gently");
            time_print_duration(output, (uint32_t)((status->slew_end_us - now_us()) / 1000000));
        }
        else
        {
            output->printf(", done");
        }
        output->printf(" (%lu fast)\n", (unsigned long)status->fast_slews);
    }
    output->printf("Polls:     %lu (%lu good), %lu exchanges, %lu timeouts, %lu DNS failures, "
                   "%lu updates, %lu steps\n",
                   (unsigned long)stats->polls, (unsigned long)stats->good_polls,
                   (unsigned long)stats->exchanges, (unsigned long)stats->timeouts,
                   (unsigned long)ntp.dns_failures, (unsigned long)status->updates,
                   (unsigned long)status->steps);
    if (ntp.have_last && !ntp.last.ok)
    {
        output->printf("Last poll: failed (%s %s)\n", poll_failure(&ntp.last), ntp.last.kiss);
    }
    output->printf("ESP PPS:   %s, %lu edges, %lu planned, %lu ran on, latency %ld us (max %ld)\n",
                   pps_output_running() ? "running" : "stopped", (unsigned long)ntp.edges,
                   (unsigned long)pps->planned, (unsigned long)pps->extrapolated,
                   (long)pps->latency_us, (long)pps->latency_max_us);
    if (ntp.edges != 0)
    {
        output->printf("Last edge: UTC + ");
        time_print_ns(output, (int64_t)ntp.report.utc_phase_ns);
        output->printf("\n");
    }
}
