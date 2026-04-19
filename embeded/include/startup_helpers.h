#ifndef STARTUP_HELPERS_H
#define STARTUP_HELPERS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "device_config.h"

bool save_config_or_log(const device_config_t *config, const char *error_message);
void resolve_startup_config(device_config_t *config, bool has_config);

void print_current_settings(const device_config_t *config,
                            int server_port,
                            int raw_audio_sample_rate_hz,
                            int audio_downsample_factor);

#endif
