#include "parse.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static bool has_text(const char *text)
{
    return text != NULL && *text != '\0' && !isspace((unsigned char)*text);
}


bool parse_u32(const char *text, uint32_t minimum, uint32_t maximum, uint32_t *value)
{
    char *end;
    unsigned long long parsed;

    if (!has_text(text) || *text == '-')
    {
        return false;
    }
    errno = 0;
    parsed = strtoull(text, &end, 10);
    if (errno != 0 || *end != '\0' || parsed < minimum || parsed > maximum)
    {
        return false;
    }
    *value = (uint32_t)parsed;
    return true;
}


bool parse_i32(const char *text, int32_t minimum, int32_t maximum, int32_t *value)
{
    char *end;
    long long parsed;

    if (!has_text(text))
    {
        return false;
    }
    errno = 0;
    parsed = strtoll(text, &end, 10);
    if (errno != 0 || *end != '\0' || parsed < minimum || parsed > maximum)
    {
        return false;
    }
    *value = (int32_t)parsed;
    return true;
}


bool parse_bool(const char *text, bool *value)
{
    static const char *const yes[] = {"1", "true", "on", "yes"};
    static const char *const no[] = {"0", "false", "off", "no"};
    unsigned index;

    if (text == NULL)
    {
        return false;
    }
    for (index = 0; index < 4; index++)
    {
        if (strcasecmp(text, yes[index]) == 0)
        {
            *value = true;
            return true;
        }
        if (strcasecmp(text, no[index]) == 0)
        {
            *value = false;
            return true;
        }
    }
    return false;
}
