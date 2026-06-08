#include <stdint.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "hardware/address_mapped.h"
#include "device_config.h"

#define DEVICE_CONFIG_MAGIC 0x43464731u
#define DEVICE_CONFIG_VERSION 5u
#define DEVICE_CONFIG_FLASH_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)
#define DEVICE_CONFIG_STORAGE_BYTES (((sizeof(device_config_record_t) + FLASH_PAGE_SIZE - 1u) / FLASH_PAGE_SIZE) * FLASH_PAGE_SIZE)

typedef struct
{
    uint32_t magic;
    uint32_t version;
    uint32_t crc32;
    device_config_t config;
} device_config_record_t;

typedef struct
{
    char ssid[DEVICE_CONFIG_SSID_MAX + 1];
    char password[DEVICE_CONFIG_PASSWORD_MAX + 1];
    char server_ip[DEVICE_CONFIG_SERVER_IP_MAX + 1];
    char device_id[DEVICE_CONFIG_ID_MAX + 1];
} device_config_legacy_v3_t;

typedef struct
{
    uint32_t magic;
    uint32_t version;
    uint32_t crc32;
    device_config_legacy_v3_t config;
} device_config_record_legacy_v3_t;

/* v4 layout: same as v5 minus the configurable interval fields. */
typedef struct
{
    char ssid[DEVICE_CONFIG_SSID_MAX + 1];
    char password[DEVICE_CONFIG_PASSWORD_MAX + 1];
    char server_ip[DEVICE_CONFIG_SERVER_IP_MAX + 1];
    char device_id[DEVICE_CONFIG_ID_MAX + 1];
    audio_calibration_t audio_calibration;
} device_config_legacy_v4_t;

typedef struct
{
    uint32_t magic;
    uint32_t version;
    uint32_t crc32;
    device_config_legacy_v4_t config;
} device_config_record_legacy_v4_t;

_Static_assert(sizeof(device_config_record_t) <= FLASH_SECTOR_SIZE, "Device config must fit in one flash sector");

static uint32_t crc32_compute(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i)
    {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit)
        {
            uint32_t mask = (uint32_t)-(int32_t)(crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

static const device_config_record_t *get_flash_record(void)
{
    return (const device_config_record_t *)(XIP_BASE + DEVICE_CONFIG_FLASH_OFFSET);
}

static void terminate_config_strings(device_config_t *config)
{
    config->ssid[DEVICE_CONFIG_SSID_MAX] = '\0';
    config->password[DEVICE_CONFIG_PASSWORD_MAX] = '\0';
    config->server_ip[DEVICE_CONFIG_SERVER_IP_MAX] = '\0';
    config->device_id[DEVICE_CONFIG_ID_MAX] = '\0';
}

static void normalize_audio_calibration(device_config_t *config)
{
    if (config == NULL)
    {
        return;
    }

    if (!audio_calibration_is_valid(&config->audio_calibration))
    {
        audio_calibration_set_defaults(&config->audio_calibration);
    }
}

static void normalize_intervals(device_config_t *config)
{
    if (config == NULL)
    {
        return;
    }

    if (config->health_report_interval_min < DEVICE_CONFIG_INTERVAL_MIN_MIN ||
        config->health_report_interval_min > DEVICE_CONFIG_INTERVAL_MIN_MAX)
    {
        config->health_report_interval_min = DEVICE_CONFIG_HEALTH_INTERVAL_MIN_DEFAULT;
    }

    if (config->time_sync_interval_min < DEVICE_CONFIG_INTERVAL_MIN_MIN ||
        config->time_sync_interval_min > DEVICE_CONFIG_INTERVAL_MIN_MAX)
    {
        config->time_sync_interval_min = DEVICE_CONFIG_TIME_SYNC_INTERVAL_MIN_DEFAULT;
    }
}

static bool config_has_required_fields(const device_config_t *config)
{
    return config->ssid[0] != '\0' &&
           config->server_ip[0] != '\0' &&
           config->device_id[0] != '\0';
}

static bool load_config_payload(device_config_t *config,
                                const void *payload,
                                size_t payload_size,
                                uint32_t expected_crc)
{
    if (crc32_compute((const uint8_t *)payload, payload_size) != expected_crc)
    {
        return false;
    }

    memset(config, 0, sizeof(*config));

    size_t bytes_to_copy = payload_size;
    if (bytes_to_copy > sizeof(*config))
    {
        bytes_to_copy = sizeof(*config);
    }
    memcpy(config, payload, bytes_to_copy);

    terminate_config_strings(config);
    normalize_audio_calibration(config);
    normalize_intervals(config);
    return config_has_required_fields(config);
}

static bool load_legacy_config_payload(device_config_t *config,
                                       const device_config_legacy_v3_t *payload,
                                       size_t payload_size,
                                       uint32_t expected_crc)
{
    if (config == NULL || payload == NULL ||
        payload_size != sizeof(*payload) ||
        crc32_compute((const uint8_t *)payload, payload_size) != expected_crc)
    {
        return false;
    }

    memset(config, 0, sizeof(*config));
    memcpy(config->ssid, payload->ssid, sizeof(config->ssid));
    memcpy(config->password, payload->password, sizeof(config->password));
    memcpy(config->server_ip, payload->server_ip, sizeof(config->server_ip));
    memcpy(config->device_id, payload->device_id, sizeof(config->device_id));
    terminate_config_strings(config);
    audio_calibration_set_defaults(&config->audio_calibration);
    normalize_intervals(config);
    return config_has_required_fields(config);
}

static bool load_legacy_v4_config_payload(device_config_t *config,
                                          const device_config_legacy_v4_t *payload,
                                          size_t payload_size,
                                          uint32_t expected_crc)
{
    if (config == NULL || payload == NULL ||
        payload_size != sizeof(*payload) ||
        crc32_compute((const uint8_t *)payload, payload_size) != expected_crc)
    {
        return false;
    }

    memset(config, 0, sizeof(*config));
    memcpy(config->ssid, payload->ssid, sizeof(config->ssid));
    memcpy(config->password, payload->password, sizeof(config->password));
    memcpy(config->server_ip, payload->server_ip, sizeof(config->server_ip));
    memcpy(config->device_id, payload->device_id, sizeof(config->device_id));
    config->audio_calibration = payload->audio_calibration;
    terminate_config_strings(config);
    normalize_audio_calibration(config);
    normalize_intervals(config); /* v4 had no intervals; apply defaults. */
    return config_has_required_fields(config);
}

static bool sanitize_config_for_storage(const device_config_t *config, device_config_t *sanitized_config)
{
    if (config == NULL || sanitized_config == NULL)
    {
        return false;
    }

    memset(sanitized_config, 0, sizeof(*sanitized_config));
    memcpy(sanitized_config, config, sizeof(*sanitized_config));
    terminate_config_strings(sanitized_config);
    normalize_audio_calibration(sanitized_config);
    normalize_intervals(sanitized_config);
    return config_has_required_fields(sanitized_config);
}

bool device_config_load(device_config_t *config)
{
    if (config == NULL)
    {
        return false;
    }

    const device_config_record_t *record = get_flash_record();
    if (record->magic != DEVICE_CONFIG_MAGIC)
    {
        return false;
    }

    if (record->version >= 1u && record->version <= 3u)
    {
        const device_config_record_legacy_v3_t *legacy_record = (const device_config_record_legacy_v3_t *)record;
        return load_legacy_config_payload(config,
                                          &legacy_record->config,
                                          sizeof(legacy_record->config),
                                          legacy_record->crc32);
    }

    if (record->version == 4u)
    {
        const device_config_record_legacy_v4_t *legacy_record = (const device_config_record_legacy_v4_t *)record;
        return load_legacy_v4_config_payload(config,
                                             &legacy_record->config,
                                             sizeof(legacy_record->config),
                                             legacy_record->crc32);
    }

    if (record->version != DEVICE_CONFIG_VERSION)
    {
        return false;
    }

    return load_config_payload(config, &record->config, sizeof(record->config), record->crc32);
}

bool device_config_save(const device_config_t *config)
{
    device_config_t sanitized_config;
    if (!sanitize_config_for_storage(config, &sanitized_config))
    {
        return false;
    }

    device_config_record_t record;
    memset(&record, 0, sizeof(record));
    record.magic = DEVICE_CONFIG_MAGIC;
    record.version = DEVICE_CONFIG_VERSION;
    memcpy(&record.config, &sanitized_config, sizeof(record.config));
    terminate_config_strings(&record.config);
    record.crc32 = crc32_compute((const uint8_t *)&record.config, sizeof(record.config));

    uint8_t pages[DEVICE_CONFIG_STORAGE_BYTES];
    memset(pages, 0xFF, sizeof(pages));
    memcpy(pages, &record, sizeof(record));

    uint32_t irq_state = save_and_disable_interrupts();
    flash_range_erase(DEVICE_CONFIG_FLASH_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(DEVICE_CONFIG_FLASH_OFFSET, pages, sizeof(pages));
    restore_interrupts(irq_state);

    return true;
}
