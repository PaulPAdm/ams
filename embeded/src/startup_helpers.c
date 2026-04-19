#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "device_config.h"
#include "startup_helpers.h"

typedef enum
{
    START_ACTION_RUN = 1,
    START_ACTION_CHANGE_SETTINGS = 2
} start_action_t;

static bool read_line(char *out, size_t out_size)
{
    if (out == NULL || out_size == 0)
    {
        return false;
    }

    size_t idx = 0;

    while (true)
    {
        int ch = getchar();
        if (ch < 0)
        {
            continue;
        }

        if (ch == '\r' || ch == '\n')
        {
            out[idx] = '\0';
            printf("\n");
            fflush(stdout);
            return true;
        }

        if (ch == 0x08 || ch == 0x7F)
        {
            if (idx > 0)
            {
                idx--;
                printf("\b \b");
                fflush(stdout);
            }
            continue;
        }

        if (ch >= 32 && ch < 127 && idx + 1 < out_size)
        {
            out[idx++] = (char)ch;
            putchar((char)ch);
            fflush(stdout);
        }
    }
}

static bool prompt_required_field(const char *label, char *out, size_t out_size)
{
    while (true)
    {
        printf("%s: ", label);
        if (!read_line(out, out_size))
        {
            return false;
        }
        if (out[0] != '\0')
        {
            return true;
        }
        printf("Value cannot be empty.\n");
    }
}

static bool prompt_int32_field(const char *label, int32_t *out, int32_t default_value, int32_t min_value)
{
    if (out == NULL)
    {
        return false;
    }

    char answer[32];
    while (true)
    {
        printf("%s [%ld]: ", label, (long)default_value);
        if (!read_line(answer, sizeof(answer)))
        {
            return false;
        }

        if (answer[0] == '\0')
        {
            *out = default_value;
            return true;
        }

        char *end = NULL;
        long value = strtol(answer, &end, 10);
        if (end == answer || *end != '\0')
        {
            printf("Invalid number.\n");
            continue;
        }
        if (value < (long)min_value || value > (long)INT32_MAX)
        {
            printf("Value must be in range [%ld..%ld].\n", (long)min_value, (long)INT32_MAX);
            continue;
        }

        *out = (int32_t)value;
        return true;
    }
}

static start_action_t prompt_start_action(void)
{
    char answer[8];
    while (true)
    {
        printf("Choose action:\n");
        printf("1 - Start\n");
        printf("2 - Change all settings\n");
        printf("Select [1/2]: ");

        if (!read_line(answer, sizeof(answer)))
        {
            continue;
        }

        if (answer[0] == '1' && answer[1] == '\0')
        {
            return START_ACTION_RUN;
        }
        if (answer[0] == '2' && answer[1] == '\0')
        {
            return START_ACTION_CHANGE_SETTINGS;
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
    if (!prompt_required_field("Wi-Fi SSID", config->ssid, sizeof(config->ssid)))
    {
        return false;
    }
    if (!prompt_required_field("Wi-Fi password", config->password, sizeof(config->password)))
    {
        return false;
    }
    if (!prompt_required_field("Server IP", config->server_ip, sizeof(config->server_ip)))
    {
        return false;
    }
    if (!prompt_required_field("Device ID", config->device_id, sizeof(config->device_id)))
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
    printf("Server IP: %s\n", config->server_ip);
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
                            int server_port,
                            int raw_audio_sample_rate_hz,
                            int audio_downsample_factor)
{
    if (config == NULL)
    {
        return;
    }

    printf("\n===== Current settings =====\n");
    print_setting_str("Wi-Fi SSID", config->ssid);
    print_setting_str("Wi-Fi password", config->password);
    print_setting_str("Server IP", config->server_ip);
    print_setting_str("Device ID", config->device_id);
    print_setting_int("UDP port", server_port);
    printf("Audio sample rate: %d Hz\n", raw_audio_sample_rate_hz);
    print_setting_int("Audio downsample factor", audio_downsample_factor);
    printf("=============================\n\n");
}

void resolve_startup_config(device_config_t *config, bool has_config)
{
    if (config == NULL)
    {
        halt_forever_with_message("Internal startup error.");
    }

    if (!has_config)
    {
        printf("No saved settings found.\n");
        if (!collect_initial_config(config))
        {
            halt_forever_with_message("Input failed. Reboot device and try again.");
        }
        printf("Settings saved to flash.\n");
        return;
    }

    printf("Saved settings found.\n");
    print_saved_identity(config);

    start_action_t action = prompt_start_action();
    if (action == START_ACTION_CHANGE_SETTINGS)
    {
        if (!update_config_from_prompt(config))
        {
            halt_forever_with_message("Input failed. Reboot device and try again.");
        }
        printf("Settings updated.\n");
    }
}
