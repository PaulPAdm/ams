#include "server_health.h"

#include <stdio.h>
#include <string.h>

#include "device_runtime_config.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "platform_time.h"

#define SERVER_HEALTH_URI_MAX 512

static const char *TAG = "server-health";

static void schedule_next_report(server_health_service_t *service, uint32_t delay_ms)
{
    service->next_report_at = make_timeout_time_ms(delay_ms);
}

static bool server_health_start_request(server_health_service_t *service,
                                        server_health_request_kind_t kind,
                                        const char *uri)
{
    if (service == NULL || uri == NULL || !service->server_addr_valid || service->request_in_flight)
    {
        return false;
    }

    char url[SERVER_HEALTH_URI_MAX + DEVICE_CONFIG_SERVER_IP_MAX + 16u];
    int written = snprintf(url,
                           sizeof(url),
                           "http://%s:%u%s",
                           service->server_ip,
                           SERVER_HEALTH_HTTP_PORT,
                           uri);
    if (written <= 0 || written >= (int)sizeof(url))
    {
        service->connection_status = SERVER_HEALTH_STATUS_ERROR;
        return false;
    }

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = DEVICE_HEALTH_REQUEST_TIMEOUT_MS,
        .disable_auto_redirect = true,
    };

    service->request_in_flight = true;
    service->active_request = kind;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL)
    {
        service->request_in_flight = false;
        service->active_request = SERVER_HEALTH_REQUEST_NONE;
        service->connection_status = SERVER_HEALTH_STATUS_ERROR;
        return false;
    }

    esp_err_t ret = esp_http_client_perform(client);
    int status_code = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    service->request_in_flight = false;
    service->active_request = SERVER_HEALTH_REQUEST_NONE;

    bool success = ret == ESP_OK && status_code >= 200 && status_code < 300;
    if (success)
    {
        service->connection_status = SERVER_HEALTH_STATUS_CONNECTED;
        ESP_LOGI(TAG, "Health report sent for %s", service->device_id);
    }
    else
    {
        service->connection_status = SERVER_HEALTH_STATUS_ERROR;
        ESP_LOGW(TAG, "Health report failed for %s: err=%s http=%d",
                 service->device_id,
                 esp_err_to_name(ret),
                 status_code);
    }

    return success;
}

static void server_health_try_report(server_health_service_t *service,
                                     bool wifi_connected,
                                     uint16_t audio_queue_depth,
                                     uint32_t audio_dropped_chunks)
{
    const char *status_message = service->microphone_active ? "streaming_power_unknown" : "network_ready_power_unknown";

    char uri[SERVER_HEALTH_URI_MAX];
    int written = snprintf(
        uri,
        sizeof(uri),
        "/api/devices/%s/health/report?firmware_version=%s&status_message=%s&uptime_ms=%llu&wifi_connected=%s&microphone_active=%s&ina219_online=unknown&bus_voltage_v=unknown&shunt_voltage_mv=unknown&current_ma=unknown&power_mw=unknown&computed_power_mw=unknown&audio_queue_depth=%u&audio_dropped_chunks=%lu",
        service->device_id,
        DEVICE_FIRMWARE_VERSION,
        status_message,
        (unsigned long long)(get_absolute_time() / 1000ull),
        wifi_connected ? "true" : "false",
        service->microphone_active ? "true" : "false",
        (unsigned int)audio_queue_depth,
        (unsigned long)audio_dropped_chunks);

    if (written <= 0 || written >= (int)sizeof(uri))
    {
        ESP_LOGW(TAG, "Health URI is too long for device %s", service->device_id);
        schedule_next_report(service, DEVICE_HEALTH_RETRY_MS);
        return;
    }

    bool success = server_health_start_request(service, SERVER_HEALTH_REQUEST_REPORT, uri);
    schedule_next_report(service, success ? DEVICE_HEALTH_REPORT_INTERVAL_MS : DEVICE_HEALTH_RETRY_MS);
}

void server_health_service_init(server_health_service_t *service, const device_config_t *config)
{
    if (service == NULL || config == NULL)
    {
        return;
    }

    memset(service, 0, sizeof(*service));
    strncpy(service->server_ip, config->server_ip, sizeof(service->server_ip) - 1u);
    strncpy(service->device_id, config->device_id, sizeof(service->device_id) - 1u);
    service->server_addr_valid = service->server_ip[0] != '\0';
    service->next_report_at = get_absolute_time();
    service->connection_status = SERVER_HEALTH_STATUS_PENDING;

    if (!service->server_addr_valid)
    {
        service->connection_status = SERVER_HEALTH_STATUS_ERROR;
        ESP_LOGW(TAG, "Invalid server host for health service: %s", service->server_ip);
    }
}

void server_health_service_set_microphone_active(server_health_service_t *service, bool active)
{
    if (service == NULL)
    {
        return;
    }

    bool changed = service->microphone_active != active;
    service->microphone_active = active;
    if (active && changed)
    {
        service->next_report_at = get_absolute_time();
    }
}

void server_health_service_request_report(server_health_service_t *service)
{
    if (service == NULL)
    {
        return;
    }

    service->next_report_at = get_absolute_time();
}

bool server_health_service_is_report_due(const server_health_service_t *service)
{
    return service != NULL &&
           service->server_addr_valid &&
           !service->request_in_flight &&
           time_reached(service->next_report_at);
}

bool server_health_service_is_request_in_flight(const server_health_service_t *service)
{
    return service != NULL && service->request_in_flight;
}

void server_health_service_mark_timeout(server_health_service_t *service)
{
    if (service == NULL)
    {
        return;
    }

    service->request_in_flight = false;
    service->active_request = SERVER_HEALTH_REQUEST_NONE;
    service->connection_status = SERVER_HEALTH_STATUS_ERROR;
    schedule_next_report(service, DEVICE_HEALTH_RETRY_MS);
}

void server_health_service_poll(server_health_service_t *service,
                                bool wifi_connected,
                                uint16_t audio_queue_depth,
                                uint32_t audio_dropped_chunks)
{
    if (service == NULL || !service->server_addr_valid || service->request_in_flight)
    {
        return;
    }

    if (!wifi_connected)
    {
        return;
    }

    if (time_reached(service->next_report_at))
    {
        server_health_try_report(service,
                                 wifi_connected,
                                 audio_queue_depth,
                                 audio_dropped_chunks);
    }
}

server_health_connection_status_t server_health_service_connection_status(const server_health_service_t *service)
{
    if (service == NULL)
    {
        return SERVER_HEALTH_STATUS_ERROR;
    }

    return service->connection_status;
}
