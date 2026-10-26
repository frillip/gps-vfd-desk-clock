/*
 * The link to the ESP32 on UART1, in SML (common/SML_PROTOCOL.md).
 *
 * Two modes, so the clock works with either ESP firmware:
 *
 *   TEXT  console text goes out raw, as the bridging ESP firmware expects.
 *         Received bytes are still parsed: SML frames are acted on, anything
 *         else (the old firmware's own messages) is counted and dropped.
 *   SML   console text travels in SML-STAT-DEBUG frames, and the clock's
 *         state goes out every epoch (TIME-AUTH, TIME-PIC, TIME-CTRL,
 *         TIME-MEAS, TIME-GNSS, STAT-UI), with STAT-SOURCES when the
 *         synchroniser's choice or a verdict changes and every
 *         ESP_LINK_SOURCES_EPOCHS, STAT-ENV every ESP_LINK_ENV_EPOCHS,
 *         STAT-SETTINGS in the first epoch and
 *         whenever the revision or its flags change, then STAT-SYS
 *         (version, uptime, reset cause), every epoch too.
 *
 * The link starts in TEXT (or SML with ESP_LINK_START_SML) and moves to SML
 * for good on the first valid SML frame from the ESP: an SML ESP announces
 * itself by sending anything, e.g. CMD-USER INFO at boot.
 *
 * Received:
 *   TZ-STATE     -> timezone_apply() (always held; TZ/DST auto choose what is used)
 *   TIME-LEAP    -> sync_set_leap(): used when the selected source has none
 *   TIME-NTP     -> held for the NTP source (sync/source_ntp.c)
 *   STAT-NET     -> held for the menu and status (esp_link_net)
 *   CMD-RESPONSE -> the answer to a command sent with esp_link_request()
 *   CMD-USER     -> the commands below, answered with CMD-RESPONSE
 *   CMD-SETTINGS -> settings_write(): every setting at once, if
 *                   expected_revision is current (else STALE_REVISION);
 *                   PERSIST asks for them to be stored
 *
 * Commands handled: INFO, REQUEST_MESSAGE (TIME-AUTH/PIC/CTRL/MEAS/GNSS,
 * STAT-UI, STAT-ENV, STAT-SOURCES, STAT-SETTINGS, STAT-SYS), RESET,
 * SET_TZ_OFFSET, SET_DST_OFFSET, SET_DST_ACTIVE, SET_TZ_AUTO, SET_DST_AUTO,
 * SET_FOSC, SET_DISPLAY (HHMM only), SET_BRIGHTNESS (0 to 4000, and auto
 * off), SET_BRI_AUTO (auto on, argument ignored), SET_BEEP (chimes and
 * button clicks), SHOW_SOURCES (STAT-SOURCES, then
 * the ACK), SET_DEBUG (clear << 16 | set, ui/debug.h; the resulting mask is
 * printed; setting an undefined bit is NACK ARGUMENT, clearing one is not),
 * SAVE and DEFAULTS (ACCEPTED, then ACK or NACK STORAGE/VERIFY once
 * stored; BUSY while one is under way), SHOW_SETTINGS (what is stored,
 * printed, then STAT-SETTINGS) and SHOW_CONFIG (the running settings,
 * printed). The others, and every ESP command code (0x8000 up), are
 * answered NACK UNSUPPORTED. Settings changes run at once, through
 * settings/settings.h, and are kept over a reset once saved.
 *
 * The PIC sends commands of its own (SML_USER_ESP_*, e.g. from the menu) with
 * esp_link_request(); the ESP's answer is kept for esp_link_response().
 *
 * Everything here runs in the foreground: esp_link_service() every 10 ms,
 * esp_link_epoch() right after sync_update().
 */
#ifndef ESP_LINK_H
#define ESP_LINK_H

#include <stdbool.h>
#include <stdint.h>

#include "../sync/sync.h"
#include "../timing/timing.h"
#include "../../common/sml.h"

#ifndef ESP_LINK_START_SML
#define ESP_LINK_START_SML 0
#endif

// A frame part-received this long ago is abandoned (a full frame takes
// 22 ms at 115200 baud).
#define ESP_LINK_FRAME_TIMEOUT_MS 100U
// STAT-NET comes every 10 s: after this long without one it is unknown.
#define ESP_LINK_NET_TIMEOUT_MS 30000UL
// STAT-SOURCES goes out at least this often, changed or not.
#define ESP_LINK_SOURCES_EPOCHS 10UL
// STAT-ENV (the light sensor) goes out in every epoch numbered a multiple
// of this.
#define ESP_LINK_ENV_EPOCHS 5UL

typedef struct
{
    bool sml;                 // in SML mode
    uint32_t received;        // valid frames from the ESP
    uint32_t handled;         // ... acted on
    uint32_t unknown;         // class and ID not in the protocol
    uint32_t wrong_direction; // a PIC -> ESP message coming back
    uint32_t wrong_length;
    uint32_t tz_states;
    uint32_t leaps;
    uint32_t ntp_times;
    uint32_t net_states;
    uint32_t requests;  // commands sent to the ESP
    uint32_t responses; // ... answers received
    uint32_t commands;
    uint32_t settings_writes; // CMD-SETTINGS
    uint32_t nacks;
    uint32_t sent;        // frames sent, debug text included
    uint32_t text_frames; // STAT-DEBUG frames
    uint32_t tx_waits;    // frames that had to wait for TX ring space
    uint32_t rx_errors;   // UART errors (each loses the partial frame)
} EspLinkStats;

// After console_init(), which starts UART1.
void esp_link_init(void);
// Every 10 ms: received bytes, commands, and any console text pending.
void esp_link_service(uint32_t now_ms);
// Once per epoch, straight after sync_update(): the epoch's messages.
void esp_link_epoch(const TimingSnapshot *snapshot, const SyncUpdate *update, uint32_t now_ms);

bool esp_link_is_sml(void);
void esp_link_set_sml(void);
const EspLinkStats *esp_link_stats(void);
const SmlParserStats *esp_link_parser_stats(void);
// The latest TIME-NTP and TIME-LEAP, if any.
bool esp_link_ntp(SML_TIME_NTP_PAYLOAD *ntp);
bool esp_link_leap(SML_TIME_LEAP_PAYLOAD *leap);
// The latest STAT-NET, if one arrived within ESP_LINK_NET_TIMEOUT_MS.
bool esp_link_net(uint32_t now_ms, SML_STAT_NET_PAYLOAD *net);

// Sends a command for the ESP (SML_USER_ESP_*). Returns its token, or 0 if
// the link is not in SML mode (an older ESP would not understand it).
uint16_t esp_link_request(uint16_t command, uint32_t argument);
// The latest CMD-RESPONSE from the ESP, if any.
bool esp_link_response(SML_CMD_RESPONSE_PAYLOAD *response);

// Sends one frame now, waiting for TX ring space if need be.
void esp_link_send(uint8_t class_id, uint8_t message_id, const void *native);

void esp_link_print(void);

#endif // ESP_LINK_H
