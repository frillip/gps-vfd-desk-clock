#include "updater.h"

#include <ESP32OTAPull.h>

#include "../console/console.h"
#include "../settings/user_prefs.h"
#include "../version.h"
#include "root_ca.h"

static ESP32OTAPull ota;
static Stream *progress_output;
static bool available;
static bool running;
static bool day_known;
static uint32_t checked_day; // local day number of the last automatic check

static String url(void)
{
    return String("https://") + user_prefs.fields.updater.server + user_prefs.fields.updater.path;
}


void updater_set_server(const char *server)
{
    user_prefs_set_string(user_prefs.fields.updater.server, server);
}


void updater_set_path(const char *path)
{
    String value = path;

    if (!value.startsWith("/"))
    {
        value = "/" + value;
    }
    user_prefs_set_string(user_prefs.fields.updater.path, value.c_str());
}


void updater_set_config(const char *config)
{
    user_prefs_set_string(user_prefs.fields.updater.config,
                          strcmp(config, "default") == 0 ? UPDATER_CONFIG_DEFAULT : config);
}


static void print_version(Stream *output)
{
    output->printf("ESP version: %s", ESP_VERSION);
    if (user_prefs.fields.updater.config[0] != '\0')
    {
        output->printf("_%s", user_prefs.fields.updater.config);
    }
    output->printf("\n");
}


static void on_progress(int offset, int total)
{
    static int previous = -1;
    int percent = total > 0 ? (int)(100LL * offset / total) : 0;

    if (percent != previous && progress_output != NULL)
    {
        progress_output->printf("Updating: %d of %d bytes (%d%%)\n", offset, total, percent);
        previous = percent;
    }
}


static const char *result_name(int code)
{
    switch (code)
    {
        case ESP32OTAPull::UPDATE_AVAILABLE: return "update available, not installed";
        case ESP32OTAPull::NO_UPDATE_PROFILE_FOUND: return "no profile matches";
        case ESP32OTAPull::NO_UPDATE_AVAILABLE: return "no update for this version";
        case ESP32OTAPull::UPDATE_OK: return "update installed, restart to use it";
        case ESP32OTAPull::HTTP_FAILED: return "HTTP request failed";
        case ESP32OTAPull::WRITE_ERROR: return "write error";
        case ESP32OTAPull::JSON_PROBLEM: return "invalid JSON";
        case ESP32OTAPull::OTA_UPDATE_FAIL: return "update failed (no OTA partition?)";
        default: return code > 0 ? "unexpected HTTP status" : "unknown error";
    }
}


// One request; action as ESP32OTAPull. Prints the outcome.
static int request(Stream *output, ESP32OTAPull::ActionType action, bool any_version)
{
    int result;

    running = action != ESP32OTAPull::DONT_DO_UPDATE;
    progress_output = output;
    ota.SetRootCA(root_ca_pem);
    ota.SetCallback(on_progress);
    ota.SetConfig(user_prefs.fields.updater.config);
    ota.AllowDowngrades(any_version);
    result = ota.CheckForOTAUpdate(url().c_str(), any_version ? "0.0.0" : ESP_VERSION, action);
    running = false;
    available = result == ESP32OTAPull::UPDATE_AVAILABLE;
    output->printf("Update server: %d (%s)\n", result, result_name(result));
    return result;
}


bool updater_check(Stream *output)
{
    print_version(output);
    request(output, ESP32OTAPull::DONT_DO_UPDATE, false);
    if (available)
    {
        output->printf("Available: %s\n", ota.GetVersion().c_str());
    }
    return available;
}


void updater_pull(Stream *output)
{
    print_version(output);
    if (request(output, ESP32OTAPull::UPDATE_BUT_NO_BOOT, false) == ESP32OTAPull::UPDATE_OK)
    {
        output->printf("Use esp-reset to finish the update\n");
    }
}


void updater_force(Stream *output)
{
    print_version(output);
    output->printf("Installing whatever the server offers, then restarting\n");
    request(output, ESP32OTAPull::UPDATE_AND_BOOT, true);
}


void updater_service(bool have_local, uint32_t local_seconds)
{
    uint32_t day = local_seconds / 86400;
    uint32_t hour = local_seconds / 3600 % 24;
    Stream *output = &Serial;

    if (!have_local)
    {
        return;
    }
    if (!day_known)
    {
        // Started inside the update hour: skip today, it may be the update.
        day_known = true;
        checked_day = hour == user_prefs.fields.updater.flags.auto_time ? day : day - 1;
    }
    if (!user_prefs.fields.updater.flags.auto_enabled || checked_day == day ||
        hour != user_prefs.fields.updater.flags.auto_time)
    {
        return;
    }
    checked_day = day;
    console_printf("Automatic update check\n");
    if (request(output, ESP32OTAPull::DONT_DO_UPDATE, false) == ESP32OTAPull::UPDATE_AVAILABLE)
    {
        request(output, ESP32OTAPull::UPDATE_AND_BOOT, false);
    }
}


bool updater_is_available(void)
{
    return available;
}


bool updater_is_running(void)
{
    return running;
}
