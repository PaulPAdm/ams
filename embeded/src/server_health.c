#include "server_health.h"

#include <stdio.h>
#include <string.h>

#include "device_clock.h"
#include "device_runtime_config.h"
#include "lwip/altcp.h"
#include "pico/cyw43_arch.h"

#define SERVER_HEALTH_URI_MAX 640
#define SERVER_HEALTH_FIRMWARE_VERSION "pico2w-embedded-1"

static void drop_oldest_sample(server_health_service_t *service)
{
    if (service->sample_count == 0u)
    {
        return;
    }

    service->sample_tail = (uint16_t)((service->sample_tail + 1u) % SERVER_HEALTH_BUFFER_CAPACITY);
    service->sample_count--;
}

static err_t server_health_recv_callback(void *arg, struct altcp_pcb *pcb, struct pbuf *p, err_t err)
{
    LWIP_UNUSED_ARG(arg);
    LWIP_UNUSED_ARG(err);

    if (p == NULL)
    {
        return ERR_OK;
    }

    altcp_recved(pcb, p->tot_len);
    pbuf_free(p);
    return ERR_OK;
}

static void schedule_next_report(server_health_service_t *service, uint32_t delay_ms)
{
    service->next_report_at = make_timeout_time_ms(delay_ms);
}

static void server_health_result_callback(void *arg,
                                          httpc_result_t httpc_result,
                                          u32_t rx_content_len,
                                          u32_t srv_res,
                                          err_t err)
{
    LWIP_UNUSED_ARG(rx_content_len);
    server_health_service_t *service = (server_health_service_t *)arg;
    if (service == NULL)
    {
        return;
    }

    service->connection = NULL;
    service->request_in_flight = false;

    bool success = (httpc_result == HTTPC_RESULT_OK) && (srv_res >= 200u) && (srv_res < 300u);

    if (success)
    {
        service->connection_status = SERVER_HEALTH_STATUS_CONNECTED;
        // The oldest sample was delivered; drop it and allow the next one to
        // flush immediately so a backlog drains back-to-back.
        drop_oldest_sample(service);
        service->next_report_at = get_absolute_time();
        printf("Health report sent for %s (%u buffered remaining)\n",
               service->device_id,
               (unsigned int)service->sample_count);
    }
    else
    {
        service->connection_status = SERVER_HEALTH_STATUS_ERROR;
        printf("Health report failed for %s (result=%d http=%lu err=%d)\n",
               service->device_id,
               (int)httpc_result,
               (unsigned long)srv_res,
               (int)err);
        // Keep the sample buffered and back off a full interval before waking
        // again, so an unreachable server does not thrash the Wi-Fi radio.
        schedule_next_report(service, service->report_interval_ms);
    }

    service->active_request = SERVER_HEALTH_REQUEST_NONE;
}

static bool server_health_start_request(server_health_service_t *service,
                                        server_health_request_kind_t kind,
                                        const char *uri)
{
    if (service == NULL || uri == NULL || !service->server_addr_valid || service->request_in_flight)
    {
        return false;
    }

    memset(&service->connection_settings, 0, sizeof(service->connection_settings));
    service->connection_settings.result_fn = server_health_result_callback;
    service->active_request = kind;

    cyw43_arch_lwip_begin();
    err_t err = httpc_get_file_dns(service->server_ip,
                                   SERVER_HEALTH_HTTP_PORT,
                                   uri,
                                   &service->connection_settings,
                                   server_health_recv_callback,
                                   service,
                                   &service->connection);
    cyw43_arch_lwip_end();

    if (err != ERR_OK)
    {
        service->active_request = SERVER_HEALTH_REQUEST_NONE;
        service->connection_status = SERVER_HEALTH_STATUS_ERROR;
        printf("HTTP request start failed for %s (err=%d, uri=%s)\n", service->device_id, (int)err, uri);
        return false;
    }

    service->request_in_flight = true;
    return true;
}

static void capture_health_sample(server_health_service_t *service,
                                  const power_meter_service_t *power_meter_service,
                                  bool wifi_connected,
                                  uint16_t audio_queue_depth,
                                  uint32_t audio_dropped_chunks)
{
    ina219_wattmeter_reading_t reading;
    bool has_reading = power_meter_service_get_latest_reading(power_meter_service, &reading);
    bool ina219_online = power_meter_service_is_sensor_online(power_meter_service);

    // When the ring is full, evict the oldest sample to keep the most recent
    // history. Never evict a sample that is currently being transmitted.
    if (service->sample_count >= SERVER_HEALTH_BUFFER_CAPACITY)
    {
        if (service->request_in_flight)
        {
            return;
        }
        drop_oldest_sample(service);
    }

    server_health_sample_t *slot = &service->samples[service->sample_head];
    memset(slot, 0, sizeof(*slot));
    slot->captured_at_ns = device_clock_now_utc_ns();
    slot->uptime_ms = time_us_64() / 1000ull;
    slot->wifi_connected = wifi_connected;
    slot->microphone_active = service->microphone_active;
    slot->ina219_online = ina219_online;
    slot->audio_queue_depth = audio_queue_depth;
    slot->audio_dropped_chunks = audio_dropped_chunks;
    slot->has_power = has_reading;
    if (has_reading)
    {
        slot->bus_voltage_v = reading.bus_voltage_v;
        slot->shunt_voltage_mv = reading.shunt_voltage_mv;
        slot->current_ma = reading.current_ma;
        slot->power_mw = reading.power_mw;
        slot->computed_power_mw = reading.computed_power_mw;
    }

    service->sample_head = (uint16_t)((service->sample_head + 1u) % SERVER_HEALTH_BUFFER_CAPACITY);
    service->sample_count++;
}

static void server_health_send_oldest(server_health_service_t *service)
{
    if (service->sample_count == 0u)
    {
        return;
    }

    const server_health_sample_t *sample = &service->samples[service->sample_tail];
    const char *status_message = sample->microphone_active ? "streaming" : "network_ready";

    char uri[SERVER_HEALTH_URI_MAX];
    int written;
    if (sample->has_power)
    {
        written = snprintf(
            uri,
            sizeof(uri),
            "/api/devices/%s/health/report?firmware_version=%s&status_message=%s&uptime_ms=%llu&wifi_connected=%s&microphone_active=%s&ina219_online=true&bus_voltage_v=%.3f&shunt_voltage_mv=%.3f&current_ma=%.3f&power_mw=%.3f&computed_power_mw=%.3f&audio_queue_depth=%u&audio_dropped_chunks=%lu&captured_at_ns=%llu",
            service->device_id,
            SERVER_HEALTH_FIRMWARE_VERSION,
            status_message,
            (unsigned long long)sample->uptime_ms,
            sample->wifi_connected ? "true" : "false",
            sample->microphone_active ? "true" : "false",
            sample->bus_voltage_v,
            sample->shunt_voltage_mv,
            sample->current_ma,
            sample->power_mw,
            sample->computed_power_mw,
            (unsigned int)sample->audio_queue_depth,
            (unsigned long)sample->audio_dropped_chunks,
            (unsigned long long)sample->captured_at_ns);
    }
    else
    {
        // Send "unknown" for all power fields so the backend stores NULL
        // instead of 0.0, allowing the frontend to display "Power: Unknown".
        written = snprintf(
            uri,
            sizeof(uri),
            "/api/devices/%s/health/report?firmware_version=%s&status_message=%s&uptime_ms=%llu&wifi_connected=%s&microphone_active=%s&ina219_online=%s&bus_voltage_v=unknown&shunt_voltage_mv=unknown&current_ma=unknown&power_mw=unknown&computed_power_mw=unknown&audio_queue_depth=%u&audio_dropped_chunks=%lu&captured_at_ns=%llu",
            service->device_id,
            SERVER_HEALTH_FIRMWARE_VERSION,
            status_message,
            (unsigned long long)sample->uptime_ms,
            sample->wifi_connected ? "true" : "false",
            sample->microphone_active ? "true" : "false",
            sample->ina219_online ? "true" : "false",
            (unsigned int)sample->audio_queue_depth,
            (unsigned long)sample->audio_dropped_chunks,
            (unsigned long long)sample->captured_at_ns);
    }

    if (written <= 0 || written >= (int)sizeof(uri))
    {
        printf("Health URI is too long for device %s\n", service->device_id);
        // Drop the malformed sample so the buffer does not get stuck on it.
        drop_oldest_sample(service);
        return;
    }

    if (!server_health_start_request(service, SERVER_HEALTH_REQUEST_REPORT, uri))
    {
        schedule_next_report(service, service->report_interval_ms);
    }
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
    service->report_interval_ms = config->health_report_interval_min != 0u
                                      ? config->health_report_interval_min * 60000u
                                      : DEVICE_HEALTH_REPORT_INTERVAL_MS;
    service->next_report_at = get_absolute_time();
    service->next_capture_at = get_absolute_time();
    service->connection_status = SERVER_HEALTH_STATUS_PENDING;

    if (!service->server_addr_valid)
    {
        service->connection_status = SERVER_HEALTH_STATUS_ERROR;
        printf("Invalid server host for health service: %s\n", service->server_ip);
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
           service->sample_count > 0u &&
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

    service->connection = NULL;
    service->request_in_flight = false;
    service->active_request = SERVER_HEALTH_REQUEST_NONE;
    service->connection_status = SERVER_HEALTH_STATUS_ERROR;
    schedule_next_report(service, service->report_interval_ms);
}

void server_health_service_poll(server_health_service_t *service,
                                const power_meter_service_t *power_meter_service,
                                bool wifi_connected,
                                uint16_t audio_queue_depth,
                                uint32_t audio_dropped_chunks)
{
    if (service == NULL || !service->server_addr_valid)
    {
        return;
    }

    // Log a health sample on the configured cadence regardless of connectivity,
    // so offline periods are captured and flushed later.
    if (time_reached(service->next_capture_at))
    {
        capture_health_sample(service,
                              power_meter_service,
                              wifi_connected,
                              audio_queue_depth,
                              audio_dropped_chunks);
        service->next_capture_at = make_timeout_time_ms(service->report_interval_ms);
    }

    // Drain buffered samples whenever connected. Each poll sends the oldest one;
    // the result callback advances to the next, so a backlog flushes in order.
    if (service->request_in_flight || !wifi_connected || service->sample_count == 0u)
    {
        return;
    }

    if (time_reached(service->next_report_at))
    {
        server_health_send_oldest(service);
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
