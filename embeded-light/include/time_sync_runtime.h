#ifndef TIME_SYNC_RUNTIME_H
#define TIME_SYNC_RUNTIME_H

#include <stdbool.h>

#include "device_config.h"
#include "device_status.h"
#include "diagnostics_service.h"
#include "server_health.h"
#include "time_sync_service.h"

void time_sync_runtime_wait_initial(const device_config_t *config,
                                    diagnostics_service_t *diagnostics_service,
                                    time_sync_service_t *time_sync_service,
                                    device_status_snapshot_t *status,
                                    device_wifi_state_t *wifi_state,
                                    bool microphone_ready);

void time_sync_runtime_run_scheduled(const device_config_t *config,
                                     diagnostics_service_t *diagnostics_service,
                                     time_sync_service_t *time_sync_service,
                                     server_health_service_t *server_health_service,
                                     device_status_snapshot_t *status,
                                     device_wifi_state_t *wifi_state,
                                     bool microphone_ready);

#endif
