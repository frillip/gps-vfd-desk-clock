#include "gnss_epoch.h"
#include "../platform/time_ms.h"
#include "../platform/utc.h"
#include <stddef.h>
#include <string.h>

#define EPOCH_PART_STATUS    0x01U
#define EPOCH_PART_TIMEUTC   0x02U
#define EPOCH_PARTS_REQUIRED (EPOCH_PART_STATUS | EPOCH_PART_TIMEUTC)

#define NANOSECONDS_PER_SECOND 1000000000L
#define HALF_SECOND_NS         500000000L
#define TIMEUTC_FULLY_VALID    (UBX_TIMEUTC_VALID_TOW | UBX_TIMEUTC_VALID_WEEK | UBX_TIMEUTC_VALID_UTC)

static bool calendar_is_plausible(const UbxNavTimeutc *time_utc)
{
    // The receiver reports year 1980..2099 before it knows the date; a clock
    // built in the 2020s rejects anything earlier as unset. Unix seconds are
    // 32-bit: up to 2105, so every second of the year is representable.
    UtcCivil civil;

    civil.year = time_utc->year;
    civil.month = time_utc->month;
    civil.day = time_utc->day;
    civil.hour = time_utc->hour;
    civil.minute = time_utc->minute;
    civil.second = time_utc->second;
    return time_utc->year >= 2020U && time_utc->year < UTC_LAST_YEAR && utc_civil_is_valid(&civil);
}


static bool fix_is_usable(const UbxNavStatus *status)
{
    bool fix_type_ok = status->fix_type == UBX_FIX_2D || status->fix_type == UBX_FIX_3D ||
                       status->fix_type == UBX_FIX_GNSS_DR || status->fix_type == UBX_FIX_TIME_ONLY;
    return fix_type_ok && (status->flags & UBX_STATUS_FIX_OK) != 0U;
}


// ---- Epoch completion ----------------------------------------------------

// Fills the derived time fields from the epoch's NAV-STATUS and NAV-TIMEUTC.
static void derive_time(GnssEpoch *epoch)
{
    const UbxNavTimeutc *time_utc = &epoch->time_utc;
    int32_t offset_ns = time_utc->fractional_second_ns;
    UtcCivil civil;
    uint32_t whole_second;
    bool utc_valid = (time_utc->validity_flags & TIMEUTC_FULLY_VALID) == TIMEUTC_FULLY_VALID;
    bool aligned;

    epoch->fix_ok = fix_is_usable(&epoch->status);
    epoch->leap_second = time_utc->second == 60U;
    if (!calendar_is_plausible(time_utc))
    {
        epoch->utc_seconds = 0;
        epoch->offset_from_second_ns = 0;
        epoch->time_valid = false;
        return;
    }

    // A leap second has no Unix representation: 23:59:60 is held as :59 and
    // flagged, so the Unix count repeats one second across the event.
    civil.year = time_utc->year;
    civil.month = time_utc->month;
    civil.day = time_utc->day;
    civil.hour = time_utc->hour;
    civil.minute = time_utc->minute;
    civil.second = time_utc->second; // 60 counts as 59
    whole_second = utc_from_civil(&civil);
    // Round the epoch time (calendar + fraction) to the nearest second.
    if (offset_ns >= HALF_SECOND_NS)
    {
        whole_second += 1U;
        offset_ns -= NANOSECONDS_PER_SECOND;
    }
    else if (offset_ns < -HALF_SECOND_NS)
    {
        whole_second -= 1U;
        offset_ns += NANOSECONDS_PER_SECOND;
    }
    aligned =
        offset_ns <= GNSS_EPOCH_ALIGNMENT_LIMIT_NS && offset_ns >= -GNSS_EPOCH_ALIGNMENT_LIMIT_NS;

    epoch->utc_seconds = whole_second;
    epoch->offset_from_second_ns = offset_ns;
    epoch->time_valid = epoch->fix_ok && utc_valid && aligned;
}


static void associate_pps(GnssEpochAssembler *assembler, GnssEpoch *epoch, uint32_t now_ms,
                          GnssPpsReference pps)
{
    bool already_labelled =
        assembler->has_labelled_pps && assembler->last_labelled_pps_sequence == pps.sequence;
    bool recent = time_ms_elapsed(now_ms, pps.observed_ms) < GNSS_PPS_ASSOCIATION_WINDOW_MS;

    epoch->pps_associated = pps.valid && recent && !already_labelled;
    epoch->pps_sequence = pps.sequence;
    if (epoch->pps_associated)
    {
        assembler->last_labelled_pps_sequence = pps.sequence;
        assembler->has_labelled_pps = true;
    }
}


static void publish(GnssEpochAssembler *assembler, uint32_t now_ms, GnssPpsReference pps)
{
    GnssEpoch *epoch = &assembler->building;

    epoch->completed_ms = now_ms;
    derive_time(epoch);
    associate_pps(assembler, epoch, now_ms, pps);

    assembler->published = *epoch;
    assembler->has_published = true;
    assembler->building_active = false;
    assembler->building_parts = 0;

    assembler->stats.published++;
    if (!epoch->pps_associated)
    {
        assembler->stats.unassociated++;
    }
    if (!epoch->time_valid)
    {
        assembler->stats.invalid_time++;
    }
}


// ---- Grouping by time of week -------------------------------------------
static bool is_published_epoch(const GnssEpochAssembler *assembler, uint32_t time_of_week_ms)
{
    return assembler->has_published && assembler->published.time_of_week_ms == time_of_week_ms;
}


// Returns the epoch under construction for this time of week, starting a
// new one (and abandoning any incomplete predecessor) if needed.
static GnssEpoch *building_epoch(GnssEpochAssembler *assembler, uint32_t time_of_week_ms)
{
    if (assembler->building_active && assembler->building.time_of_week_ms == time_of_week_ms)
    {
        return &assembler->building;
    }
    if (assembler->building_active && assembler->building_parts != 0U)
    {
        assembler->stats.abandoned++;
    }
    memset(&assembler->building, 0, sizeof(assembler->building));
    assembler->building.time_of_week_ms = time_of_week_ms;
    assembler->building_parts = 0;
    assembler->building_active = true;
    return &assembler->building;
}


// Records one of the required parts and publishes once both are present.
static bool add_required_part(GnssEpochAssembler *assembler, uint8_t part, uint32_t now_ms,
                              GnssPpsReference pps)
{
    assembler->building_parts |= part;
    if ((assembler->building_parts & EPOCH_PARTS_REQUIRED) == EPOCH_PARTS_REQUIRED)
    {
        publish(assembler, now_ms, pps);
        return true;
    }
    return false;
}


// ---- Public interface ----------------------------------------------------
void gnss_epoch_init(GnssEpochAssembler *assembler)
{
    if (assembler != NULL)
    {
        memset(assembler, 0, sizeof(*assembler));
    }
}


bool gnss_epoch_add_status(GnssEpochAssembler *assembler, const UbxNavStatus *status,
                           uint32_t now_ms, GnssPpsReference pps)
{
    GnssEpoch *epoch;

    if (assembler == NULL || status == NULL ||
        is_published_epoch(assembler, status->time_of_week_ms))
    {
        return false; // duplicate for an epoch already published
    }
    epoch = building_epoch(assembler, status->time_of_week_ms);
    epoch->status = *status;
    return add_required_part(assembler, EPOCH_PART_STATUS, now_ms, pps);
}


bool gnss_epoch_add_time_utc(GnssEpochAssembler *assembler, const UbxNavTimeutc *time_utc,
                             uint32_t now_ms, GnssPpsReference pps)
{
    GnssEpoch *epoch;

    if (assembler == NULL || time_utc == NULL ||
        is_published_epoch(assembler, time_utc->time_of_week_ms))
    {
        return false;
    }
    epoch = building_epoch(assembler, time_utc->time_of_week_ms);
    epoch->time_utc = *time_utc;
    return add_required_part(assembler, EPOCH_PART_TIMEUTC, now_ms, pps);
}


void gnss_epoch_add_clock(GnssEpochAssembler *assembler, const UbxNavClock *clock)
{
    GnssEpoch *epoch;

    if (assembler == NULL || clock == NULL)
    {
        return;
    }
    epoch = is_published_epoch(assembler, clock->time_of_week_ms)
                ? &assembler->published
                : building_epoch(assembler, clock->time_of_week_ms);
    epoch->clock = *clock;
    epoch->has_clock = true;
}


void gnss_epoch_add_position(GnssEpochAssembler *assembler, const UbxNavPosllh *position)
{
    GnssEpoch *epoch;

    if (assembler == NULL || position == NULL)
    {
        return;
    }
    epoch = is_published_epoch(assembler, position->time_of_week_ms)
                ? &assembler->published
                : building_epoch(assembler, position->time_of_week_ms);
    epoch->position = *position;
    epoch->has_position = true;
}


void gnss_epoch_end(GnssEpochAssembler *assembler, const UbxNavEoe *end)
{
    if (assembler == NULL || end == NULL)
    {
        return;
    }
    if (is_published_epoch(assembler, end->time_of_week_ms))
    {
        assembler->published.ended = true;
    }
    else if (assembler->building_active &&
             assembler->building.time_of_week_ms == end->time_of_week_ms)
    {
        // The required parts can no longer arrive.
        if (assembler->building_parts != 0U)
        {
            assembler->stats.abandoned++;
        }
        assembler->building_active = false;
        assembler->building_parts = 0;
    }
}


const GnssEpoch *gnss_epoch_latest(const GnssEpochAssembler *assembler)
{
    return (assembler != NULL && assembler->has_published) ? &assembler->published : NULL;
}
