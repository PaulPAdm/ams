#include "health_runtime.h"

#include <stdio.h>

#include "device_runtime_config.h"
#include "network_runtime.h"
#include "pico/stdlib.h"
#include "runtime_status.h"

void health_runtime_send_scheduled_report(const device_config_t *config,
                                          diagnostics_service_t *diagnostics_service,
                                          server_health_service_t *server_health_service,
                                          device_status_snapshot_t *status,
                                          device_wifi_state_t *wifi_state,
                                          bool microphone_ready)
{
    if (config == NULL || server_health_service == NULL || status == NULL || wifi_state == NULL)
    {
        return;
    }

    network_runtime_connect_wifi_with_retries(config,
                                              diagnostics_service,
                                              server_health_service,
                                              status,
                                              wifi_state,
                                              microphone_ready);

    server_health_service_request_report(server_health_service);
    absolute_time_t deadline = make_timeout_time_ms(DEVICE_HEALTH_REQUEST_TIMEOUT_MS);

    while (!time_reached(deadline))
    {
        runtime_iteration_run(config,
                              diagnostics_service,
                              NULL,
                              server_health_service,
                              status,
                              *wifi_state,
                              microphone_ready,
                              true,
                              true);

        if (!server_health_service_is_request_in_flight(server_health_service) &&
            !server_health_service_is_report_due(server_health_service))
        {
            break;
        }

        sleep_ms(DEVICE_RUNTIME_POLL_SLEEP_MS);
    }

    if (server_health_service_is_request_in_flight(server_health_service))
    {
        printf("Health report is still pending after %u ms timeout\n",
               (unsigned int)DEVICE_HEALTH_REQUEST_TIMEOUT_MS);
        server_health_service_mark_timeout(server_health_service);
    }

    network_runtime_sleep_wifi(config,
                               diagnostics_service,
                               status,
                               wifi_state,
                               microphone_ready);
}
