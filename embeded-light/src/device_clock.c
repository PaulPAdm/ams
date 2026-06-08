#include "device_clock.h"

#include <limits.h>
#include <string.h>

#include "pico/time.h"

typedef struct
{
    int64_t utc_offset_ns;
    uint64_t last_sync_monotonic_ns;
    uint64_t last_rtt_ns;
    int32_t timezone_offset_seconds;
    char timezone_name[DEVICE_CLOCK_TIMEZONE_NAME_MAX + 1u];
    bool synced;
} device_clock_state_t;

static device_clock_state_t g_device_clock;

void device_clock_reset(void)
{
    memset(&g_device_clock, 0, sizeof(g_device_clock));
}

uint64_t device_clock_monotonic_ns(void)
{
    return time_us_64() * 1000ull;
}

uint64_t device_clock_apply_sync(uint64_t client_sent_monotonic_ns,
                                 uint64_t client_received_monotonic_ns,
                                 uint64_t server_received_epoch_ns,
                                 uint64_t server_transmit_epoch_ns,
                                 int32_t timezone_offset_seconds,
                                 const char *timezone_name)
{
    int64_t receive_delta = (int64_t)server_received_epoch_ns - (int64_t)client_sent_monotonic_ns;
    int64_t transmit_delta = (int64_t)server_transmit_epoch_ns - (int64_t)client_received_monotonic_ns;
    g_device_clock.utc_offset_ns = (receive_delta + transmit_delta) / 2;
    g_device_clock.last_sync_monotonic_ns = client_received_monotonic_ns;
    g_device_clock.timezone_offset_seconds = timezone_offset_seconds;

    if (timezone_name != NULL && timezone_name[0] != '\0')
    {
        strncpy(g_device_clock.timezone_name, timezone_name, DEVICE_CLOCK_TIMEZONE_NAME_MAX);
        g_device_clock.timezone_name[DEVICE_CLOCK_TIMEZONE_NAME_MAX] = '\0';
    }

    uint64_t client_elapsed_ns = client_received_monotonic_ns - client_sent_monotonic_ns;
    uint64_t server_elapsed_ns = server_transmit_epoch_ns - server_received_epoch_ns;
    g_device_clock.last_rtt_ns = client_elapsed_ns > server_elapsed_ns ? client_elapsed_ns - server_elapsed_ns : 0;
    g_device_clock.synced = true;
    return g_device_clock.last_rtt_ns;
}

bool device_clock_is_synced(void)
{
    return g_device_clock.synced;
}

uint64_t device_clock_now_utc_ns(void)
{
    if (!g_device_clock.synced)
    {
        return 0;
    }

    int64_t utc_ns = (int64_t)device_clock_monotonic_ns() + g_device_clock.utc_offset_ns;
    return utc_ns > 0 ? (uint64_t)utc_ns : 0;
}

uint64_t device_clock_now_local_ns(void)
{
    uint64_t utc_ns = device_clock_now_utc_ns();
    if (utc_ns == 0)
    {
        return 0;
    }

    int64_t local_ns = (int64_t)utc_ns + ((int64_t)g_device_clock.timezone_offset_seconds * 1000000000ll);
    return local_ns > 0 ? (uint64_t)local_ns : 0;
}

int64_t device_clock_utc_offset_ns(void)
{
    return g_device_clock.utc_offset_ns;
}

uint64_t device_clock_last_rtt_ns(void)
{
    return g_device_clock.last_rtt_ns;
}

int32_t device_clock_timezone_offset_seconds(void)
{
    return g_device_clock.timezone_offset_seconds;
}

const char *device_clock_timezone_name(void)
{
    return g_device_clock.timezone_name;
}
