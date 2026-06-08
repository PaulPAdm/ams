#include "sound_event_uploader.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "esp_http_client.h"
#include "esp_log.h"

#define SOUND_EVENT_HTTP_PORT 80u
#define SOUND_EVENT_URI_MAX 192u
#define SOUND_EVENT_URL_MAX 320u
#define SOUND_EVENT_JSON_MAX 640u
#define SOUND_EVENT_RESPONSE_MAX 2048u
#define SOUND_EVENT_DETECTOR_VERSION "embedded-spectral-v2"
#define SOUND_EVENT_AUDIO_CONTENT_TYPE "application/vnd.ams.pcm16"

typedef struct
{
    char *response;
    size_t response_size;
    size_t response_len;
    bool response_overflow;
} http_response_context_t;

static const char *TAG = "sound-upload";

static const char *response_body_start(const char *response)
{
    if (response == NULL)
    {
        return NULL;
    }

    const char *body = strstr(response, "\r\n\r\n");
    return body == NULL ? response : body + 4;
}

static bool parse_u32_json_field(const char *json, const char *field_name, uint32_t *out_value)
{
    if (json == NULL || field_name == NULL || out_value == NULL)
    {
        return false;
    }

    char pattern[32];
    int written = snprintf(pattern, sizeof(pattern), "\"%s\"", field_name);
    if (written <= 0 || written >= (int)sizeof(pattern))
    {
        return false;
    }

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

    uint32_t value = 0u;
    while (isdigit((unsigned char)*cursor))
    {
        value = (value * 10u) + (uint32_t)(*cursor - '0');
        cursor++;
    }

    *out_value = value;
    return true;
}

static bool build_url(const device_config_t *config, const char *uri, char *url, size_t url_size)
{
    int written = snprintf(url,
                           url_size,
                           "http://%s:%u%s",
                           config->server_ip,
                           SOUND_EVENT_HTTP_PORT,
                           uri);
    return written > 0 && written < (int)url_size;
}

static esp_err_t response_event_handler(esp_http_client_event_t *evt)
{
    http_response_context_t *context = (http_response_context_t *)evt->user_data;
    if (context == NULL || evt->event_id != HTTP_EVENT_ON_DATA || evt->data == NULL || evt->data_len <= 0)
    {
        return ESP_OK;
    }

    if (context->response_len + (size_t)evt->data_len >= context->response_size)
    {
        context->response_overflow = true;
        return ESP_OK;
    }

    memcpy(context->response + context->response_len, evt->data, (size_t)evt->data_len);
    context->response_len += (size_t)evt->data_len;
    context->response[context->response_len] = '\0';
    return ESP_OK;
}

static bool post_memory_body(const device_config_t *config,
                             const char *uri,
                             const char *content_type,
                             const char *body,
                             uint32_t timeout_ms,
                             char *response,
                             size_t response_size,
                             int *status_code,
                             bool *response_overflow)
{
    char url[SOUND_EVENT_URL_MAX];
    if (!build_url(config, uri, url, sizeof(url)))
    {
        return false;
    }

    http_response_context_t context = {
        .response = response,
        .response_size = response_size,
    };
    if (response_size > 0u)
    {
        response[0] = '\0';
    }

    esp_http_client_config_t http_config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .event_handler = response_event_handler,
        .user_data = &context,
        .timeout_ms = (int)timeout_ms,
        .disable_auto_redirect = true,
    };

    esp_http_client_handle_t client = esp_http_client_init(&http_config);
    if (client == NULL)
    {
        return false;
    }

    esp_http_client_set_header(client, "Content-Type", content_type);
    esp_http_client_set_post_field(client, body, (int)strlen(body));
    esp_err_t ret = esp_http_client_perform(client);
    int http_status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (status_code != NULL)
    {
        *status_code = http_status;
    }
    if (response_overflow != NULL)
    {
        *response_overflow = context.response_overflow;
    }

    return ret == ESP_OK && http_status >= 200 && http_status < 300 && !context.response_overflow;
}

static bool post_sd_audio_body(const device_config_t *config,
                               const char *uri,
                               sd_card_buffer_t *sd_card_buffer,
                               uint32_t audio_blocks,
                               uint32_t audio_size_bytes,
                               uint32_t timeout_ms,
                               int *status_code)
{
    char url[SOUND_EVENT_URL_MAX];
    if (!build_url(config, uri, url, sizeof(url)))
    {
        return false;
    }

    esp_http_client_config_t http_config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = (int)timeout_ms,
        .disable_auto_redirect = true,
    };

    esp_http_client_handle_t client = esp_http_client_init(&http_config);
    if (client == NULL)
    {
        return false;
    }

    esp_http_client_set_header(client, "Content-Type", SOUND_EVENT_AUDIO_CONTENT_TYPE);
    esp_err_t ret = esp_http_client_open(client, (int)audio_size_bytes);
    if (ret != ESP_OK)
    {
        esp_http_client_cleanup(client);
        return false;
    }

    uint8_t block[SD_CARD_BLOCK_SIZE];
    bool ok = true;
    uint32_t bytes_remaining = audio_size_bytes;
    for (uint32_t block_index = 0; block_index < audio_blocks && bytes_remaining > 0u; ++block_index)
    {
        if (!sd_card_buffer_read_snapshot_block(sd_card_buffer, block_index, block))
        {
            ok = false;
            break;
        }

        uint32_t to_write = bytes_remaining < SD_CARD_BLOCK_SIZE ? bytes_remaining : SD_CARD_BLOCK_SIZE;
        int written = esp_http_client_write(client, (const char *)block, (int)to_write);
        if (written != (int)to_write)
        {
            ok = false;
            break;
        }
        bytes_remaining -= to_write;
    }

    if (ok)
    {
        esp_http_client_fetch_headers(client);
        char drain[128];
        while (esp_http_client_read(client, drain, sizeof(drain)) > 0)
        {
        }
    }

    int http_status = esp_http_client_get_status_code(client);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (status_code != NULL)
    {
        *status_code = http_status;
    }

    return ok && http_status >= 200 && http_status < 300;
}

bool sound_event_uploader_upload(const device_config_t *config,
                                 sd_card_buffer_t *sd_card_buffer,
                                 const sound_event_upload_t *event,
                                 uint32_t timeout_ms)
{
    if (config == NULL || sd_card_buffer == NULL || event == NULL)
    {
        ESP_LOGW(TAG, "Sound event upload skipped: missing config, SD buffer, or event");
        return false;
    }

    if (config->server_ip[0] == '\0' || config->device_id[0] == '\0')
    {
        ESP_LOGW(TAG, "Sound event upload skipped: server host or device id is empty");
        return false;
    }

    if (!sd_card_buffer_is_ready(sd_card_buffer))
    {
        ESP_LOGW(TAG, "Sound event upload skipped: SD buffer is not ready");
        return false;
    }

    if (!sd_card_buffer_finalize_snapshot(sd_card_buffer))
    {
        ESP_LOGW(TAG, "Sound event upload skipped: SD snapshot finalize failed: %s",
                 sd_card_buffer_last_error(sd_card_buffer));
        return false;
    }

    uint32_t audio_blocks = sd_card_buffer_snapshot_block_count(sd_card_buffer);
    uint32_t audio_size_bytes = sd_card_buffer_snapshot_size_bytes(sd_card_buffer);
    uint32_t duration_ms = sd_card_buffer_snapshot_duration_ms(sd_card_buffer);
    if (audio_blocks == 0u || audio_size_bytes == 0u || duration_ms == 0u)
    {
        ESP_LOGW(TAG, "Sound event upload skipped: SD snapshot is empty");
        return false;
    }

    char uri[SOUND_EVENT_URI_MAX];
    int uri_len = snprintf(uri,
                           sizeof(uri),
                           "/api/devices/%s/sound-events",
                           config->device_id);
    if (uri_len <= 0 || uri_len >= (int)sizeof(uri))
    {
        ESP_LOGW(TAG, "Sound event metadata URI is too long");
        return false;
    }

    char json[SOUND_EVENT_JSON_MAX];
    int json_len = snprintf(json,
                            sizeof(json),
                            "{\"event_time_ns\":%llu,"
                            "\"duration_ms\":%lu,"
                            "\"pre_event_ms\":%lu,"
                            "\"post_event_ms\":0,"
                            "\"sample_rate_hz\":%u,"
                            "\"channels\":1,"
                            "\"sample_format\":\"pcm16le\","
                            "\"peak_level\":%u,"
                            "\"rms_level\":%u,"
                            "\"noise_floor\":%lu,"
                            "\"detector_version\":\"%s\","
                            "\"detection_label\":\"impulse\"}",
                            (unsigned long long)event->event_time_ns,
                            (unsigned long)duration_ms,
                            (unsigned long)duration_ms,
                            (unsigned)SD_CARD_AUDIO_SAMPLE_RATE_HZ,
                            (unsigned)event->peak_abs,
                            (unsigned)event->mean_abs,
                            (unsigned long)event->noise_floor_abs,
                            SOUND_EVENT_DETECTOR_VERSION);
    if (json_len <= 0 || json_len >= (int)sizeof(json))
    {
        ESP_LOGW(TAG, "Sound event metadata JSON is too long");
        return false;
    }

    static char response[SOUND_EVENT_RESPONSE_MAX];
    int status_code = 0;
    bool overflow = false;
    ESP_LOGI(TAG,
             "Registering sound event: audio=%lu bytes duration=%lu ms peak=%u",
             (unsigned long)audio_size_bytes,
             (unsigned long)duration_ms,
             (unsigned)event->peak_abs);

    if (!post_memory_body(config,
                          uri,
                          "application/json",
                          json,
                          timeout_ms,
                          response,
                          sizeof(response),
                          &status_code,
                          &overflow))
    {
        ESP_LOGW(TAG,
                 "Sound event metadata POST failed: http=%d overflow=%s",
                 status_code,
                 overflow ? "yes" : "no");
        return false;
    }

    uint32_t event_id = 0u;
    if (!parse_u32_json_field(response_body_start(response), "id", &event_id) || event_id == 0u)
    {
        ESP_LOGW(TAG, "Sound event metadata response did not contain id: http=%d body=%s",
                 status_code,
                 response);
        return false;
    }

    uri_len = snprintf(uri,
                       sizeof(uri),
                       "/api/devices/%s/sound-events/%lu/audio",
                       config->device_id,
                       (unsigned long)event_id);
    if (uri_len <= 0 || uri_len >= (int)sizeof(uri))
    {
        ESP_LOGW(TAG, "Sound event audio URI is too long");
        return false;
    }

    if (!post_sd_audio_body(config,
                            uri,
                            sd_card_buffer,
                            audio_blocks,
                            audio_size_bytes,
                            timeout_ms,
                            &status_code))
    {
        ESP_LOGW(TAG,
                 "Sound event audio POST failed: event_id=%lu http=%d sd_error=%s",
                 (unsigned long)event_id,
                 status_code,
                 sd_card_buffer_last_error(sd_card_buffer));
        return false;
    }

    ESP_LOGI(TAG,
             "Sound event uploaded: event_id=%lu audio=%lu bytes duration=%lu ms",
             (unsigned long)event_id,
             (unsigned long)audio_size_bytes,
             (unsigned long)duration_ms);
    return true;
}
