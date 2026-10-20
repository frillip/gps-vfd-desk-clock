/*
 * Smol Clock V2+: disciplines the T2/T3 epoch timer, and with it the OC1
 * epoch output, to the best available time source, shows local time on the
 * tubes and reports every second to the ESP32 and the console.
 *
 * Where things are: the task list below and scheduler.h; source selection
 * and time of day in sync/ (sync/sync.h); the timing hardware and control
 * loop in timing/ (timing/timing_config.h); the GNSS receiver link in gnss/
 * (gnss/gnss.h); the ESP32 link in esp/ (esp/esp_link.h, SML protocol); the
 * tubes in ui/display.h and local time in ui/timezone.h.
 */
#include <stdbool.h>
#include <stdio.h>

#include "esp/esp_link.h"
#include "gnss/gnss.h"
#include "platform/system.h"
#include "scheduler.h"
#include "sync/sync.h"
#include "sync/sync_report.h"
#include "timing/timing.h"
#include "timing/timing_report.h"
#include "ui/console.h"
#include "ui/display.h"
#include "ui/timezone.h"

// Full diagnostics, once a minute each, in different epochs, late in the
// second: they use printf, which costs ~1.5 ms a call here (70-80 ms a
// report), so they stay clear of the epoch work and the GNSS burst that
// follows each navigation solution. The per-epoch lines use ui/text.h.
#define REPORT_SLOT            600U
#define FULL_REPORT_EPOCHS     60U
#define SCHEDULER_REPORT_EPOCH 15U // ... at this epoch of each minute
#define GNSS_REPORT_EPOCH      30U
#define SYNC_REPORT_EPOCH      45U

// ---- The task list --------------------------------------------------------
// Periodic tasks keep their phase on the epoch timeline; slot tasks run at a
// fixed point in every epoch. See scheduler.h.

// The UART2 ring holds ~44 ms of GNSS data at 115200, the UART1 ring ~22 ms
// of ESP data.
static SchedulerTask every_10ms = SCHEDULER_EVERY("10 ms", 10U);
// After the epoch pulse has closed the tube drivers' latch.
static SchedulerTask display_load = SCHEDULER_AT_SLOT("display load", DISPLAY_LOAD_SLOT);
// The full reports, clear of the epoch work and the GNSS burst.
static SchedulerTask minute_report = SCHEDULER_AT_SLOT("minute report", REPORT_SLOT);
// The epoch's work, started by timing_snapshot(): timed only.
static SchedulerTask epoch_work = SCHEDULER_SECTION("epoch");

// The GNSS receiver link, with the latest PPS edge for labelling.
static void service_gnss(void)
{
    GnssPpsReference reference;
    PpsEdge edge;

    pps_capture_gnss_edge(&edge);
    reference.sequence = edge.sequence;
    reference.observed_ms = edge.observed_ms;
    reference.valid = edge.valid;
    gnss_service(scheduler_now_ms(), reference);
}


// The time of day for the display, as the synchroniser keeps it.
static void prepare_display(void)
{
    const SyncStatus *status = sync_status();
    DisplayClock clock;

    clock.known = status->time.known;
    clock.synchronised = sync_is_synchronised();
    clock.utc_seconds = status->time.utc_seconds;
    clock.leap_second = status->time.leap_second;
    clock.leap = status->time.leap;
    display_prepare(&clock);
}


// Once per epoch, 1 ms after it: measure, steer, label the second, tell the
// ESP, render the next second, and report.
static void run_epoch(const TimingSnapshot *snapshot)
{
    SyncUpdate update;

    update = sync_update(snapshot, epoch_timer_ms());
    esp_link_epoch(snapshot, &update, epoch_timer_ms());
    prepare_display();

    //timing_report_epoch(snapshot, &update.loop);
    //gnss_print_edge(snapshot->capture.gnss_edge_sequence, epoch_timer_ms());
    //sync_report_epoch(&update);
}


// One of the full reports, by the epoch's place in the minute.
static void report_minute(void)
{
    uint32_t minute_epoch = scheduler_epoch() % FULL_REPORT_EPOCHS;

    if (minute_epoch == SCHEDULER_REPORT_EPOCH)
    {
        scheduler_print();
    }
    if (minute_epoch == GNSS_REPORT_EPOCH)
    {
        gnss_print(epoch_timer_ms());
    }
    if (minute_epoch == SYNC_REPORT_EPOCH)
    {
        sync_report_status();
        display_print();
        timezone_print(sync_status()->time.utc_seconds);
        esp_link_print();
    }
}


int main(void)
{
    PpsCaptureSync capture_sync;

    system_init();
    console_init(FCY);
    esp_link_init();
    printf("\033[2J\033[1;1H");

    sync_init();
    timing_init(&capture_sync);
    timing_report_sync(&capture_sync);
    gnss_init(FCY, epoch_timer_ms());
    timezone_init();
    display_init();
    scheduler_init();

    while (1)
    {
        TimingSnapshot snapshot;

        scheduler_pass();

        // Every pass. The status LED is lit for the 100 ms epoch pulse: a
        // heartbeat on the second. The epoch interrupt lights it, so its
        // rising edge also marks the interrupt's latency on a scope.
        STATUS_LED_SET(DISPLAY_LATCH_OC_GET_VALUE());

        // Every epoch, at the measurement slot (set by the T4 scheduler
        // interrupt, so the captures of the epoch are read together).
        if (timing_snapshot(&snapshot))
        {
            scheduler_section(&epoch_work);
            run_epoch(&snapshot);
        }

        if (scheduler_due(&every_10ms))
        {
            service_gnss();
            esp_link_service(scheduler_now_ms());
        }

        if (scheduler_due(&display_load))
        {
            display_service();
        }

        if (scheduler_due(&minute_report))
        {
            //report_minute();
        }
    }
}
