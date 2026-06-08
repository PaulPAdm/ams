#include "time_sync_service.h"

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "device_clock.h"
#include "device_runtime_config.h"
#include "lwip/altcp.h"
#include "lwip/pbuf.h"
#include "pico/cyw43_arch.h"

#define TIME_SYNC_URI_MAX 256

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

static void schedule_after_attempt(time_sync_service_t *service, bool success)
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

static err_t time_sync_recv_callback(void *arg, struct altcp_pcb *pcb, struct pbuf *p, err_t err)
{
    LWIP_UNUSED_ARG(err);
    time_sync_service_t *service = (time_sync_service_t *)arg;

    if (p == NULL || service == NULL)
    {
        return ERR_OK;
    }

    altcp_recved(pcb, p->tot_len);

    if (service->response_len + p->tot_len >= TIME_SYNC_RESPONSE_MAX)
    {
        service->response_overflow = true;
    }
    else
    {
        uint16_t copied = pbuf_copy_partial(p,
                                            service->response + service->response_len,
                                            p->tot_len,
                                            0);
        service->response_len += copied;
        service->response[service->response_len] = '\0';
    }

    pbuf_free(p);
    return ERR_OK;
}

static void time_sync_result_callback(void *arg,
                                      httpc_result_t httpc_result,
                                      u32_t rx_content_len,
                                      u32_t srv_res,
                                      err_t err)
{
    LWIP_UNUSED_ARG(rx_content_len);
    time_sync_service_t *service = (time_sync_service_t *)arg;
    if (service == NULL)
    {
        return;
    }

    service->connection = NULL;
    service->request_in_flight = false;

    bool http_success = (httpc_result == HTTPC_RESULT_OK) && (srv_res >= 200u) && (srv_res < 300u);
    bool sync_success = false;
    if (http_success && !service->response_overflow)
    {
        sync_success = apply_sync_response(service, device_clock_monotonic_ns());
    }

    if (!sync_success)
    {
        printf("Time sync failed for %s (result=%d http=%lu err=%d)\n",
               service->device_id,
               (int)httpc_result,
               (unsigned long)srv_res,
               (int)err);
    }

    schedule_after_attempt(service, sync_success);
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
        printf("Time sync URI is too long for device %s\n", service->device_id);
        return false;
    }

    memset(&service->connection_settings, 0, sizeof(service->connection_settings));
    service->connection_settings.result_fn = time_sync_result_callback;
    service->response_len = 0;
    service->response[0] = '\0';
    service->response_overflow = false;

    service->attempts_remaining--;
    cyw43_arch_lwip_begin();
    err_t err = httpc_get_file_dns(service->server_ip,
                                   TIME_SYNC_HTTP_PORT,
                                   uri,
                                   &service->connection_settings,
                                   time_sync_recv_callback,
                                   service,
                                   &service->connection);
    cyw43_arch_lwip_end();

    if (err != ERR_OK)
    {
        service->connection = NULL;
        printf("Time sync request start failed for %s (err=%d)\n", service->device_id, (int)err);
        schedule_after_attempt(service, false);
        return false;
    }

    service->request_in_flight = true;
    return true;
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
        printf("Invalid server host for time sync service: %s\n", service->server_ip);
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

    service->connection = NULL;
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
