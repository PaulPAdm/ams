#include "diagnostics_service.h"

#include <stdio.h>
#include <string.h>

#define DIAGNOSTICS_LOG_INTERVAL_MS 5000

void diagnostics_service_init(diagnostics_service_t *service, bool enabled)
{
    if (service == NULL)
    {
        return;
    }

    memset(service, 0, sizeof(*service));
    service->enabled = enabled;
    service->next_log_at = get_absolute_time();
}

void diagnostics_service_poll(diagnostics_service_t *service,
                              const device_status_snapshot_t *snapshot,
                              uint16_t audio_queue_depth,
                              uint32_t audio_dropped_chunks,
                              uint16_t dsp_frame_depth,
                              uint32_t dsp_dropped_frames,
                              uint32_t processed_capture_buffers)
{
    (void)audio_queue_depth;
    (void)audio_dropped_chunks;
    (void)dsp_frame_depth;
    (void)dsp_dropped_frames;
    (void)processed_capture_buffers;

    if (service == NULL || snapshot == NULL || !service->enabled)
    {
        return;
    }

    if (!time_reached(service->next_log_at))
    {
        return;
    }

    printf("[Diagnostics] Microphone=%s | SD=%s | Wi-Fi=%s | Server=%s | Power=Unknown\n",
           device_component_state_label(snapshot->microphone),
           device_component_state_label(snapshot->sd_card),
           device_wifi_state_label(snapshot->wifi),
           device_server_state_label(snapshot->server));

    service->next_log_at = make_timeout_time_ms(DIAGNOSTICS_LOG_INTERVAL_MS);
}
