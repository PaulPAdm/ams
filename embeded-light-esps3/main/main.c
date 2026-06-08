#include <stdio.h>

#include "esp_err.h"
#include "nvs_flash.h"

#include "acoustic_runtime.h"
#include "audio_calibration_runtime.h"
#include "audio_stream_queue.h"
#include "console_helpers.h"
#include "device_clock.h"
#include "device_config.h"
#include "device_runtime_config.h"
#include "device_status.h"
#include "diagnostics_service.h"
#include "health_runtime.h"
#include "network_runtime.h"
#include "platform_time.h"
#include "runtime_status.h"
#include "sd_card_buffer.h"
#include "server_health.h"
#include "startup_helpers.h"
#include "time_sync_runtime.h"
#include "time_sync_service.h"
#include "wifi_service.h"

void app_main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stdin, NULL, _IONBF, 0);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    sleep_ms(250);
    printf("Starting %s for ESP32-S3\n", DEVICE_FIRMWARE_VERSION);
    ESP_ERROR_CHECK(console_helpers_init());

    static device_config_t config;
    bool has_config = device_config_load(&config);
    bool usb_console_connected = !has_config || startup_wait_for_usb_console();
    printf("Startup console %s\n", usb_console_connected ? "enabled" : "not requested");

    startup_run_mode_t run_mode = resolve_startup_config(&config, has_config, usb_console_connected);
    printf("Selected startup mode: %s\n", startup_run_mode_label(run_mode));

    static diagnostics_service_t diagnostics_service;
    diagnostics_service_init(&diagnostics_service, run_mode == STARTUP_RUN_MODE_DIAGNOSTICS);

    static device_status_snapshot_t runtime_status;
    device_status_snapshot_init(&runtime_status);

    print_current_settings(&config,
                           AUDIO_STREAM_QUEUE_INPUT_SAMPLE_RATE_HZ,
                           AUDIO_STREAM_QUEUE_DOWNSAMPLE_FACTOR);

    if (run_mode == STARTUP_RUN_MODE_AUDIO_CALIBRATION)
    {
        audio_calibration_run_interactive(&config);
        while (true)
        {
            sleep_ms(1000);
        }
    }

    static sd_card_buffer_t sd_card_buffer;
    bool sd_card_ready = sd_card_buffer_run_startup_test(&sd_card_buffer);
    runtime_status_set_sd_card_state(sd_card_ready ? DEVICE_COMPONENT_OK : DEVICE_COMPONENT_ERROR);

    device_wifi_state_t wifi_state = DEVICE_WIFI_SLEEP;
    bool microphone_ready = false;
    runtime_status_publish(&config,
                           &diagnostics_service,
                           NULL,
                           &runtime_status,
                           wifi_state,
                           microphone_ready);

    if (initialize_network() != 0)
    {
        wifi_state = DEVICE_WIFI_ERROR;
        runtime_status_publish(&config,
                               &diagnostics_service,
                               NULL,
                               &runtime_status,
                               wifi_state,
                               microphone_ready);
        printf("Network initialization failed, stopping startup.\n");
        return;
    }

    network_runtime_connect_wifi_with_retries(&config,
                                              &diagnostics_service,
                                              NULL,
                                              &runtime_status,
                                              &wifi_state,
                                              microphone_ready);
    printf("Wi-Fi connected\n");

    device_clock_reset();

    static time_sync_service_t time_sync_service;
    time_sync_service_init(&time_sync_service, &config);

    static server_health_service_t server_health_service;
    server_health_service_init(&server_health_service, &config);

    time_sync_runtime_wait_initial(&config,
                                   &diagnostics_service,
                                   &time_sync_service,
                                   &runtime_status,
                                   &wifi_state,
                                   microphone_ready);
    network_runtime_sleep_wifi(&config,
                               &diagnostics_service,
                               &runtime_status,
                               &wifi_state,
                               microphone_ready);

    static acoustic_runtime_t acoustic_runtime;
    acoustic_runtime_init(&acoustic_runtime, &config.audio_calibration);
    microphone_ready = true;
    server_health_service_set_microphone_active(&server_health_service, microphone_ready);

    runtime_status_publish(&config,
                           &diagnostics_service,
                           &server_health_service,
                           &runtime_status,
                           wifi_state,
                           microphone_ready);

    while (true)
    {
        runtime_iteration_run(&config,
                              &diagnostics_service,
                              &time_sync_service,
                              &server_health_service,
                              &runtime_status,
                              wifi_state,
                              microphone_ready,
                              wifi_state == DEVICE_WIFI_CONNECTED,
                              true);

        bool processed_any = acoustic_runtime_poll(&acoustic_runtime,
                                                   &config,
                                                   &diagnostics_service,
                                                   sd_card_ready ? &sd_card_buffer : NULL,
                                                   &runtime_status,
                                                   &wifi_state,
                                                   microphone_ready);

        if (time_sync_service_is_due(&time_sync_service))
        {
            time_sync_runtime_run_scheduled(&config,
                                            &diagnostics_service,
                                            &time_sync_service,
                                            &server_health_service,
                                            &runtime_status,
                                            &wifi_state,
                                            microphone_ready);
        }
        else if (server_health_service_is_report_due(&server_health_service))
        {
            health_runtime_send_scheduled_report(&config,
                                                 &diagnostics_service,
                                                 &server_health_service,
                                                 &runtime_status,
                                                 &wifi_state,
                                                 microphone_ready);
        }

        if (!processed_any)
        {
            sleep_ms(2);
        }
        else
        {
            sleep_ms(1);
        }
    }
}
