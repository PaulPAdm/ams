#include "device_config.h"

#include <string.h>

#include "device_runtime_config.h"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs.h"

#define DEVICE_CONFIG_NVS_NAMESPACE "ams_config"
#define DEVICE_CONFIG_NVS_KEY "device_v1"

static const char *TAG = "device-config";

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

static bool config_has_required_fields(const device_config_t *config)
{
    return config != NULL &&
           config->ssid[0] != '\0' &&
           config->server_ip[0] != '\0' &&
           config->device_id[0] != '\0';
}

static void load_compile_time_defaults(device_config_t *config)
{
    memset(config, 0, sizeof(*config));
    strncpy(config->ssid, DEVICE_WIFI_SSID, DEVICE_CONFIG_SSID_MAX);
    strncpy(config->password, DEVICE_WIFI_PASSWORD, DEVICE_CONFIG_PASSWORD_MAX);
    strncpy(config->server_ip, DEVICE_SERVER_HOST, DEVICE_CONFIG_SERVER_IP_MAX);
    strncpy(config->device_id, DEVICE_ID, DEVICE_CONFIG_ID_MAX);
    audio_calibration_set_defaults(&config->audio_calibration);
    terminate_config_strings(config);
}

bool device_config_load(device_config_t *config)
{
    if (config == NULL)
    {
        return false;
    }

    memset(config, 0, sizeof(*config));

    nvs_handle_t handle;
    esp_err_t err = nvs_open(DEVICE_CONFIG_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_OK)
    {
        size_t required = sizeof(*config);
        err = nvs_get_blob(handle, DEVICE_CONFIG_NVS_KEY, config, &required);
        nvs_close(handle);
        if (err == ESP_OK && required == sizeof(*config))
        {
            terminate_config_strings(config);
            normalize_audio_calibration(config);
            return config_has_required_fields(config);
        }
        ESP_LOGW(TAG, "NVS config read failed: %s", esp_err_to_name(err));
    }
    else if (err != ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(TAG, "NVS config open failed: %s", esp_err_to_name(err));
    }

    load_compile_time_defaults(config);
    bool defaults_valid = strcmp(config->ssid, "CHANGE_ME") != 0 && config_has_required_fields(config);
    return defaults_valid;
}

bool device_config_save(const device_config_t *config)
{
    if (!config_has_required_fields(config))
    {
        return false;
    }

    device_config_t sanitized_config;
    memset(&sanitized_config, 0, sizeof(sanitized_config));
    memcpy(&sanitized_config, config, sizeof(sanitized_config));
    terminate_config_strings(&sanitized_config);
    normalize_audio_calibration(&sanitized_config);

    nvs_handle_t handle;
    esp_err_t err = nvs_open(DEVICE_CONFIG_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "NVS config open for write failed: %s", esp_err_to_name(err));
        return false;
    }

    err = nvs_set_blob(handle, DEVICE_CONFIG_NVS_KEY, &sanitized_config, sizeof(sanitized_config));
    if (err == ESP_OK)
    {
        err = nvs_commit(handle);
    }
    nvs_close(handle);

    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "NVS config save failed: %s", esp_err_to_name(err));
        return false;
    }

    return true;
}
