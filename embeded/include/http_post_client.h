#ifndef HTTP_POST_CLIENT_H
#define HTTP_POST_CLIENT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef bool (*http_post_body_reader_t)(void *context, size_t offset, uint8_t *destination, size_t length);

typedef struct
{
    int status_code;
    const char *response;
    size_t response_len;
    bool response_overflow;
} http_post_result_t;

bool http_post_client_post(const char *host,
                           uint16_t port,
                           const char *uri,
                           const char *content_type,
                           size_t content_length,
                           http_post_body_reader_t body_reader,
                           void *body_reader_context,
                           char *response_buffer,
                           size_t response_buffer_size,
                           uint32_t timeout_ms,
                           http_post_result_t *result);

#endif
