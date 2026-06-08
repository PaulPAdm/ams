#ifndef SERVER_HEALTH_H
#define SERVER_HEALTH_H

#include <stdbool.h>
#include <stdint.h>

#include "device_config.h"
#include "lwip/apps/http_client.h"
#include "pico/time.h"
#include "power_meter_service.h"

#define SERVER_HEALTH_HTTP_PORT 80

/* Capacity of the in-RAM ring buffer that retains health samples while the
 * device is offline so they can be flushed to the server in order later. */
#define SERVER_HEALTH_BUFFER_CAPACITY 180u

typedef struct
{
    uint64_t captured_at_ns;
    uint64_t uptime_ms;
    float bus_voltage_v;
    float shunt_voltage_mv;
    float current_ma;
    float power_mw;
    float computed_power_mw;
    uint32_t audio_dropped_chunks;
    uint16_t audio_queue_depth;
    bool has_power;
    bool ina219_online;
    bool wifi_connected;
    bool microphone_active;
} server_health_sample_t;

typedef enum
{
    SERVER_HEALTH_REQUEST_NONE = 0,
    SERVER_HEALTH_REQUEST_REPORT
} server_health_request_kind_t;

typedef enum
{
    SERVER_HEALTH_STATUS_PENDING = 0,
    SERVER_HEALTH_STATUS_CONNECTED,
    SERVER_HEALTH_STATUS_ERROR
} server_health_connection_status_t;

typedef struct
{
    ip_addr_t server_addr;
    httpc_connection_t connection_settings;
    httpc_state_t *connection;
    absolute_time_t next_report_at;
    absolute_time_t next_capture_at;
    char server_ip[DEVICE_CONFIG_SERVER_IP_MAX + 1];
    char device_id[DEVICE_CONFIG_ID_MAX + 1];
    server_health_request_kind_t active_request;
    server_health_connection_status_t connection_status;
    uint32_t report_interval_ms;
    server_health_sample_t samples[SERVER_HEALTH_BUFFER_CAPACITY];
    uint16_t sample_head;
    uint16_t sample_tail;
    uint16_t sample_count;
    bool server_addr_valid;
    bool request_in_flight;
    bool microphone_active;
} server_health_service_t;

void server_health_service_init(server_health_service_t *service, const device_config_t *config);
void server_health_service_set_microphone_active(server_health_service_t *service, bool active);
void server_health_service_request_report(server_health_service_t *service);
bool server_health_service_is_report_due(const server_health_service_t *service);
bool server_health_service_is_request_in_flight(const server_health_service_t *service);
void server_health_service_mark_timeout(server_health_service_t *service);
void server_health_service_poll(server_health_service_t *service,
                                const power_meter_service_t *power_meter_service,
                                bool wifi_connected,
                                uint16_t audio_queue_depth,
                                uint32_t audio_dropped_chunks);
server_health_connection_status_t server_health_service_connection_status(const server_health_service_t *service);

#endif
