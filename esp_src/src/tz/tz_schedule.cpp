#include "tz_schedule.h"

#include <ArduinoJson.h>
#include <string.h>

#define SERVER_ENTRIES_MAX 64 // more than any server sends; protects the parse

static bool read_u32(JsonVariantConst value, uint32_t *out)
{
    if (!value.is<int64_t>())
    {
        return false;
    }
    int64_t number = value.as<int64_t>();
    if (number < 0 || number > (int64_t)UINT32_MAX)
    {
        return false;
    }
    *out = (uint32_t)number;
    return true;
}


// One state object: {"transition_utc", "offset", "dst_offset", "dst"}.
static TzParseResult read_entry(JsonVariantConst object, TzEntry *entry)
{
    int64_t base;
    int64_t dst;

    if (!object.is<JsonObjectConst>() || !read_u32(object["transition_utc"], &entry->utc_seconds) ||
        !object["offset"].is<int64_t>() || !object["dst_offset"].is<int64_t>() ||
        !object["dst"].is<bool>())
    {
        return TZ_PARSE_FIELD;
    }
    base = object["offset"].as<int64_t>();
    dst = object["dst_offset"].as<int64_t>();
    if (base < -TZ_SCHEDULE_OFFSET_MAX || base > TZ_SCHEDULE_OFFSET_MAX || dst < INT16_MIN ||
        dst > INT16_MAX)
    {
        return TZ_PARSE_RANGE;
    }
    entry->base_offset_s = (int32_t)base;
    entry->dst_offset_s = (int16_t)dst;
    entry->dst_active = object["dst"].as<bool>();
    return TZ_PARSE_OK;
}


TzParseResult tz_schedule_parse_v1(const char *json, size_t length, TzSchedule *schedule,
                                   char *error, size_t error_size)
{
    JsonDocument document;
    TzEntry entry;
    TzParseResult result;
    size_t index;
    size_t future;
    uint32_t previous;

    memset(schedule, 0, sizeof(*schedule));
    if (error_size != 0)
    {
        error[0] = '\0';
    }
    if (deserializeJson(document, json, length) != DeserializationError::Ok ||
        !document.is<JsonObjectConst>())
    {
        return TZ_PARSE_JSON;
    }
    if (document["error"].is<JsonObjectConst>())
    {
        const char *code = document["error"]["code"] | "?";
        const char *message = document["error"]["message"] | "";

        snprintf(error, error_size, "%s: %s", code, message);
        return TZ_PARSE_SERVER_ERROR;
    }
    if (document["version"].as<int>() != 1)
    {
        return TZ_PARSE_VERSION;
    }
    JsonArrayConst current = document["current"].as<JsonArrayConst>();
    JsonArrayConst next = document["next"].as<JsonArrayConst>();
    const char *zone = document["zone"].as<const char *>();
    const char *source = document["source"].as<const char *>();

    if (current.isNull() || current.size() < 1 || next.isNull() || zone == NULL || source == NULL ||
        !read_u32(document["generated"], &schedule->generated_utc) ||
        !read_u32(document["valid_until"], &schedule->refresh_utc) ||
        next.size() > SERVER_ENTRIES_MAX)
    {
        return TZ_PARSE_FIELD;
    }
    result = read_entry(current[0], &schedule->entries[0]);
    if (result != TZ_PARSE_OK)
    {
        return result;
    }
    schedule->count = 1;
    future = next.size();
    previous = schedule->entries[0].utc_seconds;
    for (index = 0; index < future; index++)
    {
        result = read_entry(next[index], &entry);
        if (result != TZ_PARSE_OK)
        {
            return result;
        }
        if (entry.utc_seconds <= previous)
        {
            return TZ_PARSE_ORDER;
        }
        previous = entry.utc_seconds;
        if (index < SML_TZ_TRANSITION_MAX)
        {
            schedule->entries[schedule->count++] = entry;
        }
        else if (index == SML_TZ_TRANSITION_MAX)
        {
            schedule->horizon_end_utc = entry.utc_seconds; // the first one left out
        }
    }
    if (future < SML_TZ_TRANSITION_MAX)
    {
        schedule->horizon_end_utc = 0xFFFFFFFFUL;
    }
    else if (future == SML_TZ_TRANSITION_MAX)
    {
        schedule->horizon_end_utc = schedule->entries[schedule->count - 1].utc_seconds + 1;
    }
    strlcpy(schedule->zone, zone, sizeof(schedule->zone));
    schedule->source = strcmp(source, "gnss") == 0    ? SML_TZ_SOURCE_GNSS
                       : strcmp(source, "geoip") == 0 ? SML_TZ_SOURCE_GEOIP
                                                      : SML_TZ_SOURCE_NONE;
    schedule->valid = schedule->source != SML_TZ_SOURCE_NONE;
    return schedule->valid ? TZ_PARSE_OK : TZ_PARSE_FIELD;
}


const char *tz_parse_name(TzParseResult result)
{
    static const char *const names[TZ_PARSE_COUNT] = {"ok",
                                                      "not JSON",
                                                      "server error",
                                                      "wrong version",
                                                      "missing field",
                                                      "offset out of range",
                                                      "transitions out of order"};

    return result < TZ_PARSE_COUNT ? names[result] : "?";
}


const TzEntry *tz_schedule_entry_at(const TzSchedule *schedule, uint32_t utc_seconds)
{
    uint8_t index = 0;

    while (index + 1 < schedule->count && schedule->entries[index + 1].utc_seconds <= utc_seconds)
    {
        index++;
    }
    return &schedule->entries[index];
}


int32_t tz_entry_offset(const TzEntry *entry)
{
    return entry->base_offset_s + (entry->dst_active ? entry->dst_offset_s : 0);
}


static void put_entry(const TzEntry *entry, SML_TZ_ENTRY *wire)
{
    wire->utc_seconds = entry->utc_seconds;
    wire->base_offset_s = entry->base_offset_s;
    wire->dst_offset_s = entry->dst_offset_s;
    wire->flags = entry->dst_active ? SML_TZ_ENTRY_DST_ACTIVE : 0;
    wire->reserved = 0;
}


void tz_schedule_state(const TzSchedule *schedule, bool have_now, uint32_t utc_now,
                       SML_TZ_STATE_PAYLOAD *state)
{
    uint8_t first = 0;
    uint8_t index;

    memset(state, 0, sizeof(*state));
    if (!schedule->valid || schedule->count == 0)
    {
        return; // VALID clear: the PIC keeps what it has
    }
    if (have_now)
    {
        first = (uint8_t)(tz_schedule_entry_at(schedule, utc_now) - schedule->entries);
    }
    state->flags = SML_TZ_VALID | (schedule->ambiguous ? SML_TZ_AMBIGUOUS : 0);
    state->source = schedule->source;
    state->issued_utc = schedule->generated_utc;
    state->horizon_end_utc = schedule->horizon_end_utc;
    state->subsecond_offset_ns = schedule->subsecond_offset_ns;
    put_entry(&schedule->entries[first], &state->current);
    for (index = (uint8_t)(first + 1); index < schedule->count; index++)
    {
        put_entry(&schedule->entries[index], &state->next[state->count++]);
    }
}


bool tz_schedule_same(const TzSchedule *a, const TzSchedule *b)
{
    uint8_t index;

    if (a->valid != b->valid || a->source != b->source || a->ambiguous != b->ambiguous ||
        a->count != b->count || a->horizon_end_utc != b->horizon_end_utc ||
        a->subsecond_offset_ns != b->subsecond_offset_ns || strcmp(a->zone, b->zone) != 0)
    {
        return false;
    }
    for (index = 0; index < a->count; index++)
    {
        const TzEntry *x = &a->entries[index];
        const TzEntry *y = &b->entries[index];

        if (x->utc_seconds != y->utc_seconds || x->base_offset_s != y->base_offset_s ||
            x->dst_offset_s != y->dst_offset_s || x->dst_active != y->dst_active)
        {
            return false;
        }
    }
    return true;
}
