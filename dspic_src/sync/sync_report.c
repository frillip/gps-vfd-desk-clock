#include "sync_report.h"

#include <stdio.h>

#include "../ui/text.h"

static void print_time(const SyncTime *time)
{
    UtcCivil civil;

    if (!time->known)
    {
        printf("time unknown");
        return;
    }
    utc_to_civil(time->utc_seconds, time->leap_second, &civil);
    printf("%04u-%02u-%02uT%02u:%02u:%02uZ", civil.year, civil.month, civil.day, civil.hour,
           civil.minute, civil.second);
}


static void print_transition(const SyncTransition *transition)
{
    printf("epoch %lu: %s -> %s, %s (%s", (unsigned long)transition->epoch,
           sync_state_name(transition->from), sync_state_name(transition->to),
           sync_source_name(transition->source), sync_reason_name(transition->reason));
    if (transition->reason == SYNC_REASON_SOURCE_LOST)
    {
        printf(": %s", sync_source_check_name((SyncCheck)transition->verdict));
    }
    printf(")");
}


// The date and time as print_time() gives it.
static void text_time(TextLine *line, const SyncTime *time)
{
    UtcCivil civil;

    if (!time->known)
    {
        text_str(line, "time unknown");
        return;
    }
    utc_to_civil(time->utc_seconds, time->leap_second, &civil);
    text_u32(line, civil.year, 4, '0');
    text_char(line, '-');
    text_u32(line, civil.month, 2, '0');
    text_char(line, '-');
    text_u32(line, civil.day, 2, '0');
    text_char(line, 'T');
    text_u32(line, civil.hour, 2, '0');
    text_char(line, ':');
    text_u32(line, civil.minute, 2, '0');
    text_char(line, ':');
    text_u32(line, civil.second, 2, '0');
    text_char(line, 'Z');
}


void sync_report_epoch(const SyncUpdate *update)
{
    const SyncStatus *status = sync_status();
    TextLine line;

    // A state change is rare: printf will do.
    if (update->transitioned)
    {
        printf("      SYNC *** ");
        print_transition(sync_last_transition());
        printf("\r\n");
    }
    text_start(&line);
    text_str(&line, "      SYNC: ");
    text_str(&line, sync_state_name(status->state));
    text_char(&line, ' ');
    text_str(&line, sync_source_name(status->source));
    text_char(&line, ' ');
    text_time(&line, &status->time);
    if (status->state == SYNC_HOLDOVER)
    {
        text_str(&line, " holdover ");
        text_u32(&line, status->holdover_epochs, 0, ' ');
        text_char(&line, 's');
    }
    if (status->source < SYNC_SOURCE_COUNT)
    {
        const SyncSourceTracker *tracker = &status->sources[status->source];
        text_char(&line, ' ');
        text_str(&line, sync_source_check_name(tracker->check));
        if (tracker->report.uncertainty_ns != 0U)
        {
            text_str(&line, " unc ");
            text_u32(&line, tracker->report.uncertainty_ns, 0, ' ');
            text_str(&line, "ns");
        }
    }
    text_str(&line, update->usable ? "\r\n" : " (not steering)\r\n");
    text_send(&line);
}


static void print_source(uint8_t index, const SyncSourceTracker *tracker)
{
    uint8_t check;

    printf("%-5s %-12s run %u, last good epoch %lu, unc %luns, label jumps %lu\r\n",
           sync_source_name(index), sync_source_check_name(tracker->check),
           (unsigned)tracker->good_run, (unsigned long)tracker->last_good_epoch,
           (unsigned long)tracker->report.uncertainty_ns, (unsigned long)tracker->label_jumps);
    printf("      verdicts:");
    for (check = 0; check < (uint8_t)SYNC_CHECK_COUNT; check++)
    {
        if (tracker->verdicts[check] != 0U)
        {
            printf(" %s %lu", sync_source_check_name((SyncCheck)check),
                   (unsigned long)tracker->verdicts[check]);
        }
    }
    printf("\r\n");
    if (tracker->cross_checks != 0U)
    {
        printf("      cross-check: %lu, %+ld..%+ldns, latest %+ldns\r\n",
               (unsigned long)tracker->cross_checks, (long)tracker->cross_check_min_ns,
               (long)tracker->cross_check_max_ns, (long)tracker->report.cross_check_ns);
    }
}


void sync_report_status(void)
{
    const SyncStatus *status = sync_status();
    uint32_t first;
    uint32_t index;
    uint8_t source;

    printf("\r\n=== SYNC ===\r\n");
    printf("State: %s on %s for %lus, %s\r\n", sync_state_name(status->state),
           sync_source_name(status->source), (unsigned long)status->state_epochs,
           sync_is_synchronised() ? "synchronised" : "not synchronised");
    printf("Time: ");
    print_time(&status->time);
    printf(", %lu corrections", (unsigned long)status->time.corrections);
    if (status->time.leap.change != 0)
    {
        UtcCivil civil;
        utc_to_civil(status->time.leap.event_utc - 1, false, &civil);
        printf(", leap second: %04u-%02u-%02uT%s", civil.year, civil.month, civil.day,
               status->time.leap.change > 0 ? "23:59:60Z inserted" : "23:59:59Z deleted");
    }
    printf("\r\n");
    for (source = 0; source < SYNC_SOURCE_COUNT; source++)
    {
        print_source(source, &status->sources[source]);
    }

    printf("Transitions: %lu\r\n", (unsigned long)status->transitions);
    first =
        status->transitions > SYNC_HISTORY_LENGTH ? status->transitions - SYNC_HISTORY_LENGTH : 0U;
    for (index = first; index < status->transitions; index++)
    {
        printf("  ");
        print_transition(&status->history[index % SYNC_HISTORY_LENGTH]);
        printf("\r\n");
    }
}
