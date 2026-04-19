#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

#define DEVICE_CONFIG_SSID_MAX 32
#define DEVICE_CONFIG_PASSWORD_MAX 64
#define DEVICE_CONFIG_SERVER_IP_MAX 63
#define DEVICE_CONFIG_ID_MAX 63
typedef struct
{
    char ssid[DEVICE_CONFIG_SSID_MAX + 1];
    char password[DEVICE_CONFIG_PASSWORD_MAX + 1];
    char server_ip[DEVICE_CONFIG_SERVER_IP_MAX + 1];
    char device_id[DEVICE_CONFIG_ID_MAX + 1];
} device_config_t;

bool device_config_load(device_config_t *config);
bool device_config_save(const device_config_t *config);

#endif
