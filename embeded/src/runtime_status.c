#include "runtime_status.h"

#include "audio_stream_queue.h"
#include "inky_status_display.h"

static device_component_state_t g_sd_card_state = DEVICE_COMPONENT_PENDING;

static device_server_state_t map_server_status(const server_health_service_t *server_health_service,
                                               device_wifi_state_t wifi_state)
{
    if (server_health_service == NULL)
    {
        return DEVICE_SERVER_PENDING;
    }

    if (wifi_state == DEVICE_WIFI_SLEEP || wifi_state == DEVICE_WIFI_CONNECTING)
    {
        return DEVICE_SERVER_PENDING;
    }

    if (wifi_state == DEVICE_WIFI_ERROR)
    {
        return DEVICE_SERVER_ERROR;
    }

    switch (server_health_service_connection_status(server_health_service))
    {
    case SERVER_HEALTH_STATUS_CONNECTED:
        return DEVICE_SERVER_CONNECTED;
    case SERVER_HEALTH_STATUS_ERROR:
        return DEVICE_SERVER_ERROR;
    case SERVER_HEALTH_STATUS_PENDING:
    default:
        return DEVICE_SERVER_PENDING;
    }
}

static void refresh_runtime_status(device_status_snapshot_t *status,
                                   const power_meter_service_t *power_meter_service,
                                   const server_health_service_t *server_health_service,
                                   device_wifi_state_t wifi_state,
                                   bool microphone_ready)
{
    if (status == NULL)
    {
        return;
    }

    status->microphone = microphone_ready ? DEVICE_COMPONENT_OK : DEVICE_COMPONENT_PENDING;
    status->ina219 = power_meter_service_is_sensor_online(power_meter_service) ? DEVICE_COMPONENT_OK : DEVICE_COMPONENT_ERROR;
    status->sd_card = g_sd_card_state;
    status->wifi = wifi_state;
    status->server = map_server_status(server_health_service, wifi_state);

    ina219_wattmeter_reading_t power_reading;
    status->has_power_reading = power_meter_service_get_latest_reading(power_meter_service, &power_reading);
    if (status->has_power_reading)
    {
        status->bus_voltage_v = power_reading.bus_voltage_v;
        status->current_ma = power_reading.current_ma;
    }
}

void runtime_status_set_sd_card_state(device_component_state_t sd_card_state)
{
    g_sd_card_state = sd_card_state;
}

void runtime_status_publish(const device_config_t *config,
                            diagnostics_service_t *diagnostics_service,
                            power_meter_service_t *power_meter_service,
                            const server_health_service_t *server_health_service,
                            device_status_snapshot_t *status,
                            device_wifi_state_t wifi_state,
                            bool microphone_ready)
{
    if (config == NULL || status == NULL)
    {
        return;
    }

    refresh_runtime_status(status, power_meter_service, server_health_service, wifi_state, microphone_ready);

    if (diagnostics_service != NULL)
    {
        diagnostics_service_poll(diagnostics_service,
                                 status,
                                 power_meter_service,
                                 audio_stream_queue_depth(),
                                 audio_stream_queue_dropped_chunks(),
                                 audio_dsp_frame_depth(),
                                 audio_dsp_frame_dropped(),
                                 audio_stream_queue_processed_buffers());
    }

    inky_status_display_update(config,
                               status,
                               AUDIO_STREAM_QUEUE_INPUT_SAMPLE_RATE_HZ,
                               AUDIO_STREAM_QUEUE_DOWNSAMPLE_FACTOR);
}

void runtime_iteration_run(const device_config_t *config,
                           diagnostics_service_t *diagnostics_service,
                           power_meter_service_t *power_meter_service,
                           time_sync_service_t *time_sync_service,
                           server_health_service_t *server_health_service,
                           device_status_snapshot_t *status,
                           device_wifi_state_t wifi_state,
                           bool microphone_ready,
                           bool wifi_connected,
                           bool report_server_health)
{
    power_meter_service_poll(power_meter_service);
    if (time_sync_service != NULL)
    {
        time_sync_service_poll(time_sync_service);
    }

    if (report_server_health && server_health_service != NULL)
    {
        server_health_service_poll(server_health_service,
                                   power_meter_service,
                                   wifi_connected,
                                   audio_stream_queue_depth(),
                                   audio_stream_queue_dropped_chunks());
    }

    runtime_status_publish(config,
                           diagnostics_service,
                           power_meter_service,
                           server_health_service,
                           status,
                           wifi_state,
                           microphone_ready);
}
