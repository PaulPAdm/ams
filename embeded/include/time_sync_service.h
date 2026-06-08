#ifndef TIME_SYNC_SERVICE_H
#define TIME_SYNC_SERVICE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "device_config.h"
#include "lwip/apps/http_client.h"
#include "pico/time.h"

#define TIME_SYNC_HTTP_PORT 80
#define TIME_SYNC_RESPONSE_MAX 512u

typedef enum
{
    TIME_SYNC_STATUS_IDLE = 0,
    TIME_SYNC_STATUS_PENDING,
    TIME_SYNC_STATUS_SYNCED,
    TIME_SYNC_STATUS_ERROR
} time_sync_status_t;

typedef struct
{
    ip_addr_t server_addr;
    httpc_connection_t connection_settings;
    httpc_state_t *connection;
    absolute_time_t next_attempt_at;
    absolute_time_t next_sync_at;
    char server_ip[DEVICE_CONFIG_SERVER_IP_MAX + 1];
    char device_id[DEVICE_CONFIG_ID_MAX + 1];
    char response[TIME_SYNC_RESPONSE_MAX];
    size_t response_len;
    uint64_t client_sent_monotonic_ns;
    uint64_t best_rtt_ns;
    uint32_t sync_period_ms;
    uint8_t attempts_remaining;
    time_sync_status_t status;
    bool server_addr_valid;
    bool request_in_flight;
    bool response_overflow;
} time_sync_service_t;

void time_sync_service_init(time_sync_service_t *service, const device_config_t *config);
void time_sync_service_request_sync(time_sync_service_t *service);
void time_sync_service_poll(time_sync_service_t *service);
bool time_sync_service_is_due(const time_sync_service_t *service);
bool time_sync_service_has_pending_work(const time_sync_service_t *service);
void time_sync_service_mark_timeout(time_sync_service_t *service);
bool time_sync_service_is_synced(const time_sync_service_t *service);
time_sync_status_t time_sync_service_status(const time_sync_service_t *service);

#endif
