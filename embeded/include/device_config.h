#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

#include "audio_calibration.h"

#define DEVICE_CONFIG_SSID_MAX 32
#define DEVICE_CONFIG_PASSWORD_MAX 64
#define DEVICE_CONFIG_SERVER_IP_MAX 63
#define DEVICE_CONFIG_ID_MAX 63
#define DEVICE_CONFIG_HEALTH_INTERVAL_MIN_DEFAULT 15u
#define DEVICE_CONFIG_TIME_SYNC_INTERVAL_MIN_DEFAULT 60u
#define DEVICE_CONFIG_INTERVAL_MIN_MIN 1u
#define DEVICE_CONFIG_INTERVAL_MIN_MAX 1440u

typedef struct
{
    char ssid[DEVICE_CONFIG_SSID_MAX + 1];
    char password[DEVICE_CONFIG_PASSWORD_MAX + 1];
    char server_ip[DEVICE_CONFIG_SERVER_IP_MAX + 1];
    char device_id[DEVICE_CONFIG_ID_MAX + 1];
    audio_calibration_t audio_calibration;
    /* How often the device logs a health sample and flushes buffered reports. */
    uint32_t health_report_interval_min;
    /* How often the device re-synchronizes its clock with the server. */
    uint32_t time_sync_interval_min;
} device_config_t;

bool device_config_load(device_config_t *config);
bool device_config_save(const device_config_t *config);

#endif
