#include "time_sync_service.h"

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "device_clock.h"
#include "device_runtime_config.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "platform_time.h"

#define TIME_SYNC_URI_MAX 256
#define TIME_SYNC_URL_MAX 384

static const char *TAG = "time-sync";

static bool parse_u64_field(const char *json, const char *field_name, uint64_t *out_value)
{
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", field_name);

    const char *field = strstr(json, pattern);
    if (field == NULL)
    {
        return false;
    }

    const char *cursor = strchr(field, ':');
    if (cursor == NULL)
    {
        return false;
    }

    cursor++;
    while (*cursor != '\0' && isspace((unsigned char)*cursor))
    {
        cursor++;
    }

    if (!isdigit((unsigned char)*cursor))
    {
        return false;
    }

    uint64_t value = 0;
    while (isdigit((unsigned char)*cursor))
    {
        value = (value * 10u) + (uint64_t)(*cursor - '0');
        cursor++;
    }

    *out_value = value;
    return true;
}

static bool parse_i32_field(const char *json, const char *field_name, int32_t *out_value)
{
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", field_name);

    const char *field = strstr(json, pattern);
    if (field == NULL)
    {
        return false;
    }

    const char *cursor = strchr(field, ':');
    if (cursor == NULL)
    {
        return false;
    }

    cursor++;
    while (*cursor != '\0' && isspace((unsigned char)*cursor))
    {
        cursor++;
    }

    int sign = 1;
    if (*cursor == '-')
    {
        sign = -1;
        cursor++;
    }

    if (!isdigit((unsigned char)*cursor))
    {
        return false;
    }

    int32_t value = 0;
    while (isdigit((unsigned char)*cursor))
    {
        value = (value * 10) + (int32_t)(*cursor - '0');
        cursor++;
    }

    *out_value = value * sign;
    return true;
}

static bool parse_string_field(const char *json, const char *field_name, char *out_value, size_t out_size)
{
    if (out_value == NULL || out_size == 0)
    {
        return false;
    }

    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", field_name);

    const char *field = strstr(json, pattern);
    if (field == NULL)
    {
        return false;
    }

    const char *cursor = strchr(field, ':');
    if (cursor == NULL)
    {
        return false;
    }

    cursor++;
    while (*cursor != '\0' && isspace((unsigned char)*cursor))
    {
        cursor++;
    }

    if (*cursor != '"')
    {
        return false;
    }
    cursor++;

    size_t len = 0;
    while (*cursor != '\0' && *cursor != '"' && len + 1u < out_size)
    {
        out_value[len++] = *cursor++;
    }
    out_value[len] = '\0';
    return *cursor == '"';
}

static uint64_t calculate_rtt_ns(uint64_t client_sent_monotonic_ns,
                                 uint64_t client_received_monotonic_ns,
                                 uint64_t server_received_epoch_ns,
                                 uint64_t server_transmit_epoch_ns)
{
    uint64_t client_elapsed_ns = client_received_monotonic_ns - client_sent_monotonic_ns;
    uint64_t server_elapsed_ns = server_transmit_epoch_ns - server_received_epoch_ns;
    return client_elapsed_ns > server_elapsed_ns ? client_elapsed_ns - server_elapsed_ns : 0;
}

static bool apply_sync_response(time_sync_service_t *service, uint64_t client_received_monotonic_ns)
{
    uint64_t server_received_epoch_ns = 0;
    uint64_t server_transmit_epoch_ns = 0;
    int32_t timezone_offset_seconds = 0;
    char timezone_name[DEVICE_CLOCK_TIMEZONE_NAME_MAX + 1u] = {0};

    if (!parse_u64_field(service->response, "server_received_epoch_ns", &server_received_epoch_ns) ||
        !parse_u64_field(service->response, "server_transmit_epoch_ns", &server_transmit_epoch_ns) ||
        !parse_i32_field(service->response, "timezone_offset_seconds", &timezone_offset_seconds) ||
        !parse_string_field(service->response, "timezone_name", timezone_name, sizeof(timezone_name)))
    {
        return false;
    }

    uint64_t rtt_ns = calculate_rtt_ns(service->client_sent_monotonic_ns,
                                      client_received_monotonic_ns,
                                      server_received_epoch_ns,
                                      server_transmit_epoch_ns);
    if (rtt_ns <= service->best_rtt_ns)
    {
        service->best_rtt_ns = rtt_ns;
        device_clock_apply_sync(service->client_sent_monotonic_ns,
                                client_received_monotonic_ns,
                                server_received_epoch_ns,
                                server_transmit_epoch_ns,
                                timezone_offset_seconds,
                                timezone_name);
    }

    return true;
}

static void schedule_after_attempt(time_sync_service_t *service)
{
    if (service->attempts_remaining > 0)
    {
        service->next_attempt_at = make_timeout_time_ms(DEVICE_TIME_SYNC_SAMPLE_DELAY_MS);
        return;
    }

    if (device_clock_is_synced())
    {
        service->status = TIME_SYNC_STATUS_SYNCED;
        service->next_sync_at = make_timeout_time_ms(DEVICE_TIME_SYNC_PERIOD_MS);
    }
    else
    {
        service->status = TIME_SYNC_STATUS_ERROR;
        service->next_sync_at = make_timeout_time_ms(DEVICE_TIME_SYNC_RETRY_DELAY_MS);
    }
}

static esp_err_t time_sync_http_event_handler(esp_http_client_event_t *evt)
{
    time_sync_service_t *service = (time_sync_service_t *)evt->user_data;
    if (service == NULL)
    {
        return ESP_OK;
    }

    if (evt->event_id == HTTP_EVENT_ON_DATA && evt->data != NULL && evt->data_len > 0)
    {
        if (service->response_len + (size_t)evt->data_len >= TIME_SYNC_RESPONSE_MAX)
        {
            service->response_overflow = true;
            return ESP_OK;
        }

        memcpy(service->response + service->response_len, evt->data, (size_t)evt->data_len);
        service->response_len += (size_t)evt->data_len;
        service->response[service->response_len] = '\0';
    }

    return ESP_OK;
}

static bool start_time_sync_request(time_sync_service_t *service)
{
    if (service == NULL || !service->server_addr_valid || service->request_in_flight || service->attempts_remaining == 0)
    {
        return false;
    }

    char uri[TIME_SYNC_URI_MAX];
    service->client_sent_monotonic_ns = device_clock_monotonic_ns();
    int written = snprintf(uri,
                           sizeof(uri),
                           "/api/devices/%s/time/sync?client_sent_monotonic_ns=%llu",
                           service->device_id,
                           (unsigned long long)service->client_sent_monotonic_ns);
    if (written <= 0 || written >= (int)sizeof(uri))
    {
        ESP_LOGW(TAG, "Time sync URI is too long for device %s", service->device_id);
        return false;
    }

    char url[TIME_SYNC_URL_MAX];
    written = snprintf(url,
                       sizeof(url),
                       "http://%s:%u%s",
                       service->server_ip,
                       TIME_SYNC_HTTP_PORT,
                       uri);
    if (written <= 0 || written >= (int)sizeof(url))
    {
        ESP_LOGW(TAG, "Time sync URL is too long for device %s", service->device_id);
        return false;
    }

    service->response_len = 0;
    service->response[0] = '\0';
    service->response_overflow = false;
    service->attempts_remaining--;
    service->request_in_flight = true;

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .event_handler = time_sync_http_event_handler,
        .user_data = service,
        .timeout_ms = DEVICE_TIME_SYNC_REQUEST_TIMEOUT_MS,
        .disable_auto_redirect = true,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL)
    {
        service->request_in_flight = false;
        schedule_after_attempt(service);
        return false;
    }

    esp_err_t ret = esp_http_client_perform(client);
    int status_code = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    service->request_in_flight = false;

    bool http_success = ret == ESP_OK && status_code >= 200 && status_code < 300;
    bool sync_success = false;
    if (http_success && !service->response_overflow)
    {
        sync_success = apply_sync_response(service, device_clock_monotonic_ns());
    }

    if (!sync_success)
    {
        ESP_LOGW(TAG, "Time sync failed for %s: err=%s http=%d overflow=%s",
                 service->device_id,
                 esp_err_to_name(ret),
                 status_code,
                 service->response_overflow ? "yes" : "no");
    }

    schedule_after_attempt(service);
    return sync_success;
}

void time_sync_service_init(time_sync_service_t *service, const device_config_t *config)
{
    if (service == NULL || config == NULL)
    {
        return;
    }

    memset(service, 0, sizeof(*service));
    strncpy(service->server_ip, config->server_ip, sizeof(service->server_ip) - 1u);
    strncpy(service->device_id, config->device_id, sizeof(service->device_id) - 1u);
    service->server_addr_valid = service->server_ip[0] != '\0';
    service->status = service->server_addr_valid ? TIME_SYNC_STATUS_IDLE : TIME_SYNC_STATUS_ERROR;
    service->next_sync_at = get_absolute_time();
    service->best_rtt_ns = ULLONG_MAX;

    if (!service->server_addr_valid)
    {
        ESP_LOGW(TAG, "Invalid server host for time sync service: %s", service->server_ip);
    }
}

void time_sync_service_request_sync(time_sync_service_t *service)
{
    if (service == NULL || !service->server_addr_valid)
    {
        return;
    }

    service->attempts_remaining = DEVICE_TIME_SYNC_ATTEMPT_COUNT;
    service->best_rtt_ns = ULLONG_MAX;
    service->status = TIME_SYNC_STATUS_PENDING;
    service->next_attempt_at = get_absolute_time();
}

void time_sync_service_poll(time_sync_service_t *service)
{
    if (service == NULL || !service->server_addr_valid || service->request_in_flight)
    {
        return;
    }

    if ((service->status == TIME_SYNC_STATUS_SYNCED || service->status == TIME_SYNC_STATUS_ERROR) &&
        time_reached(service->next_sync_at))
    {
        time_sync_service_request_sync(service);
    }

    if (service->attempts_remaining == 0 || !time_reached(service->next_attempt_at))
    {
        return;
    }

    start_time_sync_request(service);
}

bool time_sync_service_is_due(const time_sync_service_t *service)
{
    return service != NULL &&
           service->server_addr_valid &&
           !service->request_in_flight &&
           service->attempts_remaining == 0 &&
           time_reached(service->next_sync_at);
}

bool time_sync_service_has_pending_work(const time_sync_service_t *service)
{
    return service != NULL &&
           (service->request_in_flight ||
            service->attempts_remaining > 0 ||
            service->status == TIME_SYNC_STATUS_PENDING);
}

void time_sync_service_mark_timeout(time_sync_service_t *service)
{
    if (service == NULL)
    {
        return;
    }

    service->request_in_flight = false;
    service->attempts_remaining = 0;
    service->status = TIME_SYNC_STATUS_ERROR;
    service->next_sync_at = make_timeout_time_ms(DEVICE_TIME_SYNC_RETRY_DELAY_MS);
}

bool time_sync_service_is_synced(const time_sync_service_t *service)
{
    return service != NULL && service->status == TIME_SYNC_STATUS_SYNCED && device_clock_is_synced();
}

time_sync_status_t time_sync_service_status(const time_sync_service_t *service)
{
    if (service == NULL)
    {
        return TIME_SYNC_STATUS_ERROR;
    }

    return service->status;
}
