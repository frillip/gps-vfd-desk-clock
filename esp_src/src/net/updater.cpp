#include "updater.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WiFi.h>
#include <ctype.h>
#include <string.h>

#include "../console/console.h"
#include "../settings/user_prefs.h"
#include "../version.h"
#include "net_connect.h"
#include "root_ca.h"

// What a check or an update came to: ESP32-OTA-Pull's codes, which this
// replaces, so they read the same; above 0, the HTTP status.
enum
{
    RESULT_UPDATE_AVAILABLE = -3,
    RESULT_NO_PROFILE = -2,
    RESULT_NO_UPDATE = -1,
    RESULT_UPDATE_OK = 0,
    RESULT_HTTP_FAILED = 1,
    RESULT_WRITE_ERROR = 2,
    RESULT_JSON_PROBLEM = 3,
    RESULT_OTA_FAILED = 4,
};

typedef enum
{
    ACTION_CHECK = 0,   // say whether there is one
    ACTION_INSTALL,     // install it, restart later
    ACTION_INSTALL_BOOT // install it and restart
} Action;

#define HTTP_TIMEOUT_MS       15000U
#define MANIFEST_MAX_BYTES    (16 * 1024)
#define DOWNLOAD_IDLE_MS      30000U
#define DOWNLOAD_BUFFER_BYTES 1024U

static Stream *progress_output;
static bool available;
static bool running;
static bool day_known;
static uint32_t checked_day; // local day number of the last automatic check
static String offered_version;
static char last_address[48]; // the server the latest request connected to
static char last_failure[48]; // why it failed, if it did (RESULT_HTTP_FAILED)

static String url(void)
{
    return String("https://") + net_url_host(user_prefs.fields.updater.server) +
           user_prefs.fields.updater.path;
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
        case RESULT_UPDATE_AVAILABLE: return "update available, not installed";
        case RESULT_NO_PROFILE: return "no profile matches";
        case RESULT_NO_UPDATE: return "no update for this version";
        case RESULT_UPDATE_OK: return "update installed, restart to use it";
        case RESULT_HTTP_FAILED: return "HTTP request failed";
        case RESULT_WRITE_ERROR: return "write error";
        case RESULT_JSON_PROBLEM: return "invalid JSON";
        case RESULT_OTA_FAILED: return "update failed (no OTA partition?)";
        default: return code > 0 ? "unexpected HTTP status" : "unknown error";
    }
}


// ---- Versions: dot-separated numbers, 1.10.0 above 1.9.0 ----------------

static bool numeric_version(const char *version)
{
    bool expecting_digit = true;
    const char *p;

    if (version == NULL || *version == '\0')
    {
        return false;
    }
    for (p = version; *p != '\0'; p++)
    {
        if (*p == '.')
        {
            if (expecting_digit)
            {
                return false;
            }
            expecting_digit = true;
        }
        else if (isdigit((unsigned char)*p))
        {
            expecting_digit = false;
        }
        else
        {
            return false;
        }
    }
    return !expecting_digit;
}


// a against b: 1, 0 or -1. False unless both are numeric versions.
static bool compare_versions(const char *a, const char *b, int *result)
{
    if (!numeric_version(a) || !numeric_version(b))
    {
        return false;
    }
    while (*a != '\0' || *b != '\0')
    {
        const char *a_end = a + strcspn(a, ".");
        const char *b_end = b + strcspn(b, ".");
        size_t a_length;
        size_t b_length;
        int order;

        while (a < a_end && *a == '0')
        {
            a++;
        }
        while (b < b_end && *b == '0')
        {
            b++;
        }
        a_length = (size_t)(a_end - a);
        b_length = (size_t)(b_end - b);
        if (a_length != b_length)
        {
            *result = a_length > b_length ? 1 : -1;
            return true;
        }
        order = strncmp(a, b, a_length);
        if (order != 0)
        {
            *result = order > 0 ? 1 : -1;
            return true;
        }
        a = *a_end == '.' ? a_end + 1 : a_end;
        b = *b_end == '.' ? b_end + 1 : b_end;
    }
    *result = 0;
    return true;
}


// ---- Requests --------------------------------------------------------------

// A GET of url on a connection made with fallback (net_connect.h): the
// status, or 0 if no connection.
static int get(HTTPClient &http, NetworkClientSecure &secure, NetworkClient &plain, const char *url)
{
    IPAddress address;

    http.setConnectTimeout(HTTP_TIMEOUT_MS);
    http.setTimeout(HTTP_TIMEOUT_MS);
    int status;

    if (!net_http_connect(secure, plain, url, root_ca_pem, HTTP_TIMEOUT_MS, &address))
    {
        snprintf(last_failure, sizeof(last_failure), "no connection");
        return 0;
    }
    snprintf(last_address, sizeof(last_address), "%s", address.toString().c_str());
    if (!(strncmp(url, "https://", 8) == 0 ? http.begin(secure, url) : http.begin(plain, url)))
    {
        snprintf(last_failure, sizeof(last_failure), "bad URL");
        return 0;
    }
    status = http.GET();
    if (status < 0)
    {
        snprintf(last_failure, sizeof(last_failure), "%s",
                 HTTPClient::errorToString(status).c_str());
    }
    return status;
}


// Downloads the image at url and writes it to the other OTA partition.
static int install(const char *url, Action action)
{
    NetworkClientSecure secure;
    NetworkClient plain;
    HTTPClient http;
    NetworkClient *stream;
    uint8_t buffer[DOWNLOAD_BUFFER_BYTES];
    int total;
    int offset = 0;
    int status = get(http, secure, plain, url);
    uint32_t last_progress_ms;

    if (status != HTTP_CODE_OK)
    {
        http.end();
        return status > 0 ? status : RESULT_HTTP_FAILED;
    }
    total = http.getSize();
    if (total <= 0)
    {
        http.end();
        snprintf(last_failure, sizeof(last_failure), "no Content-Length");
        return RESULT_HTTP_FAILED;
    }
    if (!Update.begin((size_t)total))
    {
        http.end();
        return RESULT_OTA_FAILED;
    }
    stream = http.getStreamPtr();
    if (stream == NULL)
    {
        Update.abort();
        http.end();
        return RESULT_WRITE_ERROR;
    }
    last_progress_ms = millis();
    while (offset < total)
    {
        size_t waiting;
        size_t wanted;
        size_t got;

        if (!http.connected() && stream->available() == 0)
        {
            break;
        }
        waiting = (size_t)stream->available();
        if (waiting == 0)
        {
            if (millis() - last_progress_ms >= DOWNLOAD_IDLE_MS)
            {
                break;
            }
            delay(1);
            continue;
        }
        wanted = waiting < sizeof(buffer) ? waiting : sizeof(buffer);
        if (wanted > (size_t)(total - offset))
        {
            wanted = (size_t)(total - offset);
        }
        got = stream->readBytes(buffer, wanted);
        if (got == 0 || Update.write(buffer, got) != got)
        {
            break;
        }
        offset += (int)got;
        last_progress_ms = millis();
        on_progress(offset, total);
    }
    http.end();
    if (offset != total)
    {
        Update.abort();
        return RESULT_WRITE_ERROR;
    }
    if (!Update.end())
    {
        Update.abort();
        return RESULT_OTA_FAILED;
    }
    if (action == ACTION_INSTALL_BOOT)
    {
        delay(1000);
        ESP.restart();
    }
    return RESULT_UPDATE_OK;
}


/* The manifest at url: {"Configurations": [{"Board", "Device", "Config",
 * "Version", "URL"}, ...]}, as ESP32-OTA-Pull reads it. The first profile
 * for this board, device and config with a later version (or, with
 * any_version, any other) is the update. */
static int check_manifest(Action action, bool any_version)
{
    NetworkClientSecure secure;
    NetworkClient plain;
    HTTPClient http;
    JsonDocument doc;
    DeserializationError error;
    String manifest_url = url();
    const char *current = any_version ? "0.0.0" : ESP_VERSION;
    String board = ARDUINO_BOARD;
    String device = WiFi.macAddress();
    String config = user_prefs.fields.updater.config;
    bool found_profile = false;
    int status = get(http, secure, plain, manifest_url.c_str());
    int length;

    if (status != HTTP_CODE_OK)
    {
        http.end();
        return status > 0 ? status : RESULT_HTTP_FAILED;
    }
    length = http.getSize();
    if (length <= 0 || length > MANIFEST_MAX_BYTES)
    {
        http.end();
        return RESULT_JSON_PROBLEM;
    }
    error = deserializeJson(doc, http.getStream());
    http.end();
    if (error || !doc.is<JsonObject>() || !doc["Configurations"].is<JsonArray>())
    {
        return RESULT_JSON_PROBLEM;
    }
    JsonArray profiles = doc["Configurations"].as<JsonArray>();
    if (profiles.size() == 0)
    {
        return RESULT_NO_PROFILE;
    }
    // Every profile checked before any is used.
    for (JsonObject profile : profiles)
    {
        const char *image;

        if (profile.isNull() || !profile["URL"].is<const char *>() ||
            !profile["Version"].is<const char *>() ||
            (!profile["Board"].isNull() && !profile["Board"].is<const char *>()) ||
            (!profile["Device"].isNull() && !profile["Device"].is<const char *>()) ||
            (!profile["Config"].isNull() && !profile["Config"].is<const char *>()))
        {
            return RESULT_JSON_PROBLEM;
        }
        image = profile["URL"].as<const char *>();
        if (image[0] == '\0' || !numeric_version(profile["Version"].as<const char *>()) ||
            (strncmp(image, "http://", 7) != 0 && strncmp(image, "https://", 8) != 0))
        {
            return RESULT_JSON_PROBLEM;
        }
    }
    for (JsonObject profile : profiles)
    {
        String p_board = profile["Board"].isNull() ? "" : profile["Board"].as<const char *>();
        String p_device = profile["Device"].isNull() ? "" : profile["Device"].as<const char *>();
        String p_config = profile["Config"].isNull() ? "" : profile["Config"].as<const char *>();
        const char *version = profile["Version"].as<const char *>();
        int order = 0;

        if ((!p_board.isEmpty() && p_board != board) ||
            (!p_device.isEmpty() && p_device != device) ||
            (!p_config.isEmpty() && p_config != config))
        {
            continue;
        }
        if (!compare_versions(version, current, &order))
        {
            return RESULT_JSON_PROBLEM;
        }
        if (order > 0 || (any_version && order != 0))
        {
            offered_version = version;
            return action == ACTION_CHECK ? RESULT_UPDATE_AVAILABLE
                                          : install(profile["URL"].as<const char *>(), action);
        }
        found_profile = true;
    }
    return found_profile ? RESULT_NO_UPDATE : RESULT_NO_PROFILE;
}


// One request; prints the outcome.
static int request(Stream *output, Action action, bool any_version)
{
    int result;

    running = action != ACTION_CHECK;
    progress_output = output;
    last_address[0] = '\0';
    last_failure[0] = '\0';
    result = check_manifest(action, any_version);
    running = false;
    available = result == RESULT_UPDATE_AVAILABLE;
    output->printf("Update server: %d (%s", result, result_name(result));
    if (result == RESULT_HTTP_FAILED && last_failure[0] != '\0')
    {
        output->printf(": %s", last_failure);
    }
    output->printf(")");
    if (last_address[0] != '\0')
    {
        output->printf(", %s", last_address);
    }
    output->printf("\n");
    return result;
}

bool updater_check(Stream *output)
{
    print_version(output);
    request(output, ACTION_CHECK, false);
    if (available)
    {
        output->printf("Available: %s\n", offered_version.c_str());
    }
    return available;
}

void updater_pull(Stream *output)
{
    print_version(output);
    if (request(output, ACTION_INSTALL, false) == RESULT_UPDATE_OK)
    {
        output->printf("Use \"esp reset\" to finish the update\n");
    }
}

void updater_force(Stream *output)
{
    print_version(output);
    output->printf("Installing whatever the server offers, then restarting\n");
    request(output, ACTION_INSTALL_BOOT, true);
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
    if (request(output, ACTION_CHECK, false) == RESULT_UPDATE_AVAILABLE)
    {
        request(output, ACTION_INSTALL_BOOT, false);
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
