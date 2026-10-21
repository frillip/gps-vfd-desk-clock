#include "commands.h"

#include <ctype.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <string.h>

extern "C"
{
#include "../common/enums.h"
}


#include "../net/telnet.h"
#include "../net/updater.h"
#include "../net/wifi_link.h"
#include "../pic/pic_link.h"
#include "../pic/pic_report.h"
#include "../settings/user_prefs.h"
#include "../timing/ntp.h"
#include "../tz/leap.h"
#include "../tz/tzinfo.h"
#include "../util/parse.h"
#include "../util/time_text.h"
#include "../version.h"

typedef enum
{
    ARG_NONE,
    ARG_UNSIGNED, // [n], min..max
    ARG_SIGNED,   // [n], min..max, may be negative
    ARG_BOOL,     // [b]: 1/0, on/off, true/false, yes/no
    ARG_TEXT,     // [s]
    ARG_DEBUG,    // [s]: SML_DEBUG_* names or a mask; may be empty
    ARG_VERBOSE,  // [-v]: -v or verbose for everything; empty for the verbose setting
} ArgKind;

typedef struct
{
    const char *name;
    USER_CMD command;
    uint16_t pic_command; // SML_USER_*: sent to the PIC; 0 for ESP commands
    ArgKind argument;
    int64_t minimum;
    int64_t maximum;
    const char *help;
} Command;

// name, USER_CMD, [SML code,] argument kind, minimum, maximum, help
#define CMD_ESP(name, cmd, ...)   \
    {                             \
        name, cmd, 0, __VA_ARGS__ \
    }
#define CMD_PIC(name, cmd, code, ...) \
    {                                 \
        name, cmd, code, __VA_ARGS__  \
    }
#define NONE    ARG_NONE, 0, 0
#define TEXT    ARG_TEXT, 0, 0
#define FLAG    ARG_BOOL, 0, 1
#define VERBOSE ARG_VERBOSE, 0, 0

// Information is printed briefly, as chronyc does, unless asked for with -v
// or the verbose setting is on. Not saved: off at restart.
static bool verbose_setting;

static const Command commands[] = {
    CMD_ESP("esp-reset", USER_CMD_ESP_RESET, NONE, "Restart the ESP"),
    CMD_ESP("esp-ntp-info", USER_CMD_ESP_NTP_INFO, VERBOSE, "NTP clock, server and ESP PPS"),
    CMD_ESP("esp-ntp-set-interval", USER_CMD_ESP_NTP_SET_INTERVAL, ARG_UNSIGNED, NTP_INTERVAL_MIN,
            NTP_INTERVAL_MAX, "Longest NTP poll interval, seconds"),
    CMD_ESP("esp-ntp-set-server", USER_CMD_ESP_NTP_SET_SERVER, TEXT, "NTP server, then poll it"),
    CMD_ESP("esp-ntp-resync", USER_CMD_ESP_NTP_RESYNC, NONE, "Poll the NTP server now"),
    CMD_ESP("esp-wifi-info", USER_CMD_ESP_WIFI_INFO, VERBOSE, "Wi-Fi state (never the passphrase)"),
    CMD_ESP("esp-wifi-scan", USER_CMD_ESP_WIFI_SCAN, NONE, "Scan for Wi-Fi networks"),
    CMD_ESP("esp-wifi-connect", USER_CMD_ESP_WIFI_CONNECT, NONE, "Reconnect to Wi-Fi"),
    CMD_ESP("esp-wifi-disconnect", USER_CMD_ESP_WIFI_DISCONNECT, NONE, "Disconnect from Wi-Fi"),
    CMD_ESP("esp-wifi-clear", USER_CMD_ESP_WIFI_CLEAR, NONE,
            "Forget the Wi-Fi settings and restart"),
    CMD_ESP("esp-wifi-setup", USER_CMD_ESP_WIFI_SETUP, NONE, "Open the Wi-Fi setup access point"),
    CMD_ESP("esp-telnet-enable", USER_CMD_ESP_TELNET_ENABLE, NONE, "Enable telnet (and start it)"),
    CMD_ESP("esp-telnet-disable", USER_CMD_ESP_TELNET_DISABLE, NONE, "Disable telnet at restart"),
    CMD_ESP("esp-telnet-start", USER_CMD_ESP_TELNET_START, NONE, "Start telnet now"),
    CMD_ESP("esp-telnet-stop", USER_CMD_ESP_TELNET_STOP, NONE, "Stop telnet now"),
    CMD_ESP("esp-telnet-port", USER_CMD_ESP_TELNET_PORT, ARG_UNSIGNED, 1, 65535, "Telnet port"),
    CMD_ESP("esp-update-check", USER_CMD_ESP_UPDATE_CHECK, NONE, "Is there an OTA update?"),
    CMD_ESP("esp-update-pull", USER_CMD_ESP_UPDATE_PULL, NONE,
            "Install an update; esp-reset after"),
    CMD_ESP("esp-update-force", USER_CMD_ESP_UPDATE_FORCE, NONE,
            "Install what the server offers, any version, and restart"),
    CMD_ESP("esp-update-set-server", USER_CMD_ESP_UPDATE_SET_SERVER, TEXT, "Update server"),
    CMD_ESP("esp-update-set-path", USER_CMD_ESP_UPDATE_SET_PATH, TEXT, "Update path"),
    CMD_ESP("esp-update-set-config", USER_CMD_ESP_UPDATE_SET_CONFIG, TEXT,
            "Update config string ('default' for none)"),
    CMD_ESP("esp-update-auto-enable", USER_CMD_ESP_UPDATE_AUTO_ENABLE, NONE,
            "Daily automatic updates"),
    CMD_ESP("esp-update-auto-disable", USER_CMD_ESP_UPDATE_AUTO_DISABLE, NONE,
            "No automatic updates"),
    CMD_ESP("esp-update-auto-hour", USER_CMD_ESP_UPDATE_AUTO_HOUR, ARG_UNSIGNED, 0,
            UPDATER_AUTO_HOUR_MAX, "Local hour of the automatic update"),
    CMD_ESP("esp-tzinfo-enable", USER_CMD_ESP_TZINFO_ENABLE, NONE,
            "Time zone from the tzinfo server"),
    CMD_ESP("esp-tzinfo-disable", USER_CMD_ESP_TZINFO_DISABLE, NONE, "No remote time zone lookups"),
    CMD_ESP("esp-tzinfo-check", USER_CMD_ESP_TZINFO_CHECK, NONE,
            "Look up the time zone, don't use it"),
    CMD_ESP("esp-tzinfo-update", USER_CMD_ESP_TZINFO_UPDATE, NONE,
            "Look up the time zone and use it"),
    CMD_ESP("esp-tzinfo-set-server", USER_CMD_ESP_TZINFO_SET_SERVER, TEXT, "tzinfo server"),
    CMD_ESP("esp-tzinfo-set-path", USER_CMD_ESP_TZINFO_SET_PATH, TEXT, "tzinfo path"),
    CMD_ESP("esp-tzinfo-set-interval", USER_CMD_ESP_TZINFO_SET_INTERVAL, ARG_UNSIGNED,
            TZINFO_INTERVAL_MIN, TZINFO_INTERVAL_MAX, "tzinfo lookup interval, seconds"),
    CMD_ESP("esp-tzinfo-set-acc", USER_CMD_ESP_TZINFO_SET_ACC, ARG_SIGNED, TZINFO_GNSS_ACCURACY_MIN,
            TZINFO_GNSS_ACCURACY_MAX, "Decimal places of position sent; -1: none (GeoIP)"),
    CMD_ESP("esp-config-show", USER_CMD_ESP_CONFIG_SHOW, NONE, "ESP settings"),
    CMD_ESP("esp-config-clear", USER_CMD_ESP_CONFIG_CLEAR, NONE,
            "ESP settings to defaults (not saved)"),
    CMD_ESP("esp-clear-all", USER_CMD_ESP_CLEAR_ALL, NONE,
            "ESP settings and Wi-Fi to defaults, restart"),
    CMD_ESP("esp-save", USER_CMD_ESP_SAVE, NONE, "Save the ESP settings"),

    CMD_PIC("pic-info", USER_CMD_PIC_INFO, SML_USER_INFO, VERBOSE,
            "The PIC's diagnostics, and its state"),
    CMD_PIC("pic-resync", USER_CMD_PIC_RESYNC, SML_USER_RESYNC, NONE, "Resynchronise the PIC"),
    CMD_PIC("pic-reset", USER_CMD_PIC_RESET, SML_USER_RESET, NONE, "Reset the PIC"),
    CMD_PIC("pic-set-rtc", USER_CMD_PIC_SET_RTC, SML_USER_SET_RTC, ARG_UNSIGNED, 0, UINT32_MAX,
            "Set the RTC, UTC seconds"),
    CMD_PIC("pic-set-tz-auto", USER_CMD_PIC_SET_TZ_AUTO, SML_USER_SET_TZ_AUTO, FLAG,
            "Base offset from the ESP's time zone"),
    CMD_PIC("pic-set-tz-offset", USER_CMD_PIC_SET_TZ_OFFSET, SML_USER_SET_TZ_OFFSET, ARG_SIGNED,
            -86399, 86399, "Manual base offset, seconds"),
    CMD_PIC("pic-set-dst-auto", USER_CMD_PIC_SET_DST_AUTO, SML_USER_SET_DST_AUTO, FLAG,
            "DST from the ESP's time zone"),
    CMD_PIC("pic-set-dst-offset", USER_CMD_PIC_SET_DST_OFFSET, SML_USER_SET_DST_OFFSET, ARG_SIGNED,
            -86399, 86399, "Manual DST amount, seconds (may be negative)"),
    CMD_PIC("pic-set-dst-active", USER_CMD_PIC_SET_DST_ACTIVE, SML_USER_SET_DST_ACTIVE, FLAG,
            "Manual DST on or off (with DST auto off)"),
    CMD_PIC("pic-set-fosc-freq", USER_CMD_PIC_SET_FOSC_FREQ, SML_USER_SET_FOSC, ARG_UNSIGNED, 1,
            UINT32_MAX, "Oscillator baseline, Hz"),
    CMD_PIC("pic-set-alarm-enabled", USER_CMD_PIC_SET_ALARM_ENABLED, SML_USER_SET_ALARM_ENABLE,
            FLAG, "Alarm on or off"),
    CMD_PIC("pic-set-alarm", USER_CMD_PIC_SET_ALARM, SML_USER_SET_ALARM, ARG_UNSIGNED, 0, 86399,
            "Alarm, seconds after local midnight"),
    CMD_PIC("pic-set-delta", USER_CMD_PIC_SET_DELTA, SML_USER_SET_DELTA, ARG_UNSIGNED, 0,
            UINT32_MAX, "Delta display epoch, UTC seconds"),
    CMD_PIC("pic-set-beeps", USER_CMD_PIC_SET_BEEPS, SML_USER_SET_BEEP, FLAG, "Beeps on or off"),
    CMD_PIC("pic-set-display", USER_CMD_PIC_SET_DISPLAY, SML_USER_SET_DISPLAY, ARG_UNSIGNED, 1, 6,
            "1 HHMM, 2 MMSS, 3 SSMM, 4 YYYY, 5 MMDD, 6 delta"),
    CMD_PIC("pic-set-brightness-auto", USER_CMD_PIC_SET_BRIGHTNESS_AUTO, SML_USER_SET_BRI_AUTO,
            NONE, "Brightness from the light sensor"),
    CMD_PIC("pic-set-brightness", USER_CMD_PIC_SET_BRIGHTNESS, SML_USER_SET_BRIGHTNESS,
            ARG_UNSIGNED, 0, 4000, "Manual brightness, of 4000"),
    CMD_PIC("pic-config-show", USER_CMD_PIC_SHOW_CONFIG, SML_USER_SHOW_CONFIG, NONE,
            "The PIC's running configuration"),
    CMD_PIC("pic-eeprom-show", USER_CMD_PIC_SHOW_EEPROM, SML_USER_SHOW_SETTINGS, NONE,
            "The PIC's stored settings"),
    CMD_PIC("pic-config-clear", USER_CMD_PIC_CLEAR_ALL, SML_USER_DEFAULTS, NONE,
            "The PIC's settings to defaults"),
    CMD_PIC("pic-save", USER_CMD_PIC_SAVE, SML_USER_SAVE, NONE, "Save the PIC's settings"),
    CMD_PIC("pic-show-sources", USER_CMD_PIC_SHOW_SOURCES, SML_USER_SHOW_SOURCES, VERBOSE,
            "The PIC's time sources, ranked"),
    CMD_PIC("pic-debug", USER_CMD_PIC_DEBUG, SML_USER_SET_DEBUG, ARG_DEBUG, 0, 0,
            "The PIC's debug output: show, or set exactly"),
    CMD_PIC("pic-debug-enable", USER_CMD_PIC_DEBUG_ENABLE, SML_USER_SET_DEBUG, ARG_DEBUG, 0, 0,
            "Turn PIC debug output on (default all)"),
    CMD_PIC("pic-debug-disable", USER_CMD_PIC_DEBUG_DISABLE, SML_USER_SET_DEBUG, ARG_DEBUG, 0, 0,
            "Turn PIC debug output off (default all)"),

    CMD_ESP("rst-all", USER_CMD_RESET_ALL, NONE, "Reset the PIC and the ESP"),
    CMD_ESP("rst-esp", USER_CMD_ESP_RESET, NONE, "Same as esp-reset"),
    CMD_PIC("rst-pic", USER_CMD_PIC_RESET, SML_USER_RESET, NONE, "Same as pic-reset"),
    CMD_ESP("status", USER_CMD_STATUS, VERBOSE, "Time, sources and links; same as an empty line"),
    CMD_ESP("verbose", USER_CMD_VERBOSE, FLAG, "Everything without -v, until restart"),
    CMD_ESP("help", USER_CMD_HELP, NONE, "This text; an empty line prints the status"),
};

#define COMMAND_COUNT (sizeof(commands) / sizeof(commands[0]))

static const Command *find(const char *name)
{
    size_t index;

    for (index = 0; index < COMMAND_COUNT; index++)
    {
        if (strcmp(commands[index].name, name) == 0)
        {
            return &commands[index];
        }
    }
    return NULL;
}


static const struct
{
    const char *name;
    uint16_t bit;
} debug_names[] = {
    {"timing", SML_DEBUG_TIMING},   {"gnss", SML_DEBUG_GNSS}, {"sync", SML_DEBUG_SYNC},
    {"sources", SML_DEBUG_SOURCES}, {"ntp", SML_DEBUG_NTP},   {"minute", SML_DEBUG_MINUTE},
    {"tuning", SML_DEBUG_TUNING},   {"all", SML_DEBUG_ALL},
};

#define DEBUG_NAME_COUNT (sizeof(debug_names) / sizeof(debug_names[0]))

static void print_debug_names(Stream *output, uint16_t mask)
{
    size_t index;
    bool first = true;

    for (index = 0; index < DEBUG_NAME_COUNT; index++)
    {
        if (debug_names[index].bit != SML_DEBUG_ALL && (mask & debug_names[index].bit) != 0)
        {
            output->printf("%s%s", first ? "" : " ", debug_names[index].name);
            first = false;
        }
    }
}


// "gnss sync", "gnss,ntp", "all", "0x1f" or "31". True with *mask set;
// *given false for an empty argument.
static bool parse_debug_mask(const char *text, uint16_t *mask, bool *given)
{
    char word[16];
    uint32_t value;

    *mask = 0;
    *given = false;
    while (*text != '\0')
    {
        size_t length = 0;
        size_t index;
        bool known = false;

        while (*text == ' ' || *text == ',')
        {
            text++;
        }
        while (text[length] != '\0' && text[length] != ' ' && text[length] != ',')
        {
            length++;
        }
        if (length == 0)
        {
            break;
        }
        if (length >= sizeof(word))
        {
            return false;
        }
        memcpy(word, text, length);
        word[length] = '\0';
        text += length;
        *given = true;
        for (index = 0; index < DEBUG_NAME_COUNT; index++)
        {
            if (strcasecmp(word, debug_names[index].name) == 0)
            {
                *mask |= debug_names[index].bit;
                known = true;
            }
        }
        if (known)
        {
            continue;
        }
        if (word[0] == '0' && (word[1] == 'x' || word[1] == 'X'))
        {
            char *end;
            unsigned long parsed = strtoul(word + 2, &end, 16);

            if (*end != '\0' || end == word + 2 || parsed > 0xFFFFUL)
            {
                return false;
            }
            *mask |= (uint16_t)parsed;
        }
        else if (parse_u32(word, 0, 0xFFFF, &value))
        {
            *mask |= (uint16_t)value;
        }
        else
        {
            return false;
        }
    }
    return true;
}


// SML_USER_SET_DEBUG's argument: clear << 16 | set.
static uint32_t debug_argument(USER_CMD command, uint16_t mask, bool given)
{
    switch (command)
    {
        case USER_CMD_PIC_DEBUG_ENABLE: return given ? mask : SML_DEBUG_ALL;
        case USER_CMD_PIC_DEBUG_DISABLE: return (uint32_t)(given ? mask : 0xFFFFU) << 16;
        default: return given ? 0xFFFF0000UL | mask : 0; // 0: only show it
    }
}


static void print_help(Stream *output)
{
    static const char *const labels[] = {"", "[n]", "[n]", "[b]", "[s]", "[s]", "[-v]"};
    size_t index;

    for (index = 0; index < COMMAND_COUNT; index++)
    {
        const Command *command = &commands[index];
        char name[40];

        snprintf(name, sizeof(name), "%s %s", command->name, labels[command->argument]);
        output->printf("%-30s %s", name, command->help);
        if (command->argument == ARG_UNSIGNED || command->argument == ARG_SIGNED)
        {
            output->printf(" (%lld..%lld)", (long long)command->minimum,
                           (long long)command->maximum);
        }
        output->printf("\n");
    }
    output->printf("PIC debug subsystems: ");
    print_debug_names(output, SML_DEBUG_ALL);
    output->printf(", all, or a mask\n");
    output->printf("[-v]: everything, not only what matters (or: verbose on)\n");
}


const char *commands_reset_reason(void)
{
    esp_reset_reason_t reason = esp_reset_reason();

    switch (reason)
    {
        // The EN pin (the USB-UART's DTR/RTS reset) reports as power-on.
        case ESP_RST_POWERON: return "power-on or EN pin";
        case ESP_RST_EXT: return "external pin";
        case ESP_RST_SW: return "software";
        case ESP_RST_PANIC: return "panic";
        case ESP_RST_INT_WDT: return "interrupt watchdog";
        case ESP_RST_TASK_WDT: return "task watchdog";
        case ESP_RST_WDT: return "watchdog";
        case ESP_RST_DEEPSLEEP: return "deep sleep";
        case ESP_RST_BROWNOUT: return "brownout";
        case ESP_RST_SDIO: return "SDIO";
        default: return "unknown";
    }
}


void commands_print_status(Stream *output, bool verbose)
{
    uint32_t up_s = (uint32_t)(esp_timer_get_time() / 1000000);

    if (!verbose)
    {
        output->printf("\nESP:      %s %s, up ", ESP_VERSION, wifi_link_hostname());
        time_print_duration(output, up_s);
        output->printf("\n");
        ntp_print(output, false);
        tzinfo_print(output, false);
        leap_print(output, false);
        wifi_link_print(output, false);
        telnet_print(output, false);
        output->printf("\n");
        pic_report_print(output, false);
        return;
    }
    output->printf("\n=== ESP %s, %s ===\n", ESP_VERSION, wifi_link_hostname());
    output->printf("Up:        ");
    time_print_duration(output, up_s);
    output->printf(", last reset: %s\n", commands_reset_reason());
    ntp_print(output, true);
    output->printf("\n=== Time zone ===\n");
    tzinfo_print(output, true);
    leap_print(output, true);
    output->printf("\n=== Wi-Fi ===\n");
    wifi_link_print(output, true);
    telnet_print(output, true);
    pic_report_print(output, true);
}


// The argument as a 32-bit value for SML, checked against the command.
static bool argument_value(Stream *output, const Command *command, const char *text,
                           uint32_t *value)
{
    bool flag;
    int32_t signed_value;

    switch (command->argument)
    {
        case ARG_NONE: *value = 0; return true;
        case ARG_BOOL:
            if (parse_bool(text, &flag))
            {
                *value = flag ? 1 : 0;
                return true;
            }
            break;
        case ARG_UNSIGNED:
            if (parse_u32(text, (uint32_t)command->minimum, (uint32_t)command->maximum, value))
            {
                return true;
            }
            break;
        case ARG_SIGNED:
            if (parse_i32(text, (int32_t)command->minimum, (int32_t)command->maximum,
                          &signed_value))
            {
                *value = (uint32_t)signed_value; // two's complement on the wire
                return true;
            }
            break;
        case ARG_TEXT:
            if (text[0] != '\0' && strlen(text) <= USER_PREFS_MAX_STRING_SIZE)
            {
                *value = 0;
                return true;
            }
            break;
        case ARG_VERBOSE:
            if (text[0] == '\0')
            {
                *value = verbose_setting ? 1 : 0;
                return true;
            }
            if (strcmp(text, "-v") == 0 || strcasecmp(text, "v") == 0 ||
                strcasecmp(text, "verbose") == 0)
            {
                *value = 1;
                return true;
            }
            output->printf("%s: bad argument '%s' (-v for everything)\n", command->name, text);
            return false;
        case ARG_DEBUG:
        {
            uint16_t mask;
            bool given;

            if (parse_debug_mask(text, &mask, &given))
            {
                *value = debug_argument(command->command, mask, given);
                return true;
            }
            output->printf("%s: bad argument '%s': ", command->name, text);
            print_debug_names(output, SML_DEBUG_ALL);
            output->printf(", all, or a mask\n");
            return false;
        }
    }
    output->printf("%s: bad argument '%s'", command->name, text);
    if (command->argument == ARG_UNSIGNED || command->argument == ARG_SIGNED)
    {
        output->printf(" (%lld..%lld)", (long long)command->minimum, (long long)command->maximum);
    }
    output->printf("\n");
    return false;
}


static void run_esp(Stream *output, USER_CMD command, const char *text, uint32_t value)
{
    switch (command)
    {
        case USER_CMD_ESP_RESET:
            output->printf("Restarting the ESP\n");
            delay(100);
            ESP.restart();
            break;
        case USER_CMD_STATUS: commands_print_status(output, value != 0); break;
        case USER_CMD_VERBOSE:
            verbose_setting = value != 0;
            output->printf("Verbose output %s\n", verbose_setting ? "on" : "off");
            break;
        case USER_CMD_ESP_NTP_INFO: ntp_print(output, value != 0); break;
        case USER_CMD_ESP_NTP_SET_INTERVAL:
            user_prefs.fields.ntp.interval = value;
            output->printf("NTP interval %lu s\n", (unsigned long)value);
            break;
        case USER_CMD_ESP_NTP_SET_SERVER:
            user_prefs_set_string(user_prefs.fields.ntp.server, text);
            output->printf("NTP server %s, polling it now\n", user_prefs.fields.ntp.server);
            ntp_resync();
            break;
        case USER_CMD_ESP_NTP_RESYNC:
            output->printf("Polling NTP now\n");
            ntp_resync();
            break;
        case USER_CMD_ESP_WIFI_INFO: wifi_link_print(output, value != 0); break;
        case USER_CMD_ESP_WIFI_SCAN: wifi_link_scan(output); break;
        case USER_CMD_ESP_WIFI_CONNECT:
            output->printf("Reconnecting to Wi-Fi\n");
            wifi_link_reconnect();
            break;
        case USER_CMD_ESP_WIFI_DISCONNECT:
            output->printf("Disconnecting from Wi-Fi\n");
            wifi_link_disconnect();
            break;
        case USER_CMD_ESP_WIFI_CLEAR: wifi_link_clear(output); break;
        case USER_CMD_ESP_WIFI_SETUP: wifi_link_setup(output); break;
        case USER_CMD_ESP_TELNET_ENABLE: telnet_enable(output); break;
        case USER_CMD_ESP_TELNET_DISABLE: telnet_disable(output); break;
        case USER_CMD_ESP_TELNET_START: telnet_start(output); break;
        case USER_CMD_ESP_TELNET_STOP: telnet_stop(output); break;
        case USER_CMD_ESP_TELNET_PORT: telnet_set_port(output, (uint16_t)value); break;
        case USER_CMD_ESP_UPDATE_CHECK: updater_check(output); break;
        case USER_CMD_ESP_UPDATE_PULL: updater_pull(output); break;
        case USER_CMD_ESP_UPDATE_FORCE: updater_force(output); break;
        case USER_CMD_ESP_UPDATE_SET_SERVER:
            updater_set_server(text);
            output->printf("Update server %s\n", user_prefs.fields.updater.server);
            break;
        case USER_CMD_ESP_UPDATE_SET_PATH:
            updater_set_path(text);
            output->printf("Update path %s\n", user_prefs.fields.updater.path);
            break;
        case USER_CMD_ESP_UPDATE_SET_CONFIG:
            updater_set_config(text);
            output->printf("Update config '%s'\n", user_prefs.fields.updater.config);
            break;
        case USER_CMD_ESP_UPDATE_AUTO_ENABLE:
            user_prefs.fields.updater.flags.auto_enabled = 1;
            output->printf("Automatic updates on, at %02u:00 local\n",
                           user_prefs.fields.updater.flags.auto_time);
            break;
        case USER_CMD_ESP_UPDATE_AUTO_DISABLE:
            user_prefs.fields.updater.flags.auto_enabled = 0;
            output->printf("Automatic updates off\n");
            break;
        case USER_CMD_ESP_UPDATE_AUTO_HOUR:
            user_prefs.fields.updater.flags.auto_time = value;
            output->printf("Automatic updates at %02lu:00 local\n", (unsigned long)value);
            break;
        case USER_CMD_ESP_TZINFO_ENABLE:
            tzinfo_set_enabled(true);
            output->printf("Remote time zone on\n");
            break;
        case USER_CMD_ESP_TZINFO_DISABLE:
            tzinfo_set_enabled(false);
            output->printf("Remote time zone off\n");
            break;
        case USER_CMD_ESP_TZINFO_CHECK: tzinfo_check(output); break;
        case USER_CMD_ESP_TZINFO_UPDATE: tzinfo_update(output); break;
        case USER_CMD_ESP_TZINFO_SET_SERVER:
            tzinfo_set_server(text);
            output->printf("tzinfo server %s\n", user_prefs.fields.rtzinfo.server);
            break;
        case USER_CMD_ESP_TZINFO_SET_PATH:
            tzinfo_set_path(text);
            output->printf("tzinfo path %s\n", user_prefs.fields.rtzinfo.path);
            break;
        case USER_CMD_ESP_TZINFO_SET_INTERVAL:
            user_prefs.fields.rtzinfo.interval = value;
            output->printf("tzinfo interval %lu s\n", (unsigned long)value);
            break;
        case USER_CMD_ESP_TZINFO_SET_ACC:
            user_prefs.fields.rtzinfo.gnss_accuracy = (int32_t)value;
            if ((int32_t)value < 0)
            {
                output->printf("No position sent: tzinfo uses GeoIP\n");
            }
            else
            {
                output->printf("Position sent to %ld decimal places\n", (long)(int32_t)value);
            }
            break;
        case USER_CMD_ESP_CONFIG_SHOW: user_prefs_print(output); break;
        case USER_CMD_ESP_CONFIG_CLEAR:
        {
            bool telnet_was_enabled = user_prefs.fields.telnet.flags.enabled;

            user_prefs_reset();
            user_prefs_print(output);
            output->printf("Defaults in use, not saved: esp-save to keep them\n");
            if (telnet_was_enabled && !user_prefs.fields.telnet.flags.enabled)
            {
                output->printf("Telnet is now disabled: esp-telnet-enable to re-enable\n");
            }
            break;
        }
        case USER_CMD_ESP_CLEAR_ALL:
            user_prefs_reset();
            user_prefs_save();
            output->printf("ESP settings cleared\n");
            wifi_link_clear(output); // restarts
            break;
        case USER_CMD_ESP_SAVE:
            user_prefs_save();
            output->printf("ESP settings saved\n");
            break;
        case USER_CMD_RESET_ALL:
            output->printf("Resetting the PIC and the ESP\n");
            pic_link_request(SML_USER_RESET, 0, NULL);
            pic_link_uart()->flush();
            delay(100);
            ESP.restart();
            break;
        case USER_CMD_HELP: print_help(output); break;
        default: output->printf("Not implemented\n"); break;
    }
}


void commands_execute(Stream *output, char *line)
{
    char *argument;
    char *end;
    const Command *command;
    uint32_t value;

    while (isspace((unsigned char)*line))
    {
        line++;
    }
    end = line + strlen(line);
    while (end > line && isspace((unsigned char)end[-1]))
    {
        *--end = '\0';
    }
    if (*line == '\0')
    {
        commands_print_status(output, verbose_setting);
        return;
    }
    argument = line;
    while (*argument != '\0' && !isspace((unsigned char)*argument))
    {
        argument++;
    }
    if (*argument != '\0')
    {
        *argument++ = '\0';
        while (isspace((unsigned char)*argument))
        {
            argument++;
        }
    }
    command = find(line);
    if (command == NULL)
    {
        output->printf("Unknown command '%s': try help\n", line);
        return;
    }
    if (!argument_value(output, command, argument, &value))
    {
        return;
    }
    if (command->pic_command != 0)
    {
        if (!pic_link_is_sml())
        {
            output->printf("The PIC is not in SML mode; sending anyway\n");
        }
        if (command->argument == ARG_VERBOSE)
        {
            // Ours, not the PIC's: it prints what follows the answer.
            bool verbose = value != 0;

            pic_link_request(command->pic_command, 0, output, verbose);
            if (command->command == USER_CMD_PIC_INFO)
            {
                pic_report_print(output, verbose);
            }
            return;
        }
        pic_link_request(command->pic_command, value, output);
        return;
    }
    run_esp(output, command->command, argument, value);
}


uint16_t commands_from_pic(uint16_t command, uint32_t argument, uint16_t *detail, bool *restart)
{
    Stream *output = &Serial;

    *detail = SML_ERROR_NONE;
    switch (command)
    {
        case SML_USER_ESP_RESET: *restart = true; return SML_RESULT_ACK;
        case SML_USER_ESP_WIFI_CLEAR:
            wifi_link_forget();
            *restart = true; // after the answer
            return SML_RESULT_ACK;
        case SML_USER_ESP_WIFI_SETUP: wifi_link_setup(output); return SML_RESULT_ACK;
        case SML_USER_ESP_NTP_RESYNC: ntp_resync(); return SML_RESULT_ACK;
        case SML_USER_ESP_TZINFO_ENABLE:
            if (argument > 1)
            {
                *detail = SML_ERROR_ARGUMENT;
                return SML_RESULT_NACK;
            }
            tzinfo_set_enabled(argument != 0);
            user_prefs_save();
            return SML_RESULT_ACK;
        case SML_USER_ESP_TZINFO_UPDATE:
            if (tzinfo_update(output))
            {
                return SML_RESULT_ACK;
            }
            *detail = SML_ERROR_STATE;
            return SML_RESULT_NACK;
        case SML_USER_ESP_UPDATE_CHECK: updater_check(output); return SML_RESULT_ACK;
        case SML_USER_ESP_DEFAULTS:
            user_prefs_reset();
            user_prefs_save();
            return SML_RESULT_ACK;
        default: *detail = SML_ERROR_UNSUPPORTED; return SML_RESULT_NACK;
    }
}
