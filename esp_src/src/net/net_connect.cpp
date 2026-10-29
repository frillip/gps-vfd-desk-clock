#include "net_connect.h"

#include <WiFi.h>
#include <lwip/netdb.h>
#include <string.h>

#include "../util/time_text.h"

enum
{
    FAMILY_IPV6 = 0,
    FAMILY_IPV4,
    FAMILIES
};

typedef struct
{
    bool failed;
    uint32_t failed_ms;
    uint32_t failures;
} Family;

static Family families[FAMILIES];

static int family_of(const IPAddress &address)
{
    return address.type() == IPv6 ? FAMILY_IPV6 : FAMILY_IPV4;
}


static bool held(int family)
{
    Family *state = &families[family];

    if (state->failed && millis() - state->failed_ms >= NET_FAMILY_HOLD_MS)
    {
        state->failed = false;
    }
    return state->failed;
}


// IPv6 first, unless it failed lately and IPv4 has not failed since.
static bool prefer_ipv6(void)
{
    if (!held(FAMILY_IPV6))
    {
        return true;
    }
    return held(FAMILY_IPV4) &&
           (int32_t)(families[FAMILY_IPV4].failed_ms - families[FAMILY_IPV6].failed_ms) > 0;
}


// One family's address for host, if it has one.
static bool lookup(const char *host, int family, IPAddress *address)
{
    struct addrinfo hints;
    struct addrinfo *result = NULL;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = family == FAMILY_IPV6 ? AF_INET6 : AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (lwip_getaddrinfo(host, "0", &hints, &result) != 0 || result == NULL)
    {
        return false;
    }
    if (result->ai_family == AF_INET6)
    {
        const struct sockaddr_in6 *six = (const struct sockaddr_in6 *)result->ai_addr;
        *address = IPAddress(IPv6, six->sin6_addr.s6_addr, (uint8_t)six->sin6_scope_id);
    }
    else
    {
        const struct sockaddr_in *four = (const struct sockaddr_in *)result->ai_addr;
        *address = IPAddress(four->sin_addr.s_addr);
    }
    lwip_freeaddrinfo(result);
    return true;
}


int net_resolve(const char *host, IPAddress addresses[NET_ADDRESSES])
{
    IPAddress ipv6;
    IPAddress ipv4;
    bool have_ipv6;
    bool have_ipv4;
    int count = 0;

    // A literal, IPv6 bracketed or not.
    if (host[0] == '[')
    {
        char literal[48];
        size_t length = strcspn(host + 1, "]");

        if (host[1 + length] == ']' && host[2 + length] == '\0' && length < sizeof(literal))
        {
            memcpy(literal, host + 1, length);
            literal[length] = '\0';
            if (addresses[0].fromString(literal) && addresses[0].type() == IPv6)
            {
                return 1;
            }
        }
        return 0;
    }
    if (addresses[0].fromString(host))
    {
        return 1;
    }
    // Without a global IPv6 address of our own, IPv6 could not be reached.
    have_ipv6 = net_ipv6_ready() && lookup(host, FAMILY_IPV6, &ipv6);
    have_ipv4 = lookup(host, FAMILY_IPV4, &ipv4);
    if (have_ipv6 && (prefer_ipv6() || !have_ipv4))
    {
        addresses[count++] = ipv6;
        have_ipv6 = false;
    }
    if (have_ipv4)
    {
        addresses[count++] = ipv4;
    }
    if (have_ipv6)
    {
        addresses[count++] = ipv6;
    }
    return count;
}


bool net_ipv6_ready(void)
{
    return WiFi.STA.hasGlobalIPv6();
}


void net_note_result(const IPAddress &address, bool worked)
{
    Family *state = &families[family_of(address)];

    if (worked)
    {
        state->failed = false;
        return;
    }
    state->failed = true;
    state->failed_ms = millis();
    state->failures++;
}


// The host and port of url: scheme://host[:port]/..., host a name, an IPv4
// literal or a bracketed IPv6 one. False if it is none of those.
static bool parse_url(const char *url, char *host, size_t size, uint16_t *port, bool *secure)
{
    const char *start;
    const char *end;
    const char *colon = NULL;
    size_t length;

    if (strncmp(url, "https://", 8) == 0)
    {
        *secure = true;
        *port = 443;
        start = url + 8;
    }
    else if (strncmp(url, "http://", 7) == 0)
    {
        *secure = false;
        *port = 80;
        start = url + 7;
    }
    else
    {
        return false;
    }
    if (*start == '[')
    {
        start++;
        end = strchr(start, ']');
        if (end == NULL)
        {
            return false;
        }
        if (end[1] == ':')
        {
            colon = end + 1;
        }
    }
    else
    {
        end = start + strcspn(start, ":/?#");
        if (*end == ':')
        {
            colon = end;
        }
    }
    length = (size_t)(end - start);
    if (length == 0 || length >= size)
    {
        return false;
    }
    memcpy(host, start, length);
    host[length] = '\0';
    if (colon != NULL)
    {
        long value = strtol(colon + 1, NULL, 10);
        if (value <= 0 || value > 65535)
        {
            return false;
        }
        *port = (uint16_t)value;
    }
    return true;
}


bool net_http_connect(NetworkClientSecure &secure, NetworkClient &plain, const char *url,
                      const char *ca, uint32_t timeout_ms, IPAddress *used)
{
    IPAddress addresses[NET_ADDRESSES];
    char host[128];
    uint16_t port;
    bool tls;
    int count;
    int index;

    if (!parse_url(url, host, sizeof(host), &port, &tls))
    {
        return false;
    }
    count = net_resolve(host, addresses);
    for (index = 0; index < count; index++)
    {
        bool connected;

        if (tls)
        {
            // Should HTTPClient connect again itself (it reconnects by name
            // if it finds the connection closed), that is verified too.
            secure.setCACert(ca);
            secure.setConnectionTimeout(timeout_ms);
            // The host name for SNI and the certificate, whichever address.
            connected = secure.connect(addresses[index], port, host, ca, NULL, NULL) == 1;
        }
        else
        {
            connected = plain.connect(addresses[index], port, (int32_t)timeout_ms) == 1;
        }
        // Only the connection counts against a family: a server that then
        // refuses the request would refuse it on either.
        net_note_result(addresses[index], connected);
        if (connected)
        {
            *used = addresses[index];
            return true;
        }
    }
    return false;
}


String net_url_host(const char *host)
{
    IPAddress literal;

    if (host[0] != '[' && literal.fromString(host) && literal.type() == IPv6)
    {
        return String("[") + host + "]";
    }
    return String(host);
}


void net_connect_print(Stream *output)
{
    static const char *const names[FAMILIES] = {"IPv6", "IPv4"};
    int family;

    output->printf("Prefer: %s first", prefer_ipv6() ? "IPv6" : "IPv4");
    for (family = 0; family < FAMILIES; family++)
    {
        if (held(family))
        {
            output->printf("; %s failed ", names[family]);
            time_print_duration(output, (millis() - families[family].failed_ms) / 1000U);
            output->printf(" ago");
        }
        if (families[family].failures != 0)
        {
            output->printf("; %s %lu failures", names[family],
                           (unsigned long)families[family].failures);
        }
    }
    output->printf("\n");
}
