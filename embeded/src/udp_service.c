#include <stdio.h>
#include <string.h>
#include "pico/cyw43_arch.h"
#include "lwip/ip_addr.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"
#include "udp_service.h"

static struct udp_pcb *g_udp_pcb = NULL;
static volatile bool g_udp_ready = false;

#define UDP_AUDIO_DEVICE_ID_MAX 63

int udp_setup_endpoint(const char *ip, int server_port)
{
    static ip_addr_t server_ip;

    if (!ipaddr_aton(ip, &server_ip))
    {
        printf("Invalid IP address\n");
        return -1;
    }

    g_udp_ready = false;

    if (g_udp_pcb != NULL)
    {
        cyw43_arch_lwip_begin();
        udp_remove(g_udp_pcb);
        cyw43_arch_lwip_end();
        g_udp_pcb = NULL;
    }

    cyw43_arch_lwip_begin();
    g_udp_pcb = udp_new_ip_type(IPADDR_TYPE_V4);
    if (g_udp_pcb == NULL)
    {
        cyw43_arch_lwip_end();
        printf("Failed to create UDP PCB\n");
        return -1;
    }

    err_t err = udp_connect(g_udp_pcb, &server_ip, (u16_t)server_port);
    cyw43_arch_lwip_end();
    if (err != ERR_OK)
    {
        cyw43_arch_lwip_begin();
        udp_remove(g_udp_pcb);
        cyw43_arch_lwip_end();
        g_udp_pcb = NULL;
        printf("Failed to setup UDP endpoint: %d\n", err);
        return -1;
    }

    g_udp_ready = true;
    return 0;
}

bool udp_send_audio_mono16(const char *device_id, const int16_t *samples, size_t sample_count)
{
    if (!g_udp_ready || g_udp_pcb == NULL || device_id == NULL || samples == NULL || sample_count == 0)
    {
        return false;
    }

    size_t device_id_len = strnlen(device_id, UDP_AUDIO_DEVICE_ID_MAX + 1);
    if (device_id_len == 0 || device_id_len > UDP_AUDIO_DEVICE_ID_MAX)
    {
        return false;
    }

    const size_t payload_len = sample_count * sizeof(int16_t);
    const size_t packet_len = device_id_len + 1u + payload_len;

    cyw43_arch_lwip_begin();
    struct pbuf *packet = pbuf_alloc(PBUF_TRANSPORT, (u16_t)packet_len, PBUF_RAM);
    if (packet == NULL)
    {
        cyw43_arch_lwip_end();
        return false;
    }

    memcpy(packet->payload, device_id, device_id_len);
    ((char *)packet->payload)[device_id_len] = '\0';
    memcpy((uint8_t *)packet->payload + device_id_len + 1u, samples, payload_len);
    err_t err = udp_send(g_udp_pcb, packet);
    pbuf_free(packet);
    cyw43_arch_lwip_end();

    if (err != ERR_OK)
    {
        static uint32_t udp_err_count = 0;
        udp_err_count++;
        if ((udp_err_count % 50u) == 1u)
        {
            printf("udp_send failed: %d\n", err);
        }
        return false;
    }

    return true;
}

bool is_udp_ready(void)
{
    return g_udp_ready && (g_udp_pcb != NULL);
}

void udp_close(void)
{
    if (g_udp_pcb != NULL)
    {
        cyw43_arch_lwip_begin();
        udp_remove(g_udp_pcb);
        cyw43_arch_lwip_end();
        g_udp_pcb = NULL;
    }
    g_udp_ready = false;
}
