#ifndef SOUND_EVENT_UPLOADER_H
#define SOUND_EVENT_UPLOADER_H

#include <stdbool.h>
#include <stdint.h>

#include "device_config.h"
#include "sd_card_buffer.h"

typedef struct
{
    uint64_t event_time_ns;
    uint16_t peak_abs;
    uint16_t mean_abs;
    uint32_t noise_floor_abs;
} sound_event_upload_t;

bool sound_event_uploader_upload(const device_config_t *config,
                                 sd_card_buffer_t *sd_card_buffer,
                                 const sound_event_upload_t *event,
                                 uint32_t timeout_ms);

#endif
