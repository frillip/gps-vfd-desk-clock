#include "gnss_detect.h"
#include "ubx_encode.h"
#include "../drivers/uart2.h"
#include "../platform/time_ms.h"
#include <stddef.h>
#include <string.h>

// A full TX ring still uses up an attempt, so detection always ends.
static void send_poll(GnssDetect *detection, uint32_t now_ms)
{
    uint8_t frame[UBX_POLL_FRAME_LENGTH];

    detection->attempt_count++;
    detection->last_poll_ms = now_ms;
    ubx_encode_poll(UBX_CLASS_MON, UBX_ID_MON_VER, frame);
    if (!uart2_tx_write(frame, sizeof(frame)))
    {
        detection->poll_rejections++;
    }
}


void gnss_detect_init(GnssDetect *detection)
{
    if (detection != NULL)
    {
        memset(detection, 0, sizeof(*detection));
    }
}


void gnss_detect_start(GnssDetect *detection, uint32_t now_ms)
{
    if (detection == NULL)
    {
        return;
    }
    gnss_detect_init(detection);
    detection->state = GNSS_DETECTION_PENDING;
    send_poll(detection, now_ms);
}


void gnss_detect_service(GnssDetect *detection, uint32_t now_ms)
{
    if (detection == NULL || detection->state != GNSS_DETECTION_PENDING ||
        !time_ms_has_elapsed(now_ms, detection->last_poll_ms, GNSS_DETECT_TIMEOUT_MS))
    {
        return;
    }
    if (detection->attempt_count < GNSS_DETECT_ATTEMPTS)
    {
        send_poll(detection, now_ms);
    }
    else
    {
        detection->state = GNSS_DETECTION_FAILED;
    }
}


bool gnss_detect_handle(GnssDetect *detection, uint8_t class_id, uint8_t message_id,
                        const uint8_t *payload, uint16_t length, uint32_t now_ms)
{
    if (detection == NULL || detection->state == GNSS_DETECTION_NOT_STARTED ||
        class_id != UBX_CLASS_MON || message_id != UBX_ID_MON_VER ||
        !ubx_decode_mon_ver(payload, length, &detection->identity))
    {
        return false;
    }
    detection->state = GNSS_DETECTION_CONFIRMED;
    detection->identified_ms = now_ms;
    return true;
}
