#include "audio_calibration_runtime.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "audio_stream_queue.h"
#include "audio_spectrum.h"
#include "console_helpers.h"
#include "inky_status_display.h"
#include "microphone.h"
#include "pico/stdlib.h"
#include "startup_helpers.h"

#define AUDIO_CALIBRATION_DEFAULT_DURATION_SEC 15u
#define AUDIO_CALIBRATION_MIN_DURATION_SEC 3u
#define AUDIO_CALIBRATION_MAX_DURATION_SEC 120u

static void calibration_microphone_callback(const int32_t *buffer, size_t words, uint64_t buffer_ready_us)
{
    audio_stream_queue_push_from_buffer(buffer, words, buffer_ready_us);
}

static uint32_t clamp_u64_to_u32(uint64_t value)
{
    return value > UINT32_MAX ? UINT32_MAX : (uint32_t)value;
}

static bool capture_calibration_spectrum(audio_calibration_t *calibration, uint32_t duration_sec)
{
    if (calibration == NULL || duration_sec == 0u)
    {
        return false;
    }

    audio_spectrum_analyzer_t analyzer;
    if (!audio_spectrum_analyzer_init(&analyzer))
    {
        printf("[Calibration] Failed to initialize CMSIS-DSP FFT analyzer.\n");
        return false;
    }

    uint64_t bin_sums[AUDIO_SPECTRUM_BIN_COUNT] = {0};
    uint32_t average_bins[AUDIO_SPECTRUM_BIN_COUNT] = {0};
    uint32_t current_bins[AUDIO_SPECTRUM_BIN_COUNT] = {0};
    uint32_t frames_seen = 0u;
    uint32_t seconds_reported = 0u;

    printf("[Calibration] Starting microphone capture for %lu seconds.\n", (unsigned long)duration_sec);
    printf("[Calibration] Keep the room in the target background state.\n");

    audio_stream_queue_init();
    microphone_set_buffer_callback(calibration_microphone_callback);
    microphone_init();

    absolute_time_t started_at = get_absolute_time();
    absolute_time_t deadline = make_timeout_time_ms(duration_sec * 1000u);
    while (!time_reached(deadline))
    {
        bool processed = false;
        for (int burst = 0; burst < 8; ++burst)
        {
            audio_chunk_view_t chunk;
            if (!audio_stream_queue_peek(&chunk) || chunk.sample_count == 0u)
            {
                break;
            }

            if (audio_spectrum_analyze_q15(&analyzer, chunk.samples, chunk.sample_count, current_bins))
            {
                for (size_t bin = 0; bin < AUDIO_SPECTRUM_BIN_COUNT; ++bin)
                {
                    bin_sums[bin] += current_bins[bin];
                }
                frames_seen++;
            }

            audio_stream_queue_pop_if_matches(chunk.slot);
            processed = true;
        }

        uint32_t elapsed_sec = (uint32_t)(absolute_time_diff_us(started_at, get_absolute_time()) / 1000000ll);
        if (elapsed_sec > seconds_reported)
        {
            seconds_reported = elapsed_sec;
            printf("[Calibration] Captured %lu/%lu sec, frames=%lu\n",
                   (unsigned long)seconds_reported,
                   (unsigned long)duration_sec,
                   (unsigned long)frames_seen);
        }

        if (!processed)
        {
            sleep_ms(2);
        }
    }

    if (frames_seen == 0u)
    {
        printf("[Calibration] No audio frames captured.\n");
        return false;
    }

    for (size_t bin = 0; bin < AUDIO_SPECTRUM_BIN_COUNT; ++bin)
    {
        average_bins[bin] = clamp_u64_to_u32(bin_sums[bin] / frames_seen);
    }

    if (!audio_calibration_build_from_spectrum(calibration, average_bins, AUDIO_SPECTRUM_BIN_COUNT))
    {
        printf("[Calibration] Failed to select stable frequency bands.\n");
        return false;
    }

    printf("[Calibration] Selected %u dominant bands from %lu frames.\n",
           (unsigned int)calibration->band_count,
           (unsigned long)frames_seen);
    return true;
}

static void edit_calibration_multiplier(audio_calibration_t *calibration)
{
    if (!audio_calibration_is_valid(calibration))
    {
        printf("Calibration is not valid.\n");
        return;
    }

    uint32_t band_number = 1u;
    if (!console_prompt_u32("Band number",
                            1u,
                            calibration->band_count,
                            1u,
                            &band_number))
    {
        return;
    }

    size_t band_index = (size_t)(band_number - 1u);
    uint16_t multiplier_x100 = calibration->bands[band_index].threshold_multiplier_x100;
    if (!console_prompt_multiplier_x100("New trigger multiplier",
                                        multiplier_x100,
                                        AUDIO_CALIBRATION_MIN_MULTIPLIER_X100,
                                        AUDIO_CALIBRATION_MAX_MULTIPLIER_X100,
                                        &multiplier_x100))
    {
        return;
    }

    if (!audio_calibration_set_multiplier(calibration, band_index, multiplier_x100))
    {
        printf("Invalid multiplier.\n");
        return;
    }

    printf("Band %lu multiplier updated.\n", (unsigned long)band_number);
}

static bool review_calibration_menu(device_config_t *config)
{
    char answer[8];
    while (true)
    {
        audio_calibration_print(&config->audio_calibration);
        inky_status_display_show_audio_calibration(&config->audio_calibration);
        printf("Calibration menu:\n");
        printf("1 - Save calibration\n");
        printf("2 - Edit band multiplier\n");
        printf("3 - Print calibration\n");
        printf("4 - Discard calibration\n");
        printf("Select [1/2/3/4]: ");

        if (!console_read_line(answer, sizeof(answer)))
        {
            continue;
        }

        if (answer[0] == '1' && answer[1] == '\0')
        {
            return save_config_or_log(config, "Failed to save calibration to flash.");
        }
        if (answer[0] == '2' && answer[1] == '\0')
        {
            edit_calibration_multiplier(&config->audio_calibration);
            continue;
        }
        if (answer[0] == '3' && answer[1] == '\0')
        {
            audio_calibration_print(&config->audio_calibration);
            continue;
        }
        if (answer[0] == '4' && answer[1] == '\0')
        {
            printf("Calibration discarded.\n");
            return false;
        }

        printf("Invalid choice.\n");
    }
}

bool audio_calibration_run_interactive(device_config_t *config)
{
    if (config == NULL)
    {
        return false;
    }

    uint32_t duration_sec = AUDIO_CALIBRATION_DEFAULT_DURATION_SEC;
    if (!console_prompt_u32("Calibration duration seconds",
                            AUDIO_CALIBRATION_MIN_DURATION_SEC,
                            AUDIO_CALIBRATION_MAX_DURATION_SEC,
                            AUDIO_CALIBRATION_DEFAULT_DURATION_SEC,
                            &duration_sec))
    {
        return false;
    }

    audio_calibration_t captured_calibration;
    if (!capture_calibration_spectrum(&captured_calibration, duration_sec))
    {
        audio_calibration_set_defaults(&config->audio_calibration);
        inky_status_display_show_audio_calibration(&config->audio_calibration);
        return false;
    }

    config->audio_calibration = captured_calibration;
    bool saved = review_calibration_menu(config);
    printf(saved ? "Calibration saved.\n" : "Calibration was not saved.\n");
    printf("Reboot the device to start normal monitoring with a clean microphone runtime.\n");
    return saved;
}
