#ifndef DEVICE_STATUS_H
#define DEVICE_STATUS_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    DEVICE_COMPONENT_PENDING = 0,
    DEVICE_COMPONENT_OK,
    DEVICE_COMPONENT_ERROR,
    DEVICE_COMPONENT_NOT_INSTALLED
} device_component_state_t;

typedef enum
{
    DEVICE_WIFI_SLEEP = 0,
    DEVICE_WIFI_CONNECTING,
    DEVICE_WIFI_CONNECTED,
    DEVICE_WIFI_ERROR
} device_wifi_state_t;

typedef enum
{
    DEVICE_SERVER_PENDING = 0,
    DEVICE_SERVER_CONNECTED,
    DEVICE_SERVER_ERROR
} device_server_state_t;

typedef struct
{
    device_component_state_t microphone;
    device_component_state_t sd_card;
    device_wifi_state_t wifi;
    device_server_state_t server;
} device_status_snapshot_t;

void device_status_snapshot_init(device_status_snapshot_t *snapshot);
bool device_status_snapshot_equals(const device_status_snapshot_t *lhs, const device_status_snapshot_t *rhs);

const char *device_component_state_label(device_component_state_t state);
const char *device_wifi_state_label(device_wifi_state_t state);
const char *device_server_state_label(device_server_state_t state);

#ifdef __cplusplus
}
#endif

#endif
