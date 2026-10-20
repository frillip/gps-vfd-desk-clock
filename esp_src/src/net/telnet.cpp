#include "telnet.h"

#include <ESPTelnetStream.h>
#include <WiFi.h>

#include "../console/commands.h"
#include "../console/console.h"
#include "../settings/user_prefs.h"

static ESPTelnetStream telnet;
static bool running;
static ConsoleLine line;

static void on_connect(String ip)
{
    Serial.printf("\nTelnet: %s connected\n", ip.c_str());
    telnet.printf("\nSmol Clock, %s\n(type 'quit' or 'exit' to disconnect)\n> ",
                  WiFi.getHostname());
    console_line_clear(&line);
}


static void on_disconnect(String ip)
{
    Serial.printf("Telnet: %s disconnected\n", ip.c_str());
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
