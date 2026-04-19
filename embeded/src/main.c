#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "microphone.h"
#include "startup_helpers.h"
#include "audio_stream_queue.h"
#include "wifi_service.h"
#include "udp_service.h"
#include "device_config.h"

#define SERVER_PORT 5000
#define RAW_AUDIO_SAMPLE_RATE_HZ 48000

static void on_microphone_buffer_ready(const int32_t *buffer, size_t words, uint64_t buffer_ready_us)
{
    audio_stream_queue_push_from_buffer(buffer, words);
}

int main()
{
    stdio_init_all();

    sleep_ms(8000); // Wait for USB serial to initialize

    device_config_t config;
    bool has_config = device_config_load(&config);
    resolve_startup_config(&config, has_config);

    audio_stream_queue_init();
    microphone_set_buffer_callback(on_microphone_buffer_ready);
    microphone_init();

    print_current_settings(&config,
                           SERVER_PORT,
                           RAW_AUDIO_SAMPLE_RATE_HZ,
                           AUDIO_STREAM_QUEUE_DOWNSAMPLE_FACTOR);

    initialize_network();

    while (connect_to_wifi(config.ssid, config.password) != 0)
    {
        printf("Wi-Fi connection...\n");
        sleep_ms(5000);
    }
    printf("Wi-Fi connected\n");

    while (udp_setup_endpoint(config.server_ip, SERVER_PORT) != 0)
    {
        printf("Retrying UDP endpoint setup...\n");
        sleep_ms(5000);
    }
    printf("UDP endpoint ready (mono16 ~16kHz)\n");

    while (true)
    {
        cyw43_arch_poll();

        if (!is_udp_ready())
        {
            if (udp_setup_endpoint(config.server_ip, SERVER_PORT) == 0)
            {
                printf("UDP endpoint restored\n");
            }
            else
            {
                sleep_ms(1000);
                continue;
            }
        }

        bool sent_any = false;
        for (int burst = 0; burst < 8; ++burst)
        {
            audio_chunk_view_t chunk;
            if (!audio_stream_queue_peek(&chunk) || chunk.sample_count == 0)
            {
                break;
            }

            if (!udp_send_audio_mono16(config.device_id, chunk.samples, chunk.sample_count))
            {
                break;
            }

            audio_stream_queue_pop_if_matches(chunk.slot);
            sent_any = true;
        }

        if (!sent_any)
        {
            sleep_ms(1);
        }
    }
}
