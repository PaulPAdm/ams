#include "microphone.h"

#include <stdbool.h>
#include <string.h>

#include "audio_stream_queue.h"
#include "device_runtime_config.h"
#include "driver/gpio.h"
#include "driver/i2s_common.h"
#include "driver/i2s_std.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define MICROPHONE_TASK_STACK_WORDS 4096u
#define MICROPHONE_TASK_PRIORITY 6u

static const char *TAG = "microphone";
static i2s_chan_handle_t g_rx_channel = NULL;
static volatile microphone_buffer_callback_t g_buffer_callback = NULL;
static bool g_microphone_started = false;
static TaskHandle_t g_microphone_task = NULL;

void microphone_set_buffer_callback(microphone_buffer_callback_t callback)
{
    g_buffer_callback = callback;
}

static esp_err_t microphone_i2s_start(void)
{
    if (g_rx_channel != NULL)
    {
        return ESP_OK;
    }

    gpio_config_t sel_cfg = {
        .pin_bit_mask = 1ULL << DEVICE_MIC_PIN_SEL,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&sel_cfg), TAG, "SEL gpio config failed");
    ESP_RETURN_ON_ERROR(gpio_set_level(DEVICE_MIC_PIN_SEL, DEVICE_MIC_SEL_LEVEL), TAG, "SEL gpio set failed");

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = 4;
    chan_cfg.dma_frame_num = AUDIO_STREAM_QUEUE_INPUT_BLOCK_FRAMES;
    ESP_RETURN_ON_ERROR(i2s_new_channel(&chan_cfg, NULL, &g_rx_channel), TAG, "i2s_new_channel failed");

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_STREAM_QUEUE_INPUT_SAMPLE_RATE_HZ),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = DEVICE_MIC_PIN_BCLK,
            .ws = DEVICE_MIC_PIN_LRCLK,
            .dout = I2S_GPIO_UNUSED,
            .din = DEVICE_MIC_PIN_DOUT,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(g_rx_channel, &std_cfg), TAG, "std mode init failed");
    ESP_RETURN_ON_ERROR(i2s_channel_enable(g_rx_channel), TAG, "i2s enable failed");

    ESP_LOGI(TAG,
             "ICS-43434 ready: BCLK=GPIO%d LRCLK=GPIO%d DOUT=GPIO%d SEL=GPIO%d level=%d",
             DEVICE_MIC_PIN_BCLK,
             DEVICE_MIC_PIN_LRCLK,
             DEVICE_MIC_PIN_DOUT,
             DEVICE_MIC_PIN_SEL,
             DEVICE_MIC_SEL_LEVEL);
    return ESP_OK;
}

static void microphone_task(void *arg)
{
    (void)arg;
    int32_t raw_frames[AUDIO_STREAM_QUEUE_INPUT_BLOCK_FRAMES * 2u];

    while (true)
    {
        size_t bytes_read = 0;
        esp_err_t err = i2s_channel_read(g_rx_channel,
                                         raw_frames,
                                         sizeof(raw_frames),
                                         &bytes_read,
                                         pdMS_TO_TICKS(1000));
        if (err != ESP_OK)
        {
            ESP_LOGW(TAG, "I2S read failed: %s", esp_err_to_name(err));
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        size_t words = bytes_read / sizeof(int32_t);
        if (words == 0u)
        {
            continue;
        }

        microphone_buffer_callback_t callback = g_buffer_callback;
        if (callback != NULL)
        {
            callback(raw_frames, words, (uint64_t)esp_timer_get_time());
        }
    }
}

void microphone_init(void)
{
    if (g_microphone_started)
    {
        return;
    }

    esp_err_t err = microphone_i2s_start();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Microphone init failed: %s", esp_err_to_name(err));
        return;
    }

    BaseType_t created = xTaskCreate(microphone_task,
                                     "microphone",
                                     MICROPHONE_TASK_STACK_WORDS,
                                     NULL,
                                     MICROPHONE_TASK_PRIORITY,
                                     &g_microphone_task);
    if (created != pdPASS)
    {
        ESP_LOGE(TAG, "Microphone task creation failed");
        return;
    }

    g_microphone_started = true;
}
