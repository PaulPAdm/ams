#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#include "console_helpers.h"
#include "device_config.h"
#include "device_runtime_config.h"
#include "startup_helpers.h"

#define USB_CONSOLE_DETECT_WINDOW_MS 7000u

typedef enum
{
    START_MENU_START_NORMAL = 1,
    START_MENU_EDIT_SETTINGS = 2,
    START_MENU_START_DIAGNOSTICS = 3,
    START_MENU_START_AUDIO_CALIBRATION = 4
} start_menu_action_t;

static start_menu_action_t prompt_start_action(void)
{
    char answer[8];
    while (true)
    {
        printf("Startup menu:\n");
        printf("1 - Start Normal Mode\n");
        printf("2 - Edit Settings\n");
        printf("3 - Start Diagnostics Mode\n");
        printf("4 - Start Audio Calibration\n");
        printf("Select [1/2/3/4]: ");

        if (!console_read_line(answer, sizeof(answer)))
        {
            continue;
        }

        if (answer[0] == '1' && answer[1] == '\0')
        {
            return START_MENU_START_NORMAL;
        }
        if (answer[0] == '2' && answer[1] == '\0')
        {
            return START_MENU_EDIT_SETTINGS;
        }
        if (answer[0] == '3' && answer[1] == '\0')
        {
            return START_MENU_START_DIAGNOSTICS;
        }
        if (answer[0] == '4' && answer[1] == '\0')
        {
            return START_MENU_START_AUDIO_CALIBRATION;
        }

        printf("Invalid choice.\n");
    }
}

static bool prompt_full_config(device_config_t *config, const device_config_t *base_config)
{
    if (config == NULL)
    {
        return false;
    }

    if (base_config != NULL)
    {
        *config = *base_config;
    }
    else
    {
        memset(config, 0, sizeof(*config));
    }

    printf("Enter device settings:\n");
    if (!console_prompt_required_field("Wi-Fi SSID", config->ssid, sizeof(config->ssid)))
    {
        return false;
    }
    if (!console_prompt_required_field("Wi-Fi password", config->password, sizeof(config->password)))
    {
        return false;
    }
    if (!console_prompt_required_field("Server host or IP", config->server_ip, sizeof(config->server_ip)))
    {
        return false;
    }
    if (!console_prompt_required_field("Device ID", config->device_id, sizeof(config->device_id)))
    {
        return false;
    }

    return true;
}

static void halt_forever_with_message(const char *message)
{
    if (message != NULL && message[0] != '\0')
    {
        printf("%s\n", message);
    }
    while (true)
    {
        sleep_ms(1000);
    }
}

static void print_saved_identity(const device_config_t *config)
{
    if (config == NULL)
    {
        return;
    }
    printf("SSID: %s\n", config->ssid);
    printf("Server host/IP: %s\n", config->server_ip);
    printf("Device ID: %s\n", config->device_id);
}

static void print_setting_str(const char *label, const char *value)
{
    printf("%s: %s\n", label, value);
}

static void print_setting_int(const char *label, long value)
{
    printf("%s: %ld\n", label, value);
}

bool save_config_or_log(const device_config_t *config, const char *error_message)
{
    if (device_config_save(config))
    {
        return true;
    }
    if (error_message != NULL && error_message[0] != '\0')
    {
        printf("%s\n", error_message);
    }
    return false;
}

static bool collect_initial_config(device_config_t *config)
{
    if (!prompt_full_config(config, NULL))
    {
        return false;
    }
    return save_config_or_log(config, "Failed to save settings to flash.");
}

static bool update_config_from_prompt(device_config_t *config)
{
    device_config_t new_config;
    if (!prompt_full_config(&new_config, config))
    {
        return false;
    }
    if (!save_config_or_log(&new_config, "Failed to save settings to flash."))
    {
        return false;
    }
    *config = new_config;
    return true;
}

void print_current_settings(const device_config_t *config,
                            int raw_audio_sample_rate_hz,
                            int audio_downsample_factor)
{
    (void)raw_audio_sample_rate_hz;
    (void)audio_downsample_factor;

    if (config == NULL)
    {
        return;
    }

    printf("\n===== Current settings =====\n");
    print_setting_str("Wi-Fi SSID", config->ssid);
    print_setting_str("Wi-Fi password", config->password);
    print_setting_str("Server host/IP", config->server_ip);
    print_setting_str("Device ID", config->device_id);
    if (audio_calibration_is_valid(&config->audio_calibration))
    {
        print_setting_int("Audio calibrated bands", config->audio_calibration.band_count);
    }
    else
    {
        print_setting_str("Audio calibration", "not calibrated");
    }
    printf("=============================\n\n");
}

bool startup_wait_for_usb_console(power_meter_service_t *power_meter_service)
{
    absolute_time_t deadline = make_timeout_time_ms(USB_CONSOLE_DETECT_WINDOW_MS);
    while (!time_reached(deadline))
    {
        if (stdio_usb_connected())
        {
            return true;
        }

        power_meter_service_poll(power_meter_service);
        sleep_ms(DEVICE_RUNTIME_POLL_SLEEP_MS);
    }

    return stdio_usb_connected();
}

const char *startup_run_mode_label(startup_run_mode_t mode)
{
    switch (mode)
    {
    case STARTUP_RUN_MODE_AUDIO_CALIBRATION:
        return "Audio Calibration Mode";
    case STARTUP_RUN_MODE_DIAGNOSTICS:
        return "Diagnostics Mode";
    case STARTUP_RUN_MODE_NORMAL:
    default:
        return "Normal Mode";
    }
}

startup_run_mode_t resolve_startup_config(device_config_t *config, bool has_config, bool usb_console_connected)
{
    if (config == NULL)
    {
        halt_forever_with_message("Internal startup error.");
    }

    if (!has_config)
    {
        printf("No saved settings found.\n");
        if (!usb_console_connected)
        {
            halt_forever_with_message("No saved settings and no USB console connected. Connect over USB to configure the device.");
        }
        printf("Entering Setup Mode.\n");
        if (!collect_initial_config(config))
        {
            halt_forever_with_message("Input failed. Reboot device and try again.");
        }
        printf("Settings saved to flash.\n");
        printf("Starting Diagnostics Mode for first-time bring-up.\n");
        return STARTUP_RUN_MODE_DIAGNOSTICS;
    }

    printf("Saved settings found.\n");
    print_saved_identity(config);

    if (!usb_console_connected)
    {
        printf("USB console not connected. Starting Normal Mode.\n");
        return STARTUP_RUN_MODE_NORMAL;
    }

    while (true)
    {
        start_menu_action_t action = prompt_start_action();
        if (action == START_MENU_EDIT_SETTINGS)
        {
            printf("Entering Setup Mode.\n");
            if (!update_config_from_prompt(config))
            {
                halt_forever_with_message("Input failed. Reboot device and try again.");
            }
            printf("Settings updated.\n");
            continue;
        }

        if (action == START_MENU_START_DIAGNOSTICS)
        {
            return STARTUP_RUN_MODE_DIAGNOSTICS;
        }
        if (action == START_MENU_START_AUDIO_CALIBRATION)
        {
            return STARTUP_RUN_MODE_AUDIO_CALIBRATION;
        }

        return STARTUP_RUN_MODE_NORMAL;
    }
}
