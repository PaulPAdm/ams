#include "network_runtime.h"

#include <stdio.h>

#include "device_runtime_config.h"
#include "hardware/watchdog.h"
#include "pico/stdlib.h"
#include "runtime_status.h"
#include "wifi_service.h"

static void set_wifi_state_and_publish(const device_config_t *config,
                                       diagnostics_service_t *diagnostics_service,
                                       power_meter_service_t *power_meter_service,
                                       const server_health_service_t *server_health_service,
                                       device_status_snapshot_t *status,
                                       device_wifi_state_t *wifi_state,
                                       device_wifi_state_t next_state,
                                       bool microphone_ready)
{
    if (wifi_state == NULL)
    {
        return;
    }

    *wifi_state = next_state;
    runtime_status_publish(config,
                           diagnostics_service,
                           power_meter_service,
                           server_health_service,
                           status,
                           *wifi_state,
                           microphone_ready);
}

void network_runtime_sleep_with_polling(uint32_t sleep_ms_total,
                                        const device_config_t *config,
                                        diagnostics_service_t *diagnostics_service,
                                        power_meter_service_t *power_meter_service,
                                        time_sync_service_t *time_sync_service,
                                        const server_health_service_t *server_health_service,
                                        device_status_snapshot_t *status,
                                        device_wifi_state_t wifi_state,
                                        bool microphone_ready)
{
    absolute_time_t deadline = make_timeout_time_ms(sleep_ms_total);
    while (!time_reached(deadline))
    {
        watchdog_update();
        runtime_iteration_run(config,
                              diagnostics_service,
                              power_meter_service,
                              time_sync_service,
                              (server_health_service_t *)server_health_service,
                              status,
                              wifi_state,
                              microphone_ready,
                              false,
                              false);
        sleep_ms(DEVICE_RUNTIME_POLL_SLEEP_MS);
    }
}

void network_runtime_connect_wifi_with_retries(const device_config_t *config,
                                               diagnostics_service_t *diagnostics_service,
                                               power_meter_service_t *power_meter_service,
                                               const server_health_service_t *server_health_service,
                                               device_status_snapshot_t *status,
                                               device_wifi_state_t *wifi_state,
                                               bool microphone_ready)
{
    if (config == NULL || power_meter_service == NULL || status == NULL || wifi_state == NULL)
    {
        return;
    }

    set_wifi_state_and_publish(config,
                               diagnostics_service,
                               power_meter_service,
                               server_health_service,
                               status,
                               wifi_state,
                               DEVICE_WIFI_CONNECTING,
                               microphone_ready);

    while (!is_wifi_connected())
    {
        if (connect_to_wifi(config->ssid, config->password) == 0)
        {
            set_wifi_state_and_publish(config,
                                       diagnostics_service,
                                       power_meter_service,
                                       server_health_service,
                                       status,
                                       wifi_state,
                                       DEVICE_WIFI_CONNECTED,
                                       microphone_ready);
            return;
        }

        set_wifi_state_and_publish(config,
                                   diagnostics_service,
                                   power_meter_service,
                                   server_health_service,
                                   status,
                                   wifi_state,
                                   DEVICE_WIFI_ERROR,
                                   microphone_ready);
        printf("Retrying Wi-Fi connection...\n");
        network_runtime_sleep_with_polling(5000,
                                           config,
                                           diagnostics_service,
                                           power_meter_service,
                                           NULL,
                                           server_health_service,
                                           status,
                                           *wifi_state,
                                           microphone_ready);
        set_wifi_state_and_publish(config,
                                   diagnostics_service,
                                   power_meter_service,
                                   server_health_service,
                                   status,
                                   wifi_state,
                                   DEVICE_WIFI_CONNECTING,
                                   microphone_ready);
    }

    *wifi_state = DEVICE_WIFI_CONNECTED;
}

bool network_runtime_ensure_wifi_connected(const char *disconnect_message,
                                           const char *reconnected_message,
                                           const device_config_t *config,
                                           diagnostics_service_t *diagnostics_service,
                                           power_meter_service_t *power_meter_service,
                                           server_health_service_t *server_health_service,
                                           device_status_snapshot_t *status,
                                           device_wifi_state_t *wifi_state,
                                           bool microphone_ready)
{
    if (is_wifi_connected())
    {
        *wifi_state = DEVICE_WIFI_CONNECTED;
        return false;
    }

    set_wifi_state_and_publish(config,
                               diagnostics_service,
                               power_meter_service,
                               server_health_service,
                               status,
                               wifi_state,
                               DEVICE_WIFI_ERROR,
                               microphone_ready);
    printf("%s\n", disconnect_message);
    network_runtime_connect_wifi_with_retries(config,
                                              diagnostics_service,
                                              power_meter_service,
                                              server_health_service,
                                              status,
                                              wifi_state,
                                              microphone_ready);
    printf("%s\n", reconnected_message);
    return true;
}

void network_runtime_sleep_wifi(const device_config_t *config,
                                diagnostics_service_t *diagnostics_service,
                                power_meter_service_t *power_meter_service,
                                device_status_snapshot_t *status,
                                device_wifi_state_t *wifi_state,
                                bool microphone_ready)
{
    disconnect_wifi();
    *wifi_state = DEVICE_WIFI_SLEEP;
    runtime_status_publish(config,
                           diagnostics_service,
                           power_meter_service,
                           NULL,
                           status,
                           *wifi_state,
                           microphone_ready);
    printf("Wi-Fi sleeping; device is now in autonomous acoustic monitoring mode\n");
}
