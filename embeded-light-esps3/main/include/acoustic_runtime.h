#ifndef ACOUSTIC_RUNTIME_H
#define ACOUSTIC_RUNTIME_H

#include <stdbool.h>
#include <stdint.h>

#include "device_config.h"
#include "device_status.h"
#include "diagnostics_service.h"
#include "platform_time.h"
#include "sd_card_buffer.h"
#include "sound_event_detector.h"

typedef struct
{
    sound_event_detector_t detector;
    bool pending_upload;
    uint64_t event_time_ns;
    uint16_t peak_abs;
    uint16_t mean_abs;
    uint32_t noise_floor_abs;
    absolute_time_t upload_after;
} acoustic_runtime_t;

void acoustic_runtime_init(acoustic_runtime_t *runtime, const audio_calibration_t *audio_calibration);

bool acoustic_runtime_poll(acoustic_runtime_t *runtime,
                           const device_config_t *config,
                           diagnostics_service_t *diagnostics_service,
                           sd_card_buffer_t *sd_card_buffer,
                           device_status_snapshot_t *status,
                           device_wifi_state_t *wifi_state,
                           bool microphone_ready);

#endif
