#include <stdio.h>
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
    return 0;
}

int connect_to_wifi(const char *ssid, const char *password)
{
    int err = cyw43_arch_wifi_connect_timeout_ms(ssid, password, CYW43_AUTH_WPA2_AES_PSK, 10000);
    if (err)
    {
        printf("Wi-Fi connection failed, error: %d\n", err);
        return -1;
    }
    return 0;
}
