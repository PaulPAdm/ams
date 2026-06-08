#include "wifi_service.h"

#include <string.h>

#include "device_runtime_config.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
#define WIFI_MAX_RETRY 8

static const char *TAG = "wifi-service";
static EventGroupHandle_t g_wifi_event_group;
static int g_retry_count = 0;
static bool g_network_initialized = false;
static bool g_wifi_started = false;

static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    (void)arg;
    (void)event_data;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        xEventGroupClearBits(g_wifi_event_group, WIFI_CONNECTED_BIT);
        if (g_wifi_started && g_retry_count < WIFI_MAX_RETRY)
        {
            g_retry_count++;
            esp_wifi_connect();
            ESP_LOGW(TAG, "Retry Wi-Fi connection (%d/%d)", g_retry_count, WIFI_MAX_RETRY);
        }
        else
        {
            xEventGroupSetBits(g_wifi_event_group, WIFI_FAIL_BIT);
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        g_retry_count = 0;
        xEventGroupSetBits(g_wifi_event_group, WIFI_CONNECTED_BIT);
        xEventGroupClearBits(g_wifi_event_group, WIFI_FAIL_BIT);
        ESP_LOGI(TAG, "Wi-Fi connected");
    }
}

int initialize_network(void)
{
    if (g_network_initialized)
    {
        return 0;
    }

    g_wifi_event_group = xEventGroupCreate();
    if (g_wifi_event_group == NULL)
    {
        return -1;
    }

    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
    {
        return -1;
    }

    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
    {
        return -1;
    }

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&init_cfg);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
    {
        return -1;
    }

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL);
    esp_wifi_set_mode(WIFI_MODE_STA);

    g_network_initialized = true;
    return 0;
}

int connect_to_wifi(const char *ssid, const char *password)
{
    if (ssid == NULL || ssid[0] == '\0')
    {
        ESP_LOGW(TAG, "Wi-Fi SSID is empty");
        return -1;
    }

    if (!g_network_initialized && initialize_network() != 0)
    {
        return -1;
    }

    if (is_wifi_connected())
    {
        return 0;
    }

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1u);
    if (password != NULL)
    {
        strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1u);
    }
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    xEventGroupClearBits(g_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
    g_retry_count = 0;

    if (esp_wifi_set_config(WIFI_IF_STA, &wifi_config) != ESP_OK)
    {
        return -1;
    }

    if (!g_wifi_started)
    {
        if (esp_wifi_start() != ESP_OK)
        {
            return -1;
        }
        g_wifi_started = true;
    }

    esp_wifi_connect();
    EventBits_t bits = xEventGroupWaitBits(g_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           pdMS_TO_TICKS(DEVICE_WIFI_CONNECT_TIMEOUT_MS));

    return (bits & WIFI_CONNECTED_BIT) != 0 ? 0 : -1;
}

void disconnect_wifi(void)
{
    if (!g_network_initialized || !g_wifi_started)
    {
        return;
    }

    g_wifi_started = false;
    esp_wifi_disconnect();
    esp_wifi_stop();
    xEventGroupClearBits(g_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
}

bool is_wifi_connected(void)
{
    if (g_wifi_event_group == NULL)
    {
        return false;
    }

    EventBits_t bits = xEventGroupGetBits(g_wifi_event_group);
    return (bits & WIFI_CONNECTED_BIT) != 0;
}
