#ifndef NETWORK_RUNTIME_H
#define NETWORK_RUNTIME_H

#include <stdbool.h>
#include <stdint.h>

#include "device_config.h"
#include "device_status.h"
#include "diagnostics_service.h"
#include "server_health.h"
#include "time_sync_service.h"

void network_runtime_sleep_with_polling(uint32_t sleep_ms_total,
                                        const device_config_t *config,
                                        diagnostics_service_t *diagnostics_service,
                                        time_sync_service_t *time_sync_service,
                                        const server_health_service_t *server_health_service,
                                        device_status_snapshot_t *status,
                                        device_wifi_state_t wifi_state,
                                        bool microphone_ready);

void network_runtime_connect_wifi_with_retries(const device_config_t *config,
                                               diagnostics_service_t *diagnostics_service,
                                               const server_health_service_t *server_health_service,
                                               device_status_snapshot_t *status,
                                               device_wifi_state_t *wifi_state,
                                               bool microphone_ready);

bool network_runtime_ensure_wifi_connected(const char *disconnect_message,
                                           const char *reconnected_message,
                                           const device_config_t *config,
                                           diagnostics_service_t *diagnostics_service,
                                           server_health_service_t *server_health_service,
                                           device_status_snapshot_t *status,
                                           device_wifi_state_t *wifi_state,
                                           bool microphone_ready);

void network_runtime_sleep_wifi(const device_config_t *config,
                                diagnostics_service_t *diagnostics_service,
                                device_status_snapshot_t *status,
                                device_wifi_state_t *wifi_state,
                                bool microphone_ready);

#endif
