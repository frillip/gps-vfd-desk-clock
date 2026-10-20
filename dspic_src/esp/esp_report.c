#include "esp_report.h"

#include <stddef.h>
#include <string.h>

#include "../drivers/tubes.h"
#include "../gnss/gnss.h"
#include "../platform/board.h"
#include "../platform/time_ms.h"
#include "../sync/sync_report.h"
#include "../timing/q16.h"
#include "../timing/timing_config.h"
#include "../ui/timezone.h"

#define NS_PER_TICK  25L // 1 / FCY
#define AGE_LIMIT_MS 0xFFFFUL

// ---- Mappings -------------------------------------------------------------

// Nothing processed yet counts as uninitialised whatever the state says.
uint8_t esp_report_lock(SyncState state, uint32_t epochs)
{
    static const uint8_t codes[] = {SML_LOCK_FREE_RUNNING, SML_LOCK_ACQUIRING, SML_LOCK_LOCKED,
                                    SML_LOCK_HOLDOVER};
    return epochs == 0U || (unsigned)state >= sizeof(codes) ? SML_LOCK_UNINITIALISED : codes[state];
}


uint8_t esp_report_sync(SyncState state, uint32_t epochs)
{
    static const uint8_t codes[] = {SML_SYNC_FREE_RUN, SML_SYNC_ACQUIRE, SML_SYNC_LOCKED,
                                    SML_SYNC_HOLDOVER};
    return epochs == 0U || (unsigned)state >= sizeof(codes) ? SML_SYNC_INIT : codes[state];
}


uint8_t esp_report_cause(uint8_t reason)
{
    static const uint8_t codes[] = {SML_CAUSE_QUALIFIED,     SML_CAUSE_TRACKING,
                                    SML_CAUSE_PHASE_LOST,    SML_CAUSE_SOURCE_LOST,
                                    SML_CAUSE_SOURCE_BACK,   SML_CAUSE_ALTERNATIVE,
                                    SML_CAUSE_BETTER_SOURCE, SML_CAUSE_EXPIRED};
    return reason < sizeof(codes) ? codes[reason] : SML_CAUSE_NONE;
}


uint8_t esp_report_source(uint8_t source, uint32_t epochs)
{
    static const uint8_t codes[SYNC_SOURCE_COUNT] = {SML_SOURCE_GNSS, SML_SOURCE_NTP};
    if (epochs == 0U)
    {
        return SML_SOURCE_INIT;
    }
    return source < SYNC_SOURCE_COUNT ? codes[source] : SML_SOURCE_NONE;
}


uint8_t esp_report_control(EpochSyncState state)
{
    static const uint8_t codes[] = {SML_CONTROL_ACQUIRE, SML_CONTROL_STEP, SML_CONTROL_TRACK};
    return (unsigned)state < sizeof(codes) ? codes[state] : SML_CONTROL_INIT;
}


uint8_t esp_report_display(DisplayMode mode, bool time_known)
{
    static const uint8_t codes[DISPLAY_MODE_COUNT] = {SML_DISPLAY_HHMM};
    if (!time_known)
    {
        return SML_DISPLAY_DASHES;
    }
    return (unsigned)mode < DISPLAY_MODE_COUNT ? codes[mode] : SML_DISPLAY_INIT;
}


static uint16_t age_ms(uint32_t now_ms, uint32_t then_ms)
{
    uint32_t age = time_ms_elapsed(now_ms, then_ms);
    return (uint16_t)(age > AGE_LIMIT_MS ? AGE_LIMIT_MS : age);
}


// ---- Payloads -------------------------------------------------------------

void esp_report_time_auth(SML_TIME_AUTH_PAYLOAD *payload)
{
    const SyncStatus *status = sync_status();
    uint32_t uncertainty_ns;

    memset(payload, 0, sizeof(*payload));
    payload->source = esp_report_source(status->source, status->epochs);
    payload->lock_state = esp_report_lock(status->state, status->epochs);
    if (status->source < SYNC_SOURCE_COUNT)
    {
        payload->flags |= SML_AUTH_SOURCE_VALID;
    }
    if (!status->time.known)
    {
        return;
    }
    // An inserted leap second is sent as 23:59:59 again (Unix seconds).
    payload->flags |= SML_AUTH_UTC_VALID;
    payload->utc_seconds = status->time.utc_seconds;
    if (status->phase_valid && status->state != SYNC_HOLDOVER)
    {
        payload->utc_phase_ns = status->phase_ticks * NS_PER_TICK;
    }
    if (sync_uncertainty(&uncertainty_ns))
    {
        payload->flags |= SML_AUTH_UNCERTAINTY_VALID;
        payload->uncertainty_ns = uncertainty_ns;
    }
}


void esp_report_time_pic(const TimingSnapshot *snapshot, const SyncUpdate *update,
                         SML_TIME_PIC_PAYLOAD *payload)
{
    const SyncStatus *status = sync_status();
    const SyncTransition *transition = sync_last_transition();
    const EpochControl *control = epoch_control_state();

    memset(payload, 0, sizeof(*payload));
    payload->source = esp_report_source(status->source, status->epochs);
    payload->lock_state = esp_report_lock(status->state, status->epochs);
    payload->sync_state = esp_report_sync(status->state, status->epochs);
    if (transition != NULL)
    {
        payload->previous_sync_state = esp_report_sync(transition->from, 1U);
        payload->last_sync_cause = esp_report_cause(transition->reason);
    }
    payload->epoch_sequence = snapshot->timer.sequence;
    if (update->loop.phase_valid)
    {
        payload->flags |= SML_PIC_PHASE_VALID;
        payload->phase_cycles = update->loop.utc_offset_ticks;
    }
    if (snapshot->capture.reference_period_valid)
    {
        payload->flags |= SML_PIC_PERIOD_VALID;
        payload->pps_period_cycles = snapshot->capture.reference_period;
    }
    if (control->pps_kf.initialized)
    {
        // The timer clock as measured against the reference: FCY ticks per
        // reference second (fosc_hz keeps its V2 meaning, FCY not Fosc).
        payload->flags |= SML_PIC_FREQUENCY_VALID;
        payload->fosc_hz = control->filtered_pps_period;
    }
    if (snapshot->capture.reference_capture_valid)
    {
        payload->flags |= SML_PIC_PPS_VALID;
    }
    payload->applied_adjustment_cycles =
        (int32_t)(control->commanded_period - (uint32_t)TIMING_NOMINAL_TICKS);
    payload->missing_pps_events = status->sources[SYNC_SOURCE_GNSS].verdicts[SYNC_CHECK_NO_EDGE];
    payload->sync_events = status->transitions;
    payload->reference_age_ms =
        (uint16_t)(control->holdover_epochs > 65U ? AGE_LIMIT_MS
                                                  : control->holdover_epochs * 1000U);
}


void esp_report_time_ctrl(const TimingSnapshot *snapshot, const SyncUpdate *update,
                          SML_TIME_CTRL_PAYLOAD *payload)
{
    static int32_t last_prediction_q16;
    static bool has_prediction;
    static uint32_t last_gate_rejections;
    const EpochControl *control = epoch_control_state();
    const EpochPiServo *pi = &control->pi;
    const KalmanFilter *kalman = &control->pps_kf;

    memset(payload, 0, sizeof(*payload));
    payload->controller_state = esp_report_control(control->state);
    payload->epoch_sequence = snapshot->timer.sequence;
    payload->nominal_period_ticks = TIMING_NOMINAL_TICKS;
    if (kalman->initialized)
    {
        payload->flags |= SML_CTRL_ESTIMATE_VALID;
        payload->frequency_offset_q16 = kalman->offset_q16;
        payload->frequency_variance_q30 = kalman->variance_ratio_q30;
        payload->measurement_variance_q16 = kalman->measurement_variance_q16;
        payload->kalman_gain_q30 = kalman->gain_q30;
        payload->predicted_phase_q16 = control->predicted_phase_q16;
    }
    if (pi->enabled)
    {
        payload->flags |= SML_CTRL_PI_ENABLED;
        if (pi->correction_q16 >= pi->correction_limit_q16 ||
            pi->correction_q16 <= -pi->correction_limit_q16)
        {
            payload->flags |= SML_CTRL_SATURATED;
        }
    }
    if (update->loop.kind == EPOCH_UPDATE_MEASURED)
    {
        payload->flags |= SML_CTRL_MEASUREMENT_USED;
    }
    if (control->gate.rejected != last_gate_rejections)
    {
        payload->flags |= SML_CTRL_GATE_REJECTED;
    }
    last_gate_rejections = control->gate.rejected;
    // Measured phase minus the phase predicted for this epoch one epoch ago.
    if (update->loop.phase_valid && has_prediction)
    {
        payload->phase_innovation_q16 =
            Q16_FROM_TICKS(q16_clamp(update->loop.utc_offset_ticks, Q16_TICK_LIMIT)) -
            last_prediction_q16;
    }
    last_prediction_q16 = control->predicted_phase_q16;
    has_prediction = kalman->initialized;
    payload->pi_proportional_q16 = pi->proportional_q16;
    payload->pi_integral_q16 = pi->integral_q16;
    payload->output_offset_q16 = control->commanded_offset_q16;
    payload->gate_rejections = control->gate.rejected;
    payload->kalman_rejections = kalman->stats.rejected;
}


void esp_report_time_meas(const TimingSnapshot *snapshot, const SyncUpdate *update, uint32_t now_ms,
                          SML_TIME_MEAS_PAYLOAD *payload)
{
    static bool has_previous;
    static uint32_t previous_running_period;
    static uint32_t previous_report_ms;
    const SyncStatus *status = sync_status();
    const PpsCaptureSample *capture = &snapshot->capture;
    const UbxTimTp *pulse;
    int32_t mark_ns;
    uint32_t mark_accuracy_ns;

    memset(payload, 0, sizeof(*payload));
    payload->source = esp_report_source(status->source, status->epochs);
    payload->controller_state = esp_report_control(epoch_control_state()->state);
    payload->epoch_sequence = snapshot->timer.sequence;
    if (status->time.known)
    {
        payload->flags |= SML_MEAS_LABEL_VALID;
        payload->utc_seconds = status->time.utc_seconds;
    }
    payload->reference_sequence = capture->reference_edge_sequence;
    if (update->loop.phase_valid)
    {
        payload->flags |= SML_MEAS_PHASE_VALID;
        payload->phase_ticks = update->loop.utc_offset_ticks;
    }
    if (capture->reference_capture_valid && capture->reference_period_valid)
    {
        payload->flags |= SML_MEAS_PERIOD_VALID;
        payload->reference_period_ticks = capture->reference_period;
    }
    if (has_previous)
    {
        // The period loaded at the previous epoch is the second just ended.
        payload->flags |= SML_MEAS_EPOCH_PERIOD_VALID;
        payload->epoch_period_ticks = previous_running_period;
    }
    if (update->loop.kind == EPOCH_UPDATE_MEASURED)
    {
        payload->flags |= SML_MEAS_MEASUREMENT_USED;
    }
    // The receiver's quantisation error is for its own edge: only while that
    // is the one captured.
    if (capture->reference_capture_valid && capture->reference_input == PPS_INPUT_GNSS &&
        gnss_time_pulse(snapshot->monitor.inputs[PPS_INPUT_GNSS].sequence, &pulse))
    {
        payload->flags |= SML_MEAS_QERR_VALID;
        payload->qerr_ps = pulse->quantisation_error_ps;
    }
    // A mark received during the second just ended describes the epoch that
    // began it: each is reported once, against epoch_sequence - 1.
    if (has_previous && gnss_mark_offset(now_ms, &mark_ns, &mark_accuracy_ns) &&
        time_ms_elapsed(now_ms, gnss_state()->time_mark_received_ms) <
            time_ms_elapsed(now_ms, previous_report_ms))
    {
        payload->flags |= SML_MEAS_MARK_VALID;
        payload->mark_offset_ns = mark_ns;
        payload->mark_accuracy_ns = mark_accuracy_ns;
    }
    has_previous = true;
    previous_running_period = snapshot->timer.running_period;
    previous_report_ms = now_ms;
}


void esp_report_time_gnss(uint32_t now_ms, SML_TIME_GNSS_PAYLOAD *payload)
{
    const GnssState *gnss = gnss_state();
    const GnssEpoch *epoch = gnss_epoch_latest(&gnss->epochs);
    UtcLeap leap;
    int8_t gps_minus_utc;

    memset(payload, 0, sizeof(*payload));
    if (gnss->detection.state == GNSS_DETECTION_CONFIRMED)
    {
        payload->flags |= SML_GNSS_DETECTED;
    }
    if (epoch != NULL)
    {
        payload->fix_type = epoch->status.fix_type;
        payload->utc_standard =
            (uint8_t)((epoch->time_utc.validity_flags & UBX_TIMEUTC_STANDARD_MASK) >>
                      UBX_TIMEUTC_STANDARD_SHIFT);
        payload->utc_age_ms = age_ms(now_ms, epoch->completed_ms);
        if (epoch->fix_ok)
        {
            payload->flags |= SML_GNSS_FIX_OK;
        }
        if (epoch->time_valid)
        {
            payload->flags |= SML_GNSS_UTC_VALID;
            payload->utc_seconds = epoch->utc_seconds;
            payload->utc_accuracy_ns = epoch->time_utc.time_accuracy_ns;
        }
        if (epoch->pps_associated)
        {
            payload->flags |= SML_GNSS_PPS_VALID;
            payload->pps_sequence = epoch->pps_sequence;
            payload->pps_age_ms = payload->utc_age_ms;
        }
        if (epoch->has_position && epoch->fix_ok)
        {
            const UbxNavPosllh *position = &epoch->position;
            payload->flags |= SML_GNSS_POSITION_VALID;
            payload->position_age_ms = payload->utc_age_ms;
            payload->latitude_e7 = position->latitude_e7;
            payload->longitude_e7 = position->longitude_e7;
            payload->height_mm = position->height_ellipsoid_mm;
            payload->height_msl_mm = position->height_msl_mm;
            payload->horizontal_accuracy_mm = position->horizontal_accuracy_mm;
            payload->vertical_accuracy_mm = position->vertical_accuracy_mm;
        }
    }
    if (gnss_leap_seconds(now_ms, &gps_minus_utc))
    {
        payload->flags |= SML_GNSS_LEAP_VALID;
        payload->gps_utc_s = gps_minus_utc;
    }
    if (gnss_leap_event(now_ms, &leap))
    {
        payload->flags |= SML_GNSS_LEAP_EVENT_VALID;
        payload->leap_change = leap.change;
        payload->leap_event_utc = leap.event_utc;
    }
}


void esp_report_stat_ui(SML_STAT_UI_PAYLOAD *payload)
{
    const SyncTime *time = &sync_status()->time;
    TimezoneOffset zone;

    memset(payload, 0, sizeof(*payload));
    payload->current_display = esp_report_display(display_mode(), time->known);
    payload->selected_display = esp_report_display(display_mode(), true);
    payload->menu_state = SML_MENU_NONE;
    payload->button_state = SML_BUTTON_NONE;
    payload->brightness = tubes_brightness();
    payload->brightness_target = tubes_brightness();
    if (tubes_brightness() != TUBES_BRIGHTNESS_OFF)
    {
        payload->flags |= SML_UI_POWER_ON;
    }
    if (UI_SWITCH_IS_ON())
    {
        payload->flags |= SML_UI_SWITCH;
    }
    if (UI_BUTTON_IS_PRESSED())
    {
        payload->flags |= SML_UI_BUTTON;
    }
    timezone_offset_at(time->utc_seconds, &zone);
    payload->base_offset_s = zone.base_offset_s;
    payload->dst_offset_s = zone.dst_offset_s;
    if (zone.dst_active)
    {
        payload->flags |= SML_UI_DST_ACTIVE;
    }
    if (zone.stale)
    {
        payload->flags |= SML_UI_TZ_STALE;
    }
    if (zone.ambiguous)
    {
        payload->flags |= SML_UI_TZ_AMBIGUOUS;
    }
    if (timezone_schedule()->held)
    {
        payload->flags |= SML_UI_TZ_HELD;
    }
    if (time->known)
    {
        payload->flags |= SML_UI_LOCAL_VALID;
        payload->local_seconds = time->utc_seconds + (uint32_t)zone.offset_s;
    }
}


void esp_report_print_all(uint32_t now_ms)
{
    const SyncStatus *status = sync_status();

    sync_report_status();
    gnss_print(now_ms);
    display_print();
    timezone_print(status->time.utc_seconds);
}
