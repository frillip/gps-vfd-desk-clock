#include "wifi_link.h"

#include <WiFi.h>
#include <WiFiManager.h>
#include <esp_wifi.h>

extern "C"
{
#include "../pic/sml/sml_proto.h"
}


#define RECONNECT_INTERVAL_MS 30000U
#define PORTAL_TIMEOUT_S      900

static WiFiManager manager;
static char hostname[24];
static uint32_t last_attempt_ms;
static uint32_t reconnects;

void wifi_link_init(Stream *output)
{
    uint64_t mac = ESP.getEfuseMac();
    uint32_t id = 0;
    int index;

    // The last three bytes of the MAC, as on V2.
    for (index = 0; index < 17; index += 8)
    {
        id |= ((mac >> (40 - index)) & 0xFF) << index;
    }
    snprintf(hostname, sizeof(hostname), "SMOL_CLOCK_%06lX", (unsigned long)(id & 0xFFFFFF));
    WiFi.setHostname(hostname);
    output->printf("%s\n", hostname);

    WiFi.mode(WIFI_STA);
    manager.setConfigPortalBlocking(false);
    manager.setConfigPortalTimeout(PORTAL_TIMEOUT_S);
    manager.setConnectTimeout(5);
    if (manager.autoConnect(hostname))
    {
        output->printf("Wi-Fi connected: %s\n", WiFi.localIP().toString().c_str());
    }
    else
    {
        output->printf("Wi-Fi setup access point %s running\n", hostname);
        esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT20);
    }
}


void wifi_link_service(void)
{
    manager.process();
    if (WiFi.status() == WL_CONNECTED || !manager.getWiFiIsSaved() ||
        manager.getConfigPortalActive())
    {
        return;
    }
    if (millis() - last_attempt_ms > RECONNECT_INTERVAL_MS)
    {
        last_attempt_ms = millis();
        if (reconnects++ == 0)
        {
            Serial.println("Reconnecting to Wi-Fi...");
        }
        WiFi.disconnect();
        WiFi.reconnect();
    }
}


bool wifi_link_is_up(void)
{
    return WiFi.status() == WL_CONNECTED;
}


uint8_t wifi_link_state(void)
{
    if (manager.getConfigPortalActive())
    {
        return SML_WIFI_PROVISIONING;
    }
    if (WiFi.status() == WL_CONNECTED)
    {
        return SML_WIFI_CONNECTED;
    }
    if (!manager.getWiFiIsSaved())
    {
        return SML_WIFI_NOT_CONFIGURED;
    }
    return WiFi.status() == WL_IDLE_STATUS ? SML_WIFI_CONNECTING : SML_WIFI_DISCONNECTED;
}


const char *wifi_link_hostname(void)
{
    return hostname;
}


void wifi_link_forget(void)
{
    manager.resetSettings();
}


void wifi_link_clear(Stream *output)
{
    output->printf("Clearing the Wi-Fi settings and restarting\n");
    wifi_link_forget();
    delay(100);
    ESP.restart();
}


void wifi_link_setup(Stream *output)
{
    if (manager.getConfigPortalActive())
    {
        output->printf("Wi-Fi setup access point already running\n");
        return;
    }
    output->printf("Wi-Fi setup access point %s starting\n", hostname);
    manager.startConfigPortal(hostname);
    esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT20);
}


void wifi_link_reconnect(void)
{
    WiFi.reconnect();
}


void wifi_link_disconnect(void)
{
    WiFi.disconnect();
}


void wifi_link_print(Stream *output)
{
    bool up = wifi_link_is_up();

    output->printf("Wi-Fi:  %s\n", up ? "CONNECTED" : "DISCONNECTED");
    output->printf("SSID:   %s\n",
                   manager.getWiFiIsSaved() ? manager.getWiFiSSID(true).c_str() : "(none)");
    if (up)
    {
        output->printf("IP:     %s, gateway %s, DNS %s\n", WiFi.localIP().toString().c_str(),
                       WiFi.gatewayIP().toString().c_str(), WiFi.dnsIP().toString().c_str());
        output->printf("Signal: %d dBm, channel %d\n", (int)WiFi.RSSI(), (int)WiFi.channel());
    }
    if (manager.getConfigPortalActive())
    {
        output->printf("Setup access point %s active\n", hostname);
    }
    output->printf("MAC:    %s\n", WiFi.macAddress().c_str());
}


static const char *auth_name(wifi_auth_mode_t mode)
{
    switch (mode)
    {
        case WIFI_AUTH_OPEN: return "open";
        case WIFI_AUTH_WEP: return "WEP";
        case WIFI_AUTH_WPA_PSK: return "WPA-PSK";
        case WIFI_AUTH_WPA2_PSK:
        case WIFI_AUTH_WPA_WPA2_PSK: return "WPA2-PSK";
        case WIFI_AUTH_WPA3_PSK:
        case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA3-PSK";
        case WIFI_AUTH_ENTERPRISE: return "WPA-EAP";
        default: return "other";
    }
}


void wifi_link_scan(Stream *output)
{
    String current = WiFi.SSID();
    int count;
    int index;

    output->printf("Scanning...\n");
    count = WiFi.scanNetworks();
    if (count <= 0)
    {
        output->printf("No networks found\n");
        return;
    }
    for (index = 0; index < count; index++)
    {
        bool connected = wifi_link_is_up() && WiFi.SSID(index) == current;

        output->printf(" %-32s %4d dBm  %-8s%s\n", WiFi.SSID(index).c_str(), (int)WiFi.RSSI(index),
                       auth_name(WiFi.encryptionType(index)), connected ? "  (connected)" : "");
    }
    WiFi.scanDelete();
}
