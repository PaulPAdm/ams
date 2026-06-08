#ifndef DIAGNOSTICS_SERVICE_H
#define DIAGNOSTICS_SERVICE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "device_status.h"
#include "platform_time.h"

typedef struct
{
    bool enabled;
    absolute_time_t next_log_at;
} diagnostics_service_t;

void diagnostics_service_init(diagnostics_service_t *service, bool enabled);
void diagnostics_service_poll(diagnostics_service_t *service,
                              const device_status_snapshot_t *snapshot,
                              uint16_t audio_queue_depth,
                              uint32_t audio_dropped_chunks,
                              uint16_t dsp_frame_depth,
                              uint32_t dsp_dropped_frames,
                              uint32_t processed_capture_buffers);

#endif
