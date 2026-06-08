#ifndef DEVICE_CLOCK_H
#define DEVICE_CLOCK_H

#include <stdbool.h>
#include <stdint.h>

#define DEVICE_CLOCK_TIMEZONE_NAME_MAX 31u

void device_clock_reset(void);
uint64_t device_clock_monotonic_ns(void);
uint64_t device_clock_apply_sync(uint64_t client_sent_monotonic_ns,
                                 uint64_t client_received_monotonic_ns,
                                 uint64_t server_received_epoch_ns,
                                 uint64_t server_transmit_epoch_ns,
                                 int32_t timezone_offset_seconds,
                                 const char *timezone_name);
bool device_clock_is_synced(void);
uint64_t device_clock_now_utc_ns(void);
uint64_t device_clock_now_local_ns(void);
int64_t device_clock_utc_offset_ns(void);
uint64_t device_clock_last_rtt_ns(void);
int32_t device_clock_timezone_offset_seconds(void);
const char *device_clock_timezone_name(void);

#endif
