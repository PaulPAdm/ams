#include "time_sync_runtime.h"

#include <stdio.h>

#include "device_clock.h"
#include "device_runtime_config.h"
#include "network_runtime.h"
#include "platform_time.h"
#include "runtime_status.h"

void time_sync_runtime_wait_initial(const device_config_t *config,
                                    diagnostics_service_t *diagnostics_service,
                                    time_sync_service_t *time_sync_service,
                                    device_status_snapshot_t *status,
                                    device_wifi_state_t *wifi_state,
                                    bool microphone_ready)
{
    time_sync_service_request_sync(time_sync_service);
    while (!time_sync_service_is_synced(time_sync_service))
    {
        if (network_runtime_ensure_wifi_connected("Wi-Fi lost during time sync. Reconnecting...",
                                                  "Wi-Fi reconnected",
                                                  config,
                                                  diagnostics_service,
                                                  NULL,
                                                  status,
                                                  wifi_state,
                                                  microphone_ready))
        {
            time_sync_service_request_sync(time_sync_service);
        }

        runtime_iteration_run(config,
                              diagnostics_service,
                              time_sync_service,
                              NULL,
                              status,
                              *wifi_state,
                              microphone_ready,
                              true,
                              false);
        sleep_ms(DEVICE_RUNTIME_POLL_SLEEP_MS);
    }

    printf("Clock synced: utc=%llu local=%llu rtt=%llu ns tz=%s offset=%ld sec\n",
           (unsigned long long)device_clock_now_utc_ns(),
           (unsigned long long)device_clock_now_local_ns(),
           (unsigned long long)device_clock_last_rtt_ns(),
           device_clock_timezone_name(),
           (long)device_clock_timezone_offset_seconds());
}

void time_sync_runtime_run_scheduled(const device_config_t *config,
                                     diagnostics_service_t *diagnostics_service,
                                     time_sync_service_t *time_sync_service,
                                     server_health_service_t *server_health_service,
                                     device_status_snapshot_t *status,
                                     device_wifi_state_t *wifi_state,
                                     bool microphone_ready)
{
    if (config == NULL || time_sync_service == NULL || status == NULL || wifi_state == NULL)
    {
        return;
    }

    network_runtime_connect_wifi_with_retries(config,
                                              diagnostics_service,
                                              server_health_service,
                                              status,
                                              wifi_state,
                                              microphone_ready);

    time_sync_service_request_sync(time_sync_service);
    absolute_time_t deadline = make_timeout_time_ms(DEVICE_TIME_SYNC_REQUEST_TIMEOUT_MS);

    while (!time_reached(deadline))
    {
        runtime_iteration_run(config,
                              diagnostics_service,
                              time_sync_service,
                              server_health_service,
                              status,
                              *wifi_state,
                              microphone_ready,
                              true,
                              false);

        if (!time_sync_service_has_pending_work(time_sync_service))
        {
            break;
        }

        sleep_ms(DEVICE_RUNTIME_POLL_SLEEP_MS);
    }

    if (time_sync_service_has_pending_work(time_sync_service))
    {
        printf("Time sync is still pending after %u ms timeout\n",
               (unsigned int)DEVICE_TIME_SYNC_REQUEST_TIMEOUT_MS);
        time_sync_service_mark_timeout(time_sync_service);
    }

    network_runtime_sleep_wifi(config,
                               diagnostics_service,
                               status,
                               wifi_state,
                               microphone_ready);
}
