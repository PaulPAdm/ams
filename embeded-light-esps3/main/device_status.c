#include "device_status.h"

#include <string.h>

void device_status_snapshot_init(device_status_snapshot_t *snapshot)
{
    if (snapshot == NULL)
    {
        return;
    }

    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->microphone = DEVICE_COMPONENT_PENDING;
    snapshot->sd_card = DEVICE_COMPONENT_PENDING;
    snapshot->wifi = DEVICE_WIFI_SLEEP;
    snapshot->server = DEVICE_SERVER_PENDING;
}

bool device_status_snapshot_equals(const device_status_snapshot_t *lhs, const device_status_snapshot_t *rhs)
{
    if (lhs == NULL || rhs == NULL)
    {
        return false;
    }

    return lhs->microphone == rhs->microphone &&
           lhs->sd_card == rhs->sd_card &&
           lhs->wifi == rhs->wifi &&
           lhs->server == rhs->server;
}

const char *device_component_state_label(device_component_state_t state)
{
    switch (state)
    {
    case DEVICE_COMPONENT_OK:
        return "OK";
    case DEVICE_COMPONENT_ERROR:
        return "Error";
    case DEVICE_COMPONENT_NOT_INSTALLED:
        return "Not Installed";
    case DEVICE_COMPONENT_PENDING:
    default:
        return "Pending";
    }
}

const char *device_wifi_state_label(device_wifi_state_t state)
{
    switch (state)
    {
    case DEVICE_WIFI_CONNECTING:
        return "Connecting";
    case DEVICE_WIFI_CONNECTED:
        return "Connected";
    case DEVICE_WIFI_ERROR:
        return "Error";
    case DEVICE_WIFI_SLEEP:
    default:
        return "Sleep";
    }
}

const char *device_server_state_label(device_server_state_t state)
{
    switch (state)
    {
    case DEVICE_SERVER_CONNECTED:
        return "Connected";
    case DEVICE_SERVER_ERROR:
        return "Error";
    case DEVICE_SERVER_PENDING:
    default:
        return "Pending";
    }
}
