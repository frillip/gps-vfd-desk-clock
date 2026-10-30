#include "net_status.h"

#include <WiFi.h>
#include <esp_timer.h>
#include <string.h>

#include "../pic/pic_link.h"
#include "../settings/user_prefs.h"
#include "../timing/ntp.h"
#include "../tz/tzinfo.h"
#include "../version.h"
#include "telnet.h"
#include "updater.h"
#include "wifi_link.h"

#define PERIOD_MS 10000U

static SML_STAT_NET_PAYLOAD last;
static uint32_t last_sent_ms;
static bool sent_once;

static void build(SML_STAT_NET_PAYLOAD *net)
{
    uint16_t flags = 0;

    memset(net, 0, sizeof(*net));
    net->wifi_state = wifi_link_state();
    if (wifi_link_is_up())
    {
        IPAddress ip = WiFi.localIP();

        flags |= SML_NET_WIFI_CONNECTED | SML_NET_IP_VALID | SML_NET_RSSI_VALID;
        net->ipv4 = (uint32_t)ip[0] << 24 | (uint32_t)ip[1] << 16 | (uint32_t)ip[2] << 8 | ip[3];
        net->rssi_dbm = (int8_t)WiFi.RSSI();
    }
    flags |= SML_NET_NTP_ENABLED;
    if (ntp_is_synced())
    {
        flags |= SML_NET_NTP_SYNCED;
    }
    if (user_prefs.fields.rtzinfo.flags.enabled)
    {
        flags |= SML_NET_TZINFO_ENABLED;
        net->tzinfo_refresh_utc = tzinfo_refresh_utc();
    }
    if (tzinfo_last_ok())
    {
        flags |= SML_NET_TZINFO_VALID;
        net->tzinfo_updated_utc = tzinfo_updated_utc();
    }
    if (user_prefs.fields.telnet.flags.enabled)
    {
        flags |= SML_NET_TELNET_ENABLED;
    }
    if (telnet_client() != NULL)
    {
        flags |= SML_NET_TELNET_CLIENT;
    }
    if (user_prefs.fields.updater.flags.auto_enabled)
    {
        flags |= SML_NET_UPDATE_AUTO;
    }
    if (updater_is_available())
    {
        flags |= SML_NET_UPDATE_AVAILABLE;
    }
    if (updater_is_running())
    {
        flags |= SML_NET_UPDATE_IN_PROGRESS;
    }
    net->flags = flags;
    net->esp_version = ESP_VERSION_NUMBER;
}


static bool differs(const SML_STAT_NET_PAYLOAD *a, const SML_STAT_NET_PAYLOAD *b)
{
    // Uptime always moves, the signal often by a dB: neither is a change.
    return a->flags != b->flags || a->wifi_state != b->wifi_state || a->ipv4 != b->ipv4 ||
           a->tzinfo_updated_utc != b->tzinfo_updated_utc || a->esp_version != b->esp_version;
}


void net_status_service(void)
{
    SML_STAT_NET_PAYLOAD net;

    build(&net);
    if (sent_once && !differs(&net, &last) && millis() - last_sent_ms < PERIOD_MS)
    {
        return;
    }
    net.uptime_s = (uint32_t)(esp_timer_get_time() / 1000000);
    pic_link_send(SML_CLASS_STAT, SML_STAT_NET, &net);
    last = net;
    last_sent_ms = millis();
    sent_once = true;
}
