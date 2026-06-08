#ifndef HEALTH_RUNTIME_H
#define HEALTH_RUNTIME_H

#include <stdbool.h>

#include "device_config.h"
#include "device_status.h"
#include "diagnostics_service.h"
#include "server_health.h"

void health_runtime_send_scheduled_report(const device_config_t *config,
                                          diagnostics_service_t *diagnostics_service,
                                          server_health_service_t *server_health_service,
                                          device_status_snapshot_t *status,
                                          device_wifi_state_t *wifi_state,
                                          bool microphone_ready);

#endif
