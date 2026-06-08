#include "acoustic_runtime.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "audio_stream_queue.h"
#include "device_clock.h"
#include "device_runtime_config.h"
#include "microphone.h"
#include "network_runtime.h"
#include "pico/stdlib.h"
#include "runtime_status.h"
#include "sound_event_uploader.h"

static void on_microphone_buffer_ready(const int32_t *buffer, size_t words, uint64_t buffer_ready_us)
{
    audio_stream_queue_push_from_buffer(buffer, words, buffer_ready_us);
}

static void upload_sound_event(const device_config_t *config,
                               const acoustic_runtime_t *runtime,
                               sd_card_buffer_t *sd_card_buffer)
{
    if (runtime == NULL)
    {
        return;
    }

    sound_event_upload_t event = {
        .event_time_ns = runtime->event_time_ns,
        .peak_abs = runtime->peak_abs,
        .mean_abs = runtime->mean_abs,
        .noise_floor_abs = runtime->noise_floor_abs,
    };

    if (!sound_event_uploader_upload(config,
                                     sd_card_buffer,
                                     &event,
                                     DEVICE_SOUND_EVENT_UPLOAD_TIMEOUT_MS))
    {
        printf("[Upload] Sound event upload failed\n");
    }
}

static void log_detected_peak(const sound_event_detector_result_t *detection)
{
    if (detection == NULL)
    {
        return;
    }

    if (detection->matched_center_hz > 0u)
    {
        printf("[Audio] Peak registered: peak=%u mean=%u noise=%lu freq=%uHz energy=%lu baseline=%lu ratio=%lu.%02lux threshold=%u.%02ux\n",
               (unsigned int)detection->peak_abs,
               (unsigned int)detection->mean_abs,
               (unsigned long)detection->noise_floor_abs,
               (unsigned int)detection->matched_center_hz,
               (unsigned long)detection->matched_band_energy,
               (unsigned long)detection->matched_baseline_energy,
               (unsigned long)(detection->matched_ratio_x100 / 100u),
               (unsigned long)(detection->matched_ratio_x100 % 100u),
               (unsigned int)(detection->matched_multiplier_x100 / 100u),
               (unsigned int)(detection->matched_multiplier_x100 % 100u));
    }
    else
    {
        printf("[Audio] Peak registered: peak=%u mean=%u noise=%lu\n",
               (unsigned int)detection->peak_abs,
               (unsigned int)detection->mean_abs,
               (unsigned long)detection->noise_floor_abs);
    }
}

void acoustic_runtime_init(acoustic_runtime_t *runtime, const audio_calibration_t *audio_calibration)
{
    if (runtime == NULL)
    {
        return;
    }

    memset(runtime, 0, sizeof(*runtime));
    audio_stream_queue_init();
    sound_event_detector_init(&runtime->detector, audio_calibration);
    microphone_set_buffer_callback(on_microphone_buffer_ready);
    microphone_init();
}

bool acoustic_runtime_poll(acoustic_runtime_t *runtime,
                           const device_config_t *config,
                           diagnostics_service_t *diagnostics_service,
                           sd_card_buffer_t *sd_card_buffer,
                           device_status_snapshot_t *status,
                           device_wifi_state_t *wifi_state,
                           bool microphone_ready)
{
    if (runtime == NULL)
    {
        return false;
    }

    bool processed_any = false;
    for (int burst = 0; burst < 8; ++burst)
    {
        audio_chunk_view_t chunk;
        if (!audio_stream_queue_peek(&chunk) || chunk.sample_count == 0)
        {
            break;
        }

        sound_event_detector_result_t detection = {0};
        if (sd_card_buffer_is_ready(sd_card_buffer) &&
            !sd_card_buffer_write_audio(sd_card_buffer, chunk.samples, chunk.sample_count))
        {
            runtime_status_set_sd_card_state(DEVICE_COMPONENT_ERROR);
            printf("[SD] Audio circular buffer write stopped: %s\n", sd_card_buffer_last_error(sd_card_buffer));
        }

        if (!runtime->pending_upload &&
            sound_event_detector_process_chunk(&runtime->detector, chunk.samples, chunk.sample_count, &detection))
        {
            runtime->pending_upload = true;
            runtime->event_time_ns = device_clock_now_utc_ns();
            runtime->peak_abs = detection.peak_abs;
            runtime->mean_abs = detection.mean_abs;
            runtime->noise_floor_abs = detection.noise_floor_abs;
            runtime->upload_after = make_timeout_time_ms(DEVICE_SOUND_EVENT_POST_CAPTURE_MS);
            if (diagnostics_service != NULL && diagnostics_service->enabled)
            {
                log_detected_peak(&detection);
            }
        }

        audio_stream_queue_pop_if_matches(chunk.slot);
        processed_any = true;
    }

    if (runtime->pending_upload && time_reached(runtime->upload_after))
    {
        if (!sd_card_buffer_is_ready(sd_card_buffer))
        {
            printf("[Upload] Sound event upload skipped: SD buffer is not ready\n");
            runtime->pending_upload = false;
            return processed_any;
        }

        network_runtime_connect_wifi_with_retries(config,
                                                  diagnostics_service,
                                                  NULL,
                                                  status,
                                                  wifi_state,
                                                  microphone_ready);
        upload_sound_event(config, runtime, sd_card_buffer);
        runtime->pending_upload = false;
        network_runtime_sleep_wifi(config,
                                   diagnostics_service,
                                   status,
                                   wifi_state,
                                   microphone_ready);
    }

    return processed_any;
}
