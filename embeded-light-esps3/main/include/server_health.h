#ifndef SERVER_HEALTH_H
#define SERVER_HEALTH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "device_config.h"
#include "platform_time.h"

#define SERVER_HEALTH_HTTP_PORT 80

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
    absolute_time_t next_report_at;
    char server_ip[DEVICE_CONFIG_SERVER_IP_MAX + 1];
    char device_id[DEVICE_CONFIG_ID_MAX + 1];
    server_health_request_kind_t active_request;
    server_health_connection_status_t connection_status;
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
                                bool wifi_connected,
                                uint16_t audio_queue_depth,
                                uint32_t audio_dropped_chunks);
server_health_connection_status_t server_health_service_connection_status(const server_health_service_t *service);

#endif
