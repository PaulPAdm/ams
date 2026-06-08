#ifndef STARTUP_HELPERS_H
#define STARTUP_HELPERS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "device_config.h"
#include "power_meter_service.h"

typedef enum
{
    STARTUP_RUN_MODE_NORMAL = 0,
    STARTUP_RUN_MODE_DIAGNOSTICS,
    STARTUP_RUN_MODE_AUDIO_CALIBRATION
} startup_run_mode_t;

bool save_config_or_log(const device_config_t *config, const char *error_message);
bool startup_wait_for_usb_console(power_meter_service_t *power_meter_service);
startup_run_mode_t resolve_startup_config(device_config_t *config, bool has_config, bool usb_console_connected);
const char *startup_run_mode_label(startup_run_mode_t mode);

void print_current_settings(const device_config_t *config,
                            int raw_audio_sample_rate_hz,
                            int audio_downsample_factor);

#endif
