#include "ntp_client.h"

#include <Arduino.h>
#include <esp_random.h>
#include <esp_timer.h>
#include <lwip/sockets.h>
#include <string.h>

#define TASK_STACK_BYTES 4096
#define TASK_PRIORITY    5

static TaskHandle_t task;
static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
static NtpServerAddress server_address;
static volatile bool busy;
static bool result_ready;
static NtpPollResult result;
static NtpClientStats stats;

static uint64_t random_cookie(void)
{
    return (uint64_t)esp_random() << 32 | esp_random();
}


// One request and its reply. True with a sample; poll->last_result and
// poll->kiss say why not.
static bool exchange(int sock, const struct sockaddr_storage *server, socklen_t length,
                     EspClockSample *sample, NtpPollResult *poll)
{
    uint8_t packet[NTP_PACKET_BYTES];
    uint64_t cookie = random_cookie();
    NtpReply reply;
    int64_t t1;
    int64_t t4;
    int64_t deadline;

    ntp_packet_request(packet, cookie);
    t1 = esp_timer_get_time();
    if (sendto(sock, packet, sizeof(packet), 0, (const struct sockaddr *)server, length) !=
        (int)sizeof(packet))
    {
        return false;
    }
    deadline = t1 + NTP_CLIENT_TIMEOUT_MS * 1000LL;
    for (;;)
    {
        int received = recvfrom(sock, packet, sizeof(packet), 0, NULL, NULL);

        t4 = esp_timer_get_time();
        if (received < 0)
        {
            stats.timeouts++;
            return false;
        }
        NtpReplyResult outcome = ntp_packet_parse(packet, (uint32_t)received, cookie, &reply);
        poll->last_result = outcome;
        if (outcome == NTP_REPLY_OK)
        {
            break;
        }
        stats.rejected[outcome]++;
        if (outcome == NTP_REPLY_KISS)
        {
            memcpy(poll->kiss, reply.kiss, sizeof(poll->kiss));
            return false;
        }
        // A late reply to an earlier request: keep listening until the deadline.
        if (outcome != NTP_REPLY_WRONG_ORIGIN || t4 >= deadline)
        {
            return false;
        }
    }
    stats.exchanges++;
    // offset = ((T2 - T1) + (T3 - T4)) / 2, delay = (T4 - T1) - (T3 - T2).
    sample->local_us = t1 + (t4 - t1) / 2;
    sample->offset_us = ((reply.receive_utc_us - t1) + (reply.transmit_utc_us - t4)) / 2;
    sample->delay_us = (t4 - t1) - (reply.transmit_utc_us - reply.receive_utc_us);
    if (sample->delay_us < 0)
    {
        sample->delay_us = 0;
    }
    sample->root_delay_us = reply.root_delay_us;
    sample->root_dispersion_us = reply.root_dispersion_us;
    sample->stratum = reply.stratum;
    return true;
}


// The server's socket address, and its address as text: as IPAddress
// writes it, as everywhere else (lwIP's inet_ntop writes IPv6 in capitals).
static socklen_t server_socket_address(const NtpServerAddress *address,
                                       struct sockaddr_storage *server, char *text, size_t size)
{
    IPAddress shown(address->ipv6 ? IPv6 : IPv4, address->bytes, address->zone);

    snprintf(text, size, "%s", shown.toString().c_str());
    memset(server, 0, sizeof(*server));
    if (address->ipv6)
    {
        struct sockaddr_in6 *six = (struct sockaddr_in6 *)server;

        six->sin6_family = AF_INET6;
        six->sin6_port = htons(NTP_PORT);
        memcpy(six->sin6_addr.s6_addr, address->bytes, 16);
        six->sin6_scope_id = address->zone;
        return sizeof(*six);
    }
    struct sockaddr_in *four = (struct sockaddr_in *)server;

    four->sin_family = AF_INET;
    four->sin_port = htons(NTP_PORT);
    memcpy(&four->sin_addr.s_addr, address->bytes, 4);
    return sizeof(*four);
}


static void poll_server(const NtpServerAddress *address, NtpPollResult *poll)
{
    struct sockaddr_storage server;
    struct timeval timeout = {NTP_CLIENT_TIMEOUT_MS / 1000, (NTP_CLIENT_TIMEOUT_MS % 1000) * 1000};
    socklen_t length;
    int sock;
    int index;

    memset(poll, 0, sizeof(*poll));
    length = server_socket_address(address, &server, poll->address, sizeof(poll->address));

    sock = socket(server.ss_family, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0)
    {
        return;
    }
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    for (index = 0; index < NTP_CLIENT_BURST; index++)
    {
        EspClockSample sample;

        if (index != 0)
        {
            vTaskDelay(pdMS_TO_TICKS(NTP_CLIENT_SPACING_MS));
        }
        if (exchange(sock, &server, length, &sample, poll))
        {
            if (!poll->ok || sample.delay_us < poll->sample.delay_us)
            {
                poll->sample = sample;
            }
            poll->ok = true;
            poll->replies++;
        }
        else if (poll->kiss[0] != '\0')
        {
            break; // told to go away: no more requests this poll
        }
    }
    close(sock);
}


static void client_task(void *argument)
{
    (void)argument;
    for (;;)
    {
        NtpPollResult poll;

        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        poll_server(&server_address, &poll);
        stats.polls++;
        if (poll.ok)
        {
            stats.good_polls++;
        }
        portENTER_CRITICAL(&lock);
        result = poll;
        result_ready = true;
        portEXIT_CRITICAL(&lock);
        busy = false;
    }
}


void ntp_client_init(void)
{
    xTaskCreatePinnedToCore(client_task, "ntp", TASK_STACK_BYTES, NULL, TASK_PRIORITY, &task,
                            ARDUINO_RUNNING_CORE);
}


bool ntp_client_poll(const NtpServerAddress *address)
{
    if (busy || task == NULL)
    {
        return false;
    }
    server_address = *address; // read by the task only after the notification
    busy = true;
    xTaskNotifyGive(task);
    return true;
}


bool ntp_client_busy(void)
{
    return busy;
}


bool ntp_client_take(NtpPollResult *destination)
{
    bool ready;

    portENTER_CRITICAL(&lock);
    ready = result_ready;
    if (ready)
    {
        *destination = result;
        result_ready = false;
    }
    portEXIT_CRITICAL(&lock);
    return ready;
}


const NtpClientStats *ntp_client_stats(void)
{
    return &stats;
}
