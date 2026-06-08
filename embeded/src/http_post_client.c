#include "http_post_client.h"

#include <stdio.h>
#include <string.h>

#include "lwip/altcp.h"
#include "lwip/dns.h"
#include "lwip/err.h"
#include "lwip/ip_addr.h"
#include "lwip/pbuf.h"
#include "lwip/tcpbase.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

#define HTTP_POST_HEADER_MAX 512u
#define HTTP_POST_SEND_BUFFER_SIZE 1024u
#define HTTP_POST_POLL_INTERVAL 2u

typedef struct
{
    const char *host;
    uint16_t port;
    const char *uri;
    const char *content_type;
    size_t content_length;
    http_post_body_reader_t body_reader;
    void *body_reader_context;
    char *response_buffer;
    size_t response_buffer_size;
    size_t response_len;
    char header[HTTP_POST_HEADER_MAX];
    size_t header_len;
    size_t header_sent;
    size_t body_sent;
    uint8_t send_buffer[HTTP_POST_SEND_BUFFER_SIZE];
    struct altcp_pcb *pcb;
    ip_addr_t address;
    int status_code;
    err_t lwip_error;
    bool done;
    bool success;
    bool response_overflow;
    bool connected;
    bool sending_complete;
} http_post_state_t;

static err_t http_post_try_send(http_post_state_t *state, struct altcp_pcb *pcb);

static int parse_status_code(const char *response, size_t response_len)
{
    if (response == NULL || response_len < 12u || memcmp(response, "HTTP/", 5u) != 0)
    {
        return 0;
    }

    const char *cursor = strchr(response, ' ');
    if (cursor == NULL || cursor + 4 > response + response_len)
    {
        return 0;
    }
    cursor++;

    if (cursor[0] < '0' || cursor[0] > '9' ||
        cursor[1] < '0' || cursor[1] > '9' ||
        cursor[2] < '0' || cursor[2] > '9')
    {
        return 0;
    }

    return ((int)(cursor[0] - '0') * 100) +
           ((int)(cursor[1] - '0') * 10) +
           (int)(cursor[2] - '0');
}

static bool status_is_success(int status_code)
{
    return status_code >= 200 && status_code < 300;
}

static err_t http_post_close_pcb(http_post_state_t *state, struct altcp_pcb *pcb, bool abort_connection)
{
    if (pcb == NULL)
    {
        return ERR_OK;
    }

    altcp_arg(pcb, NULL);
    altcp_recv(pcb, NULL);
    altcp_sent(pcb, NULL);
    altcp_poll(pcb, NULL, 0);
    altcp_err(pcb, NULL);

    if (state != NULL && state->pcb == pcb)
    {
        state->pcb = NULL;
    }

    if (abort_connection)
    {
        altcp_abort(pcb);
        return ERR_ABRT;
    }

    err_t close_err = altcp_close(pcb);
    if (close_err != ERR_OK)
    {
        altcp_abort(pcb);
        return ERR_ABRT;
    }

    return ERR_OK;
}

static err_t http_post_finish(http_post_state_t *state, struct altcp_pcb *pcb, bool success, err_t err)
{
    if (state == NULL || state->done)
    {
        return ERR_OK;
    }

    state->done = true;
    state->success = success;
    state->lwip_error = err;
    return http_post_close_pcb(state, pcb, false);
}

static err_t http_post_fail(http_post_state_t *state, struct altcp_pcb *pcb, err_t err)
{
    if (state == NULL || state->done)
    {
        return ERR_OK;
    }

    state->done = true;
    state->success = false;
    state->lwip_error = err;
    return http_post_close_pcb(state, pcb, true);
}

static err_t http_post_recv_callback(void *arg, struct altcp_pcb *pcb, struct pbuf *p, err_t err)
{
    http_post_state_t *state = (http_post_state_t *)arg;
    if (state == NULL)
    {
        if (p != NULL)
        {
            pbuf_free(p);
        }
        return ERR_OK;
    }

    if (err != ERR_OK)
    {
        if (p != NULL)
        {
            pbuf_free(p);
        }
        return http_post_fail(state, pcb, err);
    }

    if (p == NULL)
    {
        return http_post_finish(state, pcb, status_is_success(state->status_code), ERR_OK);
    }

    altcp_recved(pcb, p->tot_len);

    if (state->response_buffer != NULL && state->response_buffer_size > 0u)
    {
        size_t free_space = state->response_buffer_size - state->response_len - 1u;
        if (p->tot_len > free_space)
        {
            state->response_overflow = true;
        }

        size_t copy_len = p->tot_len < free_space ? p->tot_len : free_space;
        if (copy_len > 0u)
        {
            uint16_t copied = pbuf_copy_partial(p,
                                                state->response_buffer + state->response_len,
                                                (uint16_t)copy_len,
                                                0);
            state->response_len += copied;
            state->response_buffer[state->response_len] = '\0';

            if (state->status_code == 0)
            {
                state->status_code = parse_status_code(state->response_buffer, state->response_len);
            }
        }
    }

    pbuf_free(p);
    return ERR_OK;
}

static err_t http_post_sent_callback(void *arg, struct altcp_pcb *pcb, u16_t len)
{
    LWIP_UNUSED_ARG(len);
    return http_post_try_send((http_post_state_t *)arg, pcb);
}

static err_t http_post_poll_callback(void *arg, struct altcp_pcb *pcb)
{
    return http_post_try_send((http_post_state_t *)arg, pcb);
}

static void http_post_err_callback(void *arg, err_t err)
{
    http_post_state_t *state = (http_post_state_t *)arg;
    if (state == NULL || state->done)
    {
        return;
    }

    state->pcb = NULL;
    state->done = true;
    state->success = false;
    state->lwip_error = err;
}

static err_t http_post_try_send(http_post_state_t *state, struct altcp_pcb *pcb)
{
    if (state == NULL || pcb == NULL || state->done || !state->connected)
    {
        return ERR_OK;
    }

    while (true)
    {
        u16_t send_capacity = altcp_sndbuf(pcb);
        if (send_capacity == 0u)
        {
            break;
        }

        if (state->header_sent < state->header_len)
        {
            size_t remaining = state->header_len - state->header_sent;
            u16_t write_len = remaining < send_capacity ? (u16_t)remaining : send_capacity;
            err_t err = altcp_write(pcb,
                                    state->header + state->header_sent,
                                    write_len,
                                    TCP_WRITE_FLAG_COPY);
            if (err == ERR_MEM)
            {
                break;
            }
            if (err != ERR_OK)
            {
                return http_post_fail(state, pcb, err);
            }
            state->header_sent += write_len;
            continue;
        }

        if (state->body_sent < state->content_length)
        {
            size_t remaining = state->content_length - state->body_sent;
            size_t chunk_len = remaining < sizeof(state->send_buffer) ? remaining : sizeof(state->send_buffer);
            if (chunk_len > send_capacity)
            {
                chunk_len = send_capacity;
            }

            if (!state->body_reader(state->body_reader_context,
                                    state->body_sent,
                                    state->send_buffer,
                                    chunk_len))
            {
                return http_post_fail(state, pcb, ERR_VAL);
            }

            err_t err = altcp_write(pcb, state->send_buffer, (u16_t)chunk_len, TCP_WRITE_FLAG_COPY);
            if (err == ERR_MEM)
            {
                break;
            }
            if (err != ERR_OK)
            {
                return http_post_fail(state, pcb, err);
            }
            state->body_sent += chunk_len;
            continue;
        }

        state->sending_complete = true;
        break;
    }

    if (state->header_sent > 0u || state->body_sent > 0u)
    {
        err_t err = altcp_output(pcb);
        if (err != ERR_OK)
        {
            return http_post_fail(state, pcb, err);
        }
    }

    return ERR_OK;
}

static err_t http_post_connected_callback(void *arg, struct altcp_pcb *pcb, err_t err)
{
    http_post_state_t *state = (http_post_state_t *)arg;
    if (state == NULL)
    {
        return ERR_OK;
    }

    if (err != ERR_OK)
    {
        return http_post_fail(state, pcb, err);
    }

    state->connected = true;
    return http_post_try_send(state, pcb);
}

static err_t http_post_connect(http_post_state_t *state)
{
    state->pcb = altcp_new_ip_type(NULL, IPADDR_TYPE_V4);
    if (state->pcb == NULL)
    {
        state->done = true;
        state->success = false;
        state->lwip_error = ERR_MEM;
        return ERR_MEM;
    }

    altcp_arg(state->pcb, state);
    altcp_recv(state->pcb, http_post_recv_callback);
    altcp_sent(state->pcb, http_post_sent_callback);
    altcp_poll(state->pcb, http_post_poll_callback, HTTP_POST_POLL_INTERVAL);
    altcp_err(state->pcb, http_post_err_callback);

    err_t err = altcp_connect(state->pcb, &state->address, state->port, http_post_connected_callback);
    if (err != ERR_OK)
    {
        http_post_fail(state, state->pcb, err);
        return err;
    }

    return ERR_OK;
}

static void http_post_dns_callback(const char *name, const ip_addr_t *ipaddr, void *callback_arg)
{
    LWIP_UNUSED_ARG(name);
    http_post_state_t *state = (http_post_state_t *)callback_arg;
    if (state == NULL || state->done)
    {
        return;
    }

    if (ipaddr == NULL)
    {
        state->done = true;
        state->success = false;
        state->lwip_error = ERR_VAL;
        return;
    }

    ip_addr_copy(state->address, *ipaddr);
    http_post_connect(state);
}

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
                           http_post_result_t *result)
{
    if (result != NULL)
    {
        memset(result, 0, sizeof(*result));
    }

    if (host == NULL || host[0] == '\0' || uri == NULL || uri[0] == '\0' ||
        content_type == NULL || body_reader == NULL || response_buffer == NULL ||
        response_buffer_size == 0u)
    {
        return false;
    }

    static http_post_state_t state;
    memset(&state, 0, sizeof(state));
    state.host = host;
    state.port = port;
    state.uri = uri;
    state.content_type = content_type;
    state.content_length = content_length;
    state.body_reader = body_reader;
    state.body_reader_context = body_reader_context;
    state.response_buffer = response_buffer;
    state.response_buffer_size = response_buffer_size;
    response_buffer[0] = '\0';

    int header_len = snprintf(state.header,
                              sizeof(state.header),
                              "POST %s HTTP/1.1\r\n"
                              "Host: %s\r\n"
                              "User-Agent: AMS-Pico2W/1\r\n"
                              "Content-Type: %s\r\n"
                              "Content-Length: %lu\r\n"
                              "Connection: close\r\n"
                              "\r\n",
                              uri,
                              host,
                              content_type,
                              (unsigned long)content_length);
    if (header_len <= 0 || header_len >= (int)sizeof(state.header))
    {
        return false;
    }
    state.header_len = (size_t)header_len;

    cyw43_arch_lwip_begin();
    err_t dns_err = dns_gethostbyname(host, &state.address, http_post_dns_callback, &state);
    if (dns_err == ERR_OK)
    {
        http_post_connect(&state);
    }
    else if (dns_err != ERR_INPROGRESS)
    {
        state.done = true;
        state.success = false;
        state.lwip_error = dns_err;
    }
    cyw43_arch_lwip_end();

    absolute_time_t deadline = make_timeout_time_ms(timeout_ms);
    while (!state.done && !time_reached(deadline))
    {
        sleep_ms(10);
    }

    if (!state.done)
    {
        cyw43_arch_lwip_begin();
        if (state.pcb != NULL)
        {
            http_post_close_pcb(&state, state.pcb, true);
        }
        state.done = true;
        state.success = false;
        state.lwip_error = ERR_TIMEOUT;
        cyw43_arch_lwip_end();
    }

    if (result != NULL)
    {
        result->status_code = state.status_code;
        result->response = response_buffer;
        result->response_len = state.response_len;
        result->response_overflow = state.response_overflow;
    }

    return state.success && !state.response_overflow;
}
