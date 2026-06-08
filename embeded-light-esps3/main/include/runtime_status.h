#ifndef RUNTIME_STATUS_H
#define RUNTIME_STATUS_H

#include <stdbool.h>

#include "device_config.h"
#include "device_status.h"
#include "diagnostics_service.h"
#include "server_health.h"
#include "time_sync_service.h"

void runtime_status_publish(const device_config_t *config,
                            diagnostics_service_t *diagnostics_service,
                            const server_health_service_t *server_health_service,
                            device_status_snapshot_t *status,
                            device_wifi_state_t wifi_state,
                            bool microphone_ready);

void runtime_status_set_sd_card_state(device_component_state_t sd_card_state);

void runtime_iteration_run(const device_config_t *config,
                           diagnostics_service_t *diagnostics_service,
                           time_sync_service_t *time_sync_service,
                           server_health_service_t *server_health_service,
                           device_status_snapshot_t *status,
                           device_wifi_state_t wifi_state,
                           bool microphone_ready,
                           bool wifi_connected,
                           bool report_server_health);

#endif
