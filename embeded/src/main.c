#include <stdio.h>

#include "hardware/watchdog.h"
#include "pico/stdlib.h"

#include "acoustic_runtime.h"
#include "audio_calibration_runtime.h"
#include "audio_stream_queue.h"
#include "device_clock.h"
#include "device_config.h"
#include "device_runtime_config.h"
#include "device_status.h"
#include "diagnostics_service.h"
#include "health_runtime.h"
#include "inky_status_display.h"
#include "network_runtime.h"
#include "power_meter_service.h"
#include "runtime_status.h"
#include "sd_card_buffer.h"
#include "server_health.h"
#include "startup_helpers.h"
#include "time_sync_runtime.h"
#include "time_sync_service.h"
#include "wifi_service.h"

int main()
{
    stdio_init_all();
    sleep_ms(250);

    // 8-second timeout. Keeps device alive through Wi-Fi hangs and long uploads.
    watchdog_enable(8000, true);

    power_meter_service_t power_meter_service;
    power_meter_service_init(&power_meter_service);

    bool usb_console_connected = startup_wait_for_usb_console(&power_meter_service);
    printf("USB console %s\n", usb_console_connected ? "detected" : "not detected");

    device_config_t config;
    bool has_config = device_config_load(&config);
    inky_status_display_reset();
    if (!has_config)
    {
        inky_status_display_show_missing_settings(usb_console_connected);
    }

    startup_run_mode_t run_mode = resolve_startup_config(&config, has_config, usb_console_connected);
    printf("Selected startup mode: %s\n", startup_run_mode_label(run_mode));

    diagnostics_service_t diagnostics_service;
    diagnostics_service_init(&diagnostics_service, run_mode == STARTUP_RUN_MODE_DIAGNOSTICS);

    device_status_snapshot_t runtime_status;
    device_status_snapshot_init(&runtime_status);
    inky_status_display_reset();

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

    sd_card_buffer_t sd_card_buffer;
    bool sd_card_ready = sd_card_buffer_run_startup_test(&sd_card_buffer);
    runtime_status_set_sd_card_state(sd_card_ready ? DEVICE_COMPONENT_OK : DEVICE_COMPONENT_ERROR);

    device_wifi_state_t wifi_state = DEVICE_WIFI_SLEEP;
    bool microphone_ready = false;
    runtime_status_publish(&config,
                           &diagnostics_service,
                           &power_meter_service,
                           NULL,
                           &runtime_status,
                           wifi_state,
                           microphone_ready);

    if (initialize_network() != 0)
    {
        wifi_state = DEVICE_WIFI_ERROR;
        runtime_status_publish(&config,
                               &diagnostics_service,
                               &power_meter_service,
                               NULL,
                               &runtime_status,
                               wifi_state,
                               microphone_ready);
        printf("Network initialization failed, stopping startup.\n");
        return 1;
    }

    network_runtime_connect_wifi_with_retries(&config,
                                              &diagnostics_service,
                                              &power_meter_service,
                                              NULL,
                                              &runtime_status,
                                              &wifi_state,
                                              microphone_ready);
    printf("Wi-Fi connected\n");

    device_clock_reset();

    time_sync_service_t time_sync_service;
    time_sync_service_init(&time_sync_service, &config);

    server_health_service_t server_health_service;
    server_health_service_init(&server_health_service, &config);

    time_sync_runtime_wait_initial(&config,
                                   &diagnostics_service,
                                   &power_meter_service,
                                   &time_sync_service,
                                   &runtime_status,
                                   &wifi_state,
                                   microphone_ready);
    network_runtime_sleep_wifi(&config,
                               &diagnostics_service,
                               &power_meter_service,
                               &runtime_status,
                               &wifi_state,
                               microphone_ready);

    acoustic_runtime_t acoustic_runtime;
    acoustic_runtime_init(&acoustic_runtime, &config.audio_calibration);
    microphone_ready = true;
    server_health_service_set_microphone_active(&server_health_service, microphone_ready);

    runtime_status_publish(&config,
                           &diagnostics_service,
                           &power_meter_service,
                           &server_health_service,
                           &runtime_status,
                           wifi_state,
                           microphone_ready);

    while (true)
    {
        watchdog_update();

        runtime_iteration_run(&config,
                              &diagnostics_service,
                              &power_meter_service,
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
                                                   &power_meter_service,
                                                   sd_card_ready ? &sd_card_buffer : NULL,
                                                   &runtime_status,
                                                   &wifi_state,
                                                   microphone_ready);

        if (time_sync_service_is_due(&time_sync_service))
        {
            time_sync_runtime_run_scheduled(&config,
                                            &diagnostics_service,
                                            &power_meter_service,
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
                                                 &power_meter_service,
                                                 &server_health_service,
                                                 &runtime_status,
                                                 &wifi_state,
                                                 microphone_ready);
        }

        if (!processed_any)
        {
            sleep_ms(2);
        }
    }
}
