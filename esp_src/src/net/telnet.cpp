#include "telnet.h"

#include <ESPTelnetStream.h>
#include <WiFi.h>
#include <lwip/sockets.h>

#include "../console/commands.h"
#include "../console/console.h"
#include "../settings/user_prefs.h"

static ESPTelnetStream telnet;
static bool running;
static ConsoleLine line;
static uint32_t pending_dropped; // bytes dropped since the last note
static uint32_t dropped_bytes;   // ... in all
static uint32_t connections;
static uint32_t disconnections;

static void on_connect(String ip)
{
    Serial.printf("\nTelnet: %s connected\n", ip.c_str());
    telnet.printf("\nSmol Clock, %s\n(type 'quit' or 'exit' to disconnect)\n> ",
                  WiFi.getHostname());
    console_line_clear(&line);
    pending_dropped = 0;
    connections++;
}


static void on_disconnect(String ip)
{
    Serial.printf("Telnet: %s disconnected\n", ip.c_str());
    disconnections++;
}


static void on_reconnect(String ip)
{
    Serial.printf("\nTelnet: %s reconnected\n", ip.c_str());
}


static void on_attempt(String ip)
{
    Serial.printf("Telnet: %s tried to connect\n", ip.c_str());
}


void telnet_init(Stream *output)
{
    telnet.onConnect(on_connect);
    telnet.onConnectionAttempt(on_attempt);
    telnet.onReconnect(on_reconnect);
    telnet.onDisconnect(on_disconnect);
    if (user_prefs.fields.telnet.flags.enabled)
    {
        telnet_start(output);
    }
    else
    {
        output->printf("Telnet disabled, enable with \"esp-telnet-enable\"\n");
    }
}


void telnet_service(void)
{
    if (!running)
    {
        return;
    }
    telnet.loop();
    if (telnet.isConnected() && console_read_line(&line, &telnet, false))
    {
        if (strcmp(line.text, "quit") == 0 || strcmp(line.text, "exit") == 0)
        {
            telnet.disconnectClient();
        }
        else
        {
            commands_execute(&telnet, line.text);
            telnet.print("> ");
        }
        console_line_clear(&line);
    }
}


void telnet_start(Stream *output)
{
    if (running)
    {
        output->printf("Telnet already running on %s:%u\n", WiFi.localIP().toString().c_str(),
                       user_prefs.fields.telnet.port);
        return;
    }
    telnet.begin(user_prefs.fields.telnet.port, false);
    running = true;
    output->printf("Telnet running, connect on %s:%u\n", WiFi.localIP().toString().c_str(),
                   user_prefs.fields.telnet.port);
}


void telnet_stop(Stream *output)
{
    if (!running)
    {
        output->printf("Telnet is not running\n");
        return;
    }
    output->printf("Telnet stopping; the current session stays open\n");
    if (user_prefs.fields.telnet.flags.enabled)
    {
        output->printf("It starts again at reset: disable it with \"esp-telnet-disable\"\n");
    }
    telnet.stop(false);
    running = false;
}


void telnet_enable(Stream *output)
{
    user_prefs.fields.telnet.flags.enabled = 1;
    output->printf("Telnet enabled\n");
    if (!running)
    {
        telnet_start(output);
    }
}


void telnet_disable(Stream *output)
{
    user_prefs.fields.telnet.flags.enabled = 0;
    output->printf("Telnet disabled%s\n",
                   running ? " but still running: stop it with \"esp-telnet-stop\"" : "");
}


void telnet_set_port(Stream *output, uint16_t port)
{
    if (port == user_prefs.fields.telnet.port)
    {
        return;
    }
    user_prefs.fields.telnet.port = port;
    if (running)
    {
        telnet_stop(output);
        telnet_start(output);
    }
}


bool telnet_is_running(void)
{
    return running;
}


Stream *telnet_client(void)
{
    return running && telnet.isConnected() ? &telnet : NULL;
}


// As much as the socket takes now; true if all of it.
static bool send_now(int fd, const uint8_t *data, size_t length)
{
    ssize_t sent = send(fd, data, length, MSG_DONTWAIT);

    if (sent < 0)
    {
        sent = 0;
    }
    if ((size_t)sent < length)
    {
        pending_dropped += (uint32_t)(length - (size_t)sent);
        dropped_bytes += (uint32_t)(length - (size_t)sent);
        return false;
    }
    return true;
}


void telnet_write_text(const uint8_t *text, size_t length)
{
    int fd;

    if (!running || !telnet.isConnected())
    {
        return;
    }
    fd = telnet.getClient().fd();
    if (fd < 0)
    {
        return;
    }
    if (pending_dropped != 0)
    {
        char note[48];
        int n = snprintf(note, sizeof(note), "\r\n[telnet: %lu bytes dropped]\r\n",
                         (unsigned long)pending_dropped);

        if (send(fd, note, (size_t)n, MSG_DONTWAIT) != n)
        {
            // Still no room: this goes too, and the note waits.
            pending_dropped += (uint32_t)length;
            dropped_bytes += (uint32_t)length;
            return;
        }
        pending_dropped = 0;
    }
    send_now(fd, text, length);
}


void telnet_print(Stream *output)
{
    output->printf("Telnet:   %s", running ? "running" : "stopped");
    if (running && telnet.isConnected())
    {
        output->printf(", %s connected", telnet.getIP().c_str());
    }
    output->printf(", %lu connections, %lu disconnections, %lu bytes dropped\n",
                   (unsigned long)connections, (unsigned long)disconnections,
                   (unsigned long)dropped_bytes);
}
