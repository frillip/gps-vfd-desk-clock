#include "pic_link.h"

#include <esp_timer.h>
#include <string.h>

#include "../board.h"
#include "../console/console.h"
#include "../timing/pps.h"
#include "pic_report.h"
#include "sml_stream.h"

#define RX_BUFFER_BYTES    4096
#define TX_BUFFER_BYTES    1024
#define FRAME_TIMEOUT_MS   100U
#define SML_TIMEOUT_MS     5000U
#define BACKLOG_MS         300U
#define PAIR_WINDOW_US     900000LL
#define TIME_TIMEOUT_US    10000000LL
#define REQUEST_TIMEOUT_MS 5000U
#define PENDING_MAX        8
#define HELD_MAX           16

typedef struct
{
    uint8_t class_id;
    uint8_t message_id;
    bool valid;
    uint32_t received_ms;
    uint8_t payload[SML_MAX_PAYLOAD_BYTES]; // native (decoded) form
} Held;

typedef struct
{
    uint16_t token;
    uint16_t command;
    Stream *reply_to;
    bool verbose; // for what is printed with the answer
    uint32_t sent_ms;
} Pending;

static HardwareSerial uart(BOARD_PIC_UART);
static SmlStream stream;
static PicLinkStats stats;
static Held held[HELD_MAX];
static Pending pending[PENDING_MAX];
static PicCommandHandler command_handler;
static PicTime pic_time;
static uint16_t next_token = 1;
static uint32_t last_byte_ms;
static uint32_t last_frame_ms;
static uint32_t last_service_ms;
static bool backlog; // this service found the loop had been held up

static Held *find_held(uint8_t class_id, uint8_t message_id, bool create)
{
    int index;

    for (index = 0; index < HELD_MAX; index++)
    {
        if (held[index].valid && held[index].class_id == class_id &&
            held[index].message_id == message_id)
        {
            return &held[index];
        }
    }
    if (!create)
    {
        return NULL;
    }
    for (index = 0; index < HELD_MAX; index++)
    {
        if (!held[index].valid)
        {
            held[index].class_id = class_id;
            held[index].message_id = message_id;
            return &held[index];
        }
    }
    return NULL;
}


void pic_link_send(uint8_t class_id, uint8_t message_id, const void *native)
{
    uint8_t frame[SML_MAX_FRAME_BYTES];
    uint16_t length;

    length = sml_frame_message(class_id, message_id, native, frame);
    if (length != 0)
    {
        uart.write(frame, length);
        stats.sent++;
    }
}


static void respond(uint16_t token, uint16_t command, uint16_t result, uint16_t detail)
{
    SML_CMD_RESPONSE_PAYLOAD response;

    memset(&response, 0, sizeof(response));
    response.status = result;
    response.detail = detail;
    response.token = token;
    response.command = command;
    pic_link_send(SML_CLASS_CMD, SML_CMD_RESPONSE, &response);
}


uint16_t pic_link_request(uint16_t command, uint32_t argument, Stream *reply_to, bool verbose)
{
    SML_CMD_USER_PAYLOAD request;
    Pending *slot = &pending[0];
    int index;

    for (index = 0; index < PENDING_MAX; index++)
    {
        if (pending[index].token == 0)
        {
            slot = &pending[index];
            break;
        }
        if ((int32_t)(pending[index].sent_ms - slot->sent_ms) < 0)
        {
            slot = &pending[index]; // full: reuse the oldest
        }
    }
    memset(&request, 0, sizeof(request));
    request.command = command;
    request.token = next_token;
    request.argument = argument;
    slot->token = next_token;
    slot->command = command;
    slot->reply_to = reply_to;
    slot->verbose = verbose;
    slot->sent_ms = millis();
    next_token = next_token == 0xFFFF ? 1 : next_token + 1;
    pic_link_send(SML_CLASS_CMD, SML_CMD_USER, &request);
    stats.requests++;
    return request.token;
}


static void handle_response(const SML_CMD_RESPONSE_PAYLOAD *response)
{
    int index;

    stats.responses++;
    for (index = 0; index < PENDING_MAX; index++)
    {
        Pending *slot = &pending[index];

        if (slot->token == 0 || slot->token != response->token)
        {
            continue;
        }
        if (slot->reply_to != NULL)
        {
            // The PIC sends STAT-SOURCES before its ACK; the table is the
            // answer, so the ACK itself only shows with -v.
            bool table =
                slot->command == SML_USER_SHOW_SOURCES && response->status == SML_RESULT_ACK;

            if (!table || slot->verbose)
            {
                slot->reply_to->printf("PIC %s: %s", pic_report_command_name(slot->command),
                                       pic_report_result_name(response->status));
                if (response->status == SML_RESULT_NACK)
                {
                    slot->reply_to->printf(" (%s)", pic_report_error_name(response->detail));
                }
                slot->reply_to->printf("\n");
            }
            if (table)
            {
                pic_report_print_sources(slot->reply_to, slot->verbose);
            }
        }
        if (response->status != SML_RESULT_ACCEPTED)
        {
            slot->token = 0; // final answer
        }
        return;
    }
}


static void expire_requests(void)
{
    int index;

    for (index = 0; index < PENDING_MAX; index++)
    {
        Pending *slot = &pending[index];

        if (slot->token != 0 && millis() - slot->sent_ms > REQUEST_TIMEOUT_MS)
        {
            if (slot->reply_to != NULL)
            {
                slot->reply_to->printf("PIC %s: no response\n",
                                       pic_report_command_name(slot->command));
            }
            slot->token = 0;
            stats.unanswered++;
        }
    }
}


static void handle_command(const SML_CMD_USER_PAYLOAD *command)
{
    uint16_t detail = SML_ERROR_UNSUPPORTED;
    uint16_t result = SML_RESULT_NACK;
    bool restart = false;

    stats.commands++;
    if (command->command >= SML_USER_ESP_FIRST && command_handler != NULL)
    {
        detail = SML_ERROR_NONE;
        result = command_handler(command->command, command->argument, &detail, &restart);
    }
    respond(command->token, command->command, result, detail);
    if (restart)
    {
        uart.flush();
        delay(100);
        ESP.restart();
    }
}


static void pair_time(const SML_TIME_AUTH_PAYLOAD *auth)
{
    PpsEdge edge;
    int64_t now = esp_timer_get_time();

    if (backlog || !pps_pic_latest(&edge) || now - edge.local_us > PAIR_WINDOW_US ||
        (auth->flags & SML_AUTH_UTC_VALID) == 0)
    {
        stats.unpaired++;
        return;
    }
    pic_time.valid = true;
    pic_time.edge_local_us = edge.local_us;
    pic_time.edge_utc_us = (int64_t)auth->utc_seconds * 1000000 + auth->utc_phase_ns / 1000;
    pic_time.uncertainty_ns = auth->uncertainty_ns;
    pic_time.uncertainty_valid = (auth->flags & SML_AUTH_UNCERTAINTY_VALID) != 0;
    pic_time.source = auth->source;
    pic_time.lock_state = auth->lock_state;
    stats.pairings++;
}


// STAT-SYS: announce the PIC the first time, and whenever its uptime drops
// (it restarted), with why.
static void note_system(const SML_STAT_SYS_PAYLOAD *sys)
{
    static bool seen;
    static uint32_t last_uptime_s;
    char version[16];

    pic_report_version(version, sizeof(version), sys->pic_version);
    if (!seen || sys->uptime_s < last_uptime_s)
    {
        if (seen)
        {
            stats.restarts++;
        }
        console_printf("%s: firmware %s, up %lu s, last reset: %s\n",
                       seen ? "PIC restarted" : "PIC", version, (unsigned long)sys->uptime_s,
                       pic_report_reset_name(sys->reset_cause));
    }
    seen = true;
    last_uptime_s = sys->uptime_s;
}


static void handle_frame(const SmlFrame *frame)
{
    const SmlMessage *message = sml_message(frame->class_id, frame->message_id);
    uint8_t native[SML_MAX_PAYLOAD_BYTES];

    stats.frames++;
    last_frame_ms = millis();
    if (message == NULL)
    {
        stats.unknown++;
        return;
    }
    if ((message->direction & SML_FROM_PIC) == 0)
    {
        stats.wrong_direction++;
        return;
    }
    if (message->length == 0) // STAT-DEBUG
    {
        stats.text_frames++;
        console_write_text(frame->payload, frame->length);
        return;
    }
    if (frame->length != message->length)
    {
        stats.wrong_length++;
        return;
    }
    sml_decode(message->layout, frame->payload, native);
    if (frame->class_id == SML_CLASS_CMD)
    {
        if (frame->message_id == SML_CMD_USER)
        {
            handle_command((const SML_CMD_USER_PAYLOAD *)native);
        }
        else if (frame->message_id == SML_CMD_RESPONSE)
        {
            handle_response((const SML_CMD_RESPONSE_PAYLOAD *)native);
        }
        return;
    }
    if (frame->class_id == SML_CLASS_TIME && frame->message_id == SML_TIME_AUTH)
    {
        pair_time((const SML_TIME_AUTH_PAYLOAD *)native);
    }
    if (frame->class_id == SML_CLASS_STAT && frame->message_id == SML_STAT_SYS)
    {
        note_system((const SML_STAT_SYS_PAYLOAD *)native);
    }
    Held *slot = find_held(frame->class_id, frame->message_id, true);
    if (slot != NULL)
    {
        memcpy(slot->payload, native, message->length);
        slot->received_ms = millis();
        slot->valid = true;
    }
}


// Text outside frames: the PIC's raw console output. Printable only.
static void on_text(const uint8_t *text, uint16_t length, void *context)
{
    uint8_t out[SML_MAX_FRAME_BYTES + 1];
    uint16_t kept = 0;
    uint16_t index;

    (void)context;
    for (index = 0; index < length; index++)
    {
        uint8_t c = text[index];

        if ((c >= 0x20 && c < 0x7F) || c == '\r' || c == '\n' || c == '\t')
        {
            out[kept++] = c;
        }
    }
    if (kept != 0)
    {
        console_write_text(out, kept);
        stats.text_bytes += kept;
    }
}


static void on_frame(const SmlFrame *frame, void *context)
{
    (void)context;
    handle_frame(frame);
}


void pic_link_init(void)
{
    uart.setRxBufferSize(RX_BUFFER_BYTES);
    uart.setTxBufferSize(TX_BUFFER_BYTES);
    uart.begin(BOARD_PIC_BAUD, SERIAL_8N1, BOARD_PIC_RX_PIN, BOARD_PIC_TX_PIN);
    sml_stream_init(&stream, on_frame, on_text, NULL);
    last_service_ms = millis();

    SML_CMD_USER_PAYLOAD info;
    memset(&info, 0, sizeof(info));
    info.command = SML_USER_INFO;
    info.token = 0;
    // Announce SML: the PIC switches modes on this frame and prints its state.
    pic_link_send(SML_CLASS_CMD, SML_CMD_USER, &info);
}


void pic_link_service(void)
{
    uint32_t now = millis();

    backlog = now - last_service_ms > BACKLOG_MS;
    if (backlog && uart.available())
    {
        stats.backlogs++;
    }
    last_service_ms = now;
    while (uart.available())
    {
        sml_stream_push(&stream, (uint8_t)uart.read());
        last_byte_ms = now;
    }
    if (now - last_byte_ms > FRAME_TIMEOUT_MS)
    {
        sml_stream_timeout(&stream);
    }
    expire_requests();
}


void pic_link_set_command_handler(PicCommandHandler handler)
{
    command_handler = handler;
}


bool pic_link_is_sml(void)
{
    return stats.frames != 0 && millis() - last_frame_ms < SML_TIMEOUT_MS;
}


bool pic_link_get(uint8_t class_id, uint8_t message_id, void *native, uint32_t max_age_ms)
{
    const Held *slot = find_held(class_id, message_id, false);
    const SmlMessage *message = sml_message(class_id, message_id);

    if (slot == NULL || message == NULL || millis() - slot->received_ms > max_age_ms)
    {
        return false;
    }
    if (native != NULL)
    {
        memcpy(native, slot->payload, message->length);
    }
    return true;
}


bool pic_link_time(PicTime *time)
{
    *time = pic_time;
    return pic_time.valid;
}


bool pic_link_utc_now(uint32_t *utc_seconds)
{
    int64_t elapsed = esp_timer_get_time() - pic_time.edge_local_us;

    if (!pic_time.valid || elapsed > TIME_TIMEOUT_US)
    {
        return false;
    }
    *utc_seconds = (uint32_t)((pic_time.edge_utc_us + elapsed) / 1000000);
    return true;
}


HardwareSerial *pic_link_uart(void)
{
    return &uart;
}


const PicLinkStats *pic_link_stats(void)
{
    return &stats;
}


const SmlParserStats *pic_link_parser_stats(void)
{
    return &stream.parser.stats;
}
