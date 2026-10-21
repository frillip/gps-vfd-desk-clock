#include "leap.h"

#include <string.h>

#include "../pic/pic_link.h"
#include "../timing/esp_time.h"
#include "../util/time_text.h"

#define RESEND_MS 600000U

static SML_TIME_LEAP_PAYLOAD held;
static bool changed;
static uint32_t last_sent_ms;
static uint32_t sent;

// An event that has passed becomes the count in force.
static bool roll_on(SML_TIME_LEAP_PAYLOAD *leap)
{
    uint32_t utc;

    if ((leap->flags & SML_LEAP_EVENT_PENDING) == 0 || !esp_time_utc_now(&utc) ||
        utc < leap->event_utc)
    {
        return false;
    }
    leap->tai_utc_s = (int16_t)(leap->tai_utc_s + leap->change);
    leap->flags &= (uint16_t)~SML_LEAP_EVENT_PENDING;
    leap->change = 0;
    leap->event_utc = 0;
    return true;
}


static void take_gnss(void)
{
    SML_TIME_GNSS_PAYLOAD gnss;
    SML_TIME_LEAP_PAYLOAD leap;

    if (!pic_link_get(SML_CLASS_TIME, SML_TIME_GNSS, &gnss, PIC_LINK_STATE_TIMEOUT_MS) ||
        (gnss.flags & SML_GNSS_LEAP_VALID) == 0)
    {
        return;
    }
    memset(&leap, 0, sizeof(leap));
    leap.flags = SML_LEAP_VALID;
    leap.source = SML_LEAP_SOURCE_GNSS;
    leap.tai_utc_s = (int16_t)(gnss.gps_utc_s + 19);
    if ((gnss.flags & SML_GNSS_LEAP_EVENT_VALID) && gnss.leap_change != 0)
    {
        leap.flags |= SML_LEAP_EVENT_PENDING;
        leap.change = gnss.leap_change;
        leap.event_utc = gnss.leap_event_utc;
    }
    roll_on(&leap); // the receiver may still announce an event just past
    if (memcmp(&leap, &held, sizeof(leap)) != 0)
    {
        held = leap;
        changed = true;
    }
}


void leap_service(void)
{
    take_gnss();
    if (roll_on(&held))
    {
        changed = true;
    }
    if ((held.flags & SML_LEAP_VALID) == 0 || !pic_link_is_sml())
    {
        return;
    }
    if (changed || millis() - last_sent_ms > RESEND_MS)
    {
        pic_link_send(SML_CLASS_TIME, SML_TIME_LEAP, &held);
        changed = false;
        last_sent_ms = millis();
        sent++;
    }
}


void leap_print(Stream *output, bool verbose)
{
    output->printf("Leap:     ");
    if ((held.flags & SML_LEAP_VALID) == 0)
    {
        output->printf("unknown (no GNSS leap data yet)\n");
        return;
    }
    output->printf("TAI-UTC %d s (GNSS)", held.tai_utc_s);
    if (held.flags & SML_LEAP_EVENT_PENDING)
    {
        output->printf(", %+d at ", held.change);
        time_print_utc(output, held.event_utc);
    }
    if (verbose)
    {
        output->printf(", %lu sent", (unsigned long)sent);
    }
    output->printf("\n");
}
