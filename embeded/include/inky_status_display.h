#ifndef INKY_STATUS_DISPLAY_H
#define INKY_STATUS_DISPLAY_H

#include <stdbool.h>

#include "device_status.h"
#include "device_config.h"
#include "audio_calibration.h"

#ifdef __cplusplus
extern "C" {
#endif

void inky_status_display_reset(void);
void inky_status_display_show_missing_settings(bool usb_console_connected);
void inky_status_display_update(const device_config_t *config,
                                const device_status_snapshot_t *status,
                                int raw_audio_sample_rate_hz,
                                int audio_downsample_factor);
void inky_status_display_show_audio_calibration(const audio_calibration_t *calibration);

#ifdef __cplusplus
}
#endif

#endif
