/*
 * Receiver identification at the current baud, by MON-VER poll.
 *
 * CONFIRMED records that the receiver answered, and who it is. It does not
 * describe ongoing link health; gnss.c tracks that from frame traffic.
 * A late reply also moves FAILED to CONFIRMED.
 */
#ifndef GNSS_DETECT_H
#define GNSS_DETECT_H
#include <stdint.h>
#include <stdbool.h>
#include "ubx_decode.h"

#define GNSS_DETECT_TIMEOUT_MS 500U
#define GNSS_DETECT_ATTEMPTS   3U

typedef enum
{
    GNSS_DETECTION_NOT_STARTED,
    GNSS_DETECTION_PENDING,
    GNSS_DETECTION_CONFIRMED,
    GNSS_DETECTION_FAILED
} GnssDetectionState;

typedef struct
{
    GnssDetectionState state;
    UbxMonVer identity;
    uint32_t last_poll_ms;
    uint32_t identified_ms;
    uint8_t attempt_count;
    uint8_t poll_rejections; // TX ring full when a poll was due
} GnssDetect;

void gnss_detect_init(GnssDetect *detection);
// The UART must already run at the baud to test. Sends the first poll.
void gnss_detect_start(GnssDetect *detection, uint32_t now_ms);
void gnss_detect_service(GnssDetect *detection, uint32_t now_ms);
// Returns true if the message was a MON-VER that confirmed detection.
bool gnss_detect_handle(GnssDetect *detection, uint8_t class_id, uint8_t message_id,
                        const uint8_t *payload, uint16_t length, uint32_t now_ms);

#endif
