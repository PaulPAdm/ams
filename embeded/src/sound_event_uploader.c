#include "sound_event_uploader.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "http_post_client.h"

#define SOUND_EVENT_HTTP_PORT 80u
#define SOUND_EVENT_URI_MAX 192u
#define SOUND_EVENT_JSON_MAX 640u
#define SOUND_EVENT_RESPONSE_MAX 2048u
#define SOUND_EVENT_DETECTOR_VERSION "embedded-spectral-v2"
#define SOUND_EVENT_AUDIO_CONTENT_TYPE "application/vnd.ams.pcm16"

typedef struct
{
    const uint8_t *data;
    size_t length;
} memory_body_context_t;

typedef struct
{
    sd_card_buffer_t *buffer;
    uint32_t block_count;
    uint32_t cached_block_index;
    uint8_t cached_block[SD_CARD_BLOCK_SIZE];
    bool cached_block_valid;
} sd_audio_body_context_t;

static bool memory_body_reader(void *context, size_t offset, uint8_t *destination, size_t length)
{
    memory_body_context_t *body = (memory_body_context_t *)context;
    if (body == NULL || destination == NULL || offset + length > body->length)
    {
        return false;
    }

    memcpy(destination, body->data + offset, length);
    return true;
}

static bool sd_audio_body_reader(void *context, size_t offset, uint8_t *destination, size_t length)
{
    sd_audio_body_context_t *body = (sd_audio_body_context_t *)context;
    if (body == NULL || body->buffer == NULL || destination == NULL)
    {
        return false;
    }

    size_t copied = 0u;
    while (copied < length)
    {
        size_t absolute_offset = offset + copied;
        uint32_t block_index = (uint32_t)(absolute_offset / SD_CARD_BLOCK_SIZE);
        size_t block_offset = absolute_offset % SD_CARD_BLOCK_SIZE;
        if (block_index >= body->block_count)
        {
            return false;
        }

        if (!body->cached_block_valid || body->cached_block_index != block_index)
        {
            if (!sd_card_buffer_read_snapshot_block(body->buffer, block_index, body->cached_block))
            {
                return false;
            }
            body->cached_block_index = block_index;
            body->cached_block_valid = true;
        }

        size_t available = SD_CARD_BLOCK_SIZE - block_offset;
        size_t copy_len = (length - copied) < available ? (length - copied) : available;
        memcpy(destination + copied, body->cached_block + block_offset, copy_len);
        copied += copy_len;
    }

    return true;
}

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

static bool post_memory_body(const device_config_t *config,
                             const char *uri,
                             const char *content_type,
                             const char *body,
                             uint32_t timeout_ms,
                             char *response,
                             size_t response_size,
                             http_post_result_t *result)
{
    memory_body_context_t context = {
        .data = (const uint8_t *)body,
        .length = strlen(body),
    };

    return http_post_client_post(config->server_ip,
                                 SOUND_EVENT_HTTP_PORT,
                                 uri,
                                 content_type,
                                 context.length,
                                 memory_body_reader,
                                 &context,
                                 response,
                                 response_size,
                                 timeout_ms,
                                 result);
}

bool sound_event_uploader_upload(const device_config_t *config,
                                 sd_card_buffer_t *sd_card_buffer,
                                 const sound_event_upload_t *event,
                                 uint32_t timeout_ms)
{
    if (config == NULL || sd_card_buffer == NULL || event == NULL)
    {
        printf("[Upload] Sound event upload skipped: missing config, SD buffer, or event\n");
        return false;
    }

    if (config->server_ip[0] == '\0' || config->device_id[0] == '\0')
    {
        printf("[Upload] Sound event upload skipped: server host or device id is empty\n");
        return false;
    }

    if (!sd_card_buffer_is_ready(sd_card_buffer))
    {
        printf("[Upload] Sound event upload skipped: SD buffer is not ready\n");
        return false;
    }

    if (!sd_card_buffer_finalize_snapshot(sd_card_buffer))
    {
        printf("[Upload] Sound event upload skipped: SD snapshot finalize failed: %s\n",
               sd_card_buffer_last_error(sd_card_buffer));
        return false;
    }

    uint32_t audio_blocks = sd_card_buffer_snapshot_block_count(sd_card_buffer);
    uint32_t audio_size_bytes = sd_card_buffer_snapshot_size_bytes(sd_card_buffer);
    uint32_t duration_ms = sd_card_buffer_snapshot_duration_ms(sd_card_buffer);
    if (audio_blocks == 0u || audio_size_bytes == 0u || duration_ms == 0u)
    {
        printf("[Upload] Sound event upload skipped: SD snapshot is empty\n");
        return false;
    }

    char uri[SOUND_EVENT_URI_MAX];
    int uri_len = snprintf(uri,
                           sizeof(uri),
                           "/api/devices/%s/sound-events",
                           config->device_id);
    if (uri_len <= 0 || uri_len >= (int)sizeof(uri))
    {
        printf("[Upload] Sound event metadata URI is too long\n");
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
        printf("[Upload] Sound event metadata JSON is too long\n");
        return false;
    }

    static char response[SOUND_EVENT_RESPONSE_MAX];
    http_post_result_t post_result;
    printf("[Upload] Registering sound event: audio=%lu bytes duration=%lu ms peak=%u\n",
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
                          &post_result))
    {
        printf("[Upload] Sound event metadata POST failed: http=%d overflow=%s\n",
               post_result.status_code,
               post_result.response_overflow ? "yes" : "no");
        return false;
    }

    uint32_t event_id = 0u;
    if (!parse_u32_json_field(response_body_start(response), "id", &event_id) || event_id == 0u)
    {
        printf("[Upload] Sound event metadata response did not contain id: http=%d\n",
               post_result.status_code);
        return false;
    }

    uri_len = snprintf(uri,
                       sizeof(uri),
                       "/api/devices/%s/sound-events/%lu/audio",
                       config->device_id,
                       (unsigned long)event_id);
    if (uri_len <= 0 || uri_len >= (int)sizeof(uri))
    {
        printf("[Upload] Sound event audio URI is too long\n");
        return false;
    }

    static sd_audio_body_context_t audio_context;
    memset(&audio_context, 0, sizeof(audio_context));
    audio_context.buffer = sd_card_buffer;
    audio_context.block_count = audio_blocks;

    if (!http_post_client_post(config->server_ip,
                               SOUND_EVENT_HTTP_PORT,
                               uri,
                               SOUND_EVENT_AUDIO_CONTENT_TYPE,
                               audio_size_bytes,
                               sd_audio_body_reader,
                               &audio_context,
                               response,
                               sizeof(response),
                               timeout_ms,
                               &post_result))
    {
        printf("[Upload] Sound event audio POST failed: event_id=%lu http=%d overflow=%s sd_error=%s\n",
               (unsigned long)event_id,
               post_result.status_code,
               post_result.response_overflow ? "yes" : "no",
               sd_card_buffer_last_error(sd_card_buffer));
        return false;
    }

    printf("[Upload] Sound event uploaded: event_id=%lu audio=%lu bytes duration=%lu ms\n",
           (unsigned long)event_id,
           (unsigned long)audio_size_bytes,
           (unsigned long)duration_ms);
    return true;
}
