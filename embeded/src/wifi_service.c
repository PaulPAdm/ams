#include <stdio.h>
#include <stdbool.h>
#include "pico/cyw43_arch.h"
#include "wifi_service.h"

int initialize_network(void)
{
    // Initialise the Wi-Fi chip
    if (cyw43_arch_init())
    {
        printf("Wi-Fi init failed\n");
        return -1;
    }

    cyw43_arch_enable_sta_mode();
    cyw43_wifi_pm(&cyw43_state, CYW43_PERFORMANCE_PM);
    return 0;
}

int connect_to_wifi(const char *ssid, const char *password)
{
    cyw43_arch_enable_sta_mode();
    int err = cyw43_arch_wifi_connect_timeout_ms(ssid, password, CYW43_AUTH_WPA2_AES_PSK, 10000);
    if (err)
    {
        printf("Wi-Fi connection failed, error: %d\n", err);
        return -1;
    }
    return 0;
}

void disconnect_wifi(void)
{
    cyw43_arch_disable_sta_mode();
}

bool is_wifi_connected(void)
{
    return cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA) == CYW43_LINK_UP;
}
