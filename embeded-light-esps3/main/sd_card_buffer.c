#include "sd_card_buffer.h"

#include <stdio.h>
#include <string.h>

#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "sdmmc_cmd.h"

#include "device_runtime_config.h"

static const char *TAG = "sd-buffer";
static sdmmc_card_t g_card;
static sdmmc_host_t g_host;
static bool g_card_ready = false;

static void set_error(sd_card_buffer_t *buffer, const char *message)
{
    if (buffer != NULL)
    {
        buffer->last_error = message;
    }
}

static esp_err_t sd_platform_init(void)
{
    if (g_card_ready)
    {
        return ESP_OK;
    }

    g_host = (sdmmc_host_t)SDSPI_HOST_DEFAULT();
    g_host.slot = DEVICE_SD_SPI_HOST;
    g_host.max_freq_khz = 8000;
    g_host.flags |= SDMMC_HOST_FLAG_ALLOC_ALIGNED_BUF;

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = DEVICE_SD_PIN_MOSI,
        .miso_io_num = DEVICE_SD_PIN_MISO,
        .sclk_io_num = DEVICE_SD_PIN_SCK,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = SD_CARD_BLOCK_SIZE,
    };

    esp_err_t ret = spi_bus_initialize(g_host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE)
    {
        ESP_RETURN_ON_ERROR(ret, TAG, "SPI bus init failed");
    }

    sdspi_device_config_t device_cfg = SDSPI_DEVICE_CONFIG_DEFAULT();
    device_cfg.host_id = g_host.slot;
    device_cfg.gpio_cs = DEVICE_SD_PIN_CS;

    sdspi_dev_handle_t sdspi_handle;
    ESP_RETURN_ON_ERROR(sdspi_host_init_device(&device_cfg, &sdspi_handle), TAG, "SDSPI device init failed");
    g_host.slot = sdspi_handle;

    memset(&g_card, 0, sizeof(g_card));
    ESP_RETURN_ON_ERROR(sdmmc_card_init(&g_host, &g_card), TAG, "SD card init failed");
    g_card_ready = true;

    sdmmc_card_print_info(stdout, &g_card);
    return ESP_OK;
}

static bool sd_write_block(sd_card_buffer_t *buffer, uint32_t relative_block, const uint8_t data[SD_CARD_BLOCK_SIZE])
{
    if (!g_card_ready)
    {
        set_error(buffer, "SD card is not initialized");
        return false;
    }

    uint32_t block = buffer->base_block + relative_block;
    esp_err_t ret = sdmmc_write_sectors(&g_card, data, block, 1);
    if (ret != ESP_OK)
    {
        set_error(buffer, "SD write failed");
        ESP_LOGW(TAG, "write block=%lu failed: %s", (unsigned long)block, esp_err_to_name(ret));
        return false;
    }

    return true;
}

static bool sd_read_block(sd_card_buffer_t *buffer, uint32_t relative_block, uint8_t data[SD_CARD_BLOCK_SIZE])
{
    if (!g_card_ready)
    {
        set_error(buffer, "SD card is not initialized");
        return false;
    }

    uint32_t block = buffer->base_block + relative_block;
    esp_err_t ret = sdmmc_read_sectors(&g_card, data, block, 1);
    if (ret != ESP_OK)
    {
        set_error(buffer, "SD read failed");
        ESP_LOGW(TAG, "read block=%lu failed: %s", (unsigned long)block, esp_err_to_name(ret));
        return false;
    }

    return true;
}

static bool flush_pending_block(sd_card_buffer_t *buffer)
{
    if (buffer == NULL || !buffer->ready || buffer->pending_bytes != SD_CARD_BLOCK_SIZE)
    {
        return true;
    }

    if (!sd_write_block(buffer, buffer->next_block_index, buffer->pending_block))
    {
        buffer->ready = false;
        buffer->failed_writes++;
        return false;
    }

    buffer->written_blocks++;
    buffer->next_block_index = (buffer->next_block_index + 1u) % buffer->block_count;
    if (buffer->next_block_index == 0u)
    {
        buffer->wrap_count++;
    }
    buffer->pending_bytes = 0;
    memset(buffer->pending_block, 0, sizeof(buffer->pending_block));
    return true;
}

bool sd_card_buffer_init(sd_card_buffer_t *buffer)
{
    if (buffer == NULL)
    {
        return false;
    }

    memset(buffer, 0, sizeof(*buffer));
    buffer->initialized = true;
    buffer->base_block = DEVICE_SD_BUFFER_BASE_BLOCK;
    buffer->block_count = SD_CARD_AUDIO_BUFFER_BLOCKS;
    buffer->last_error = "Not initialized";

    esp_err_t ret = sd_platform_init();
    if (ret != ESP_OK)
    {
        set_error(buffer, "SD platform init failed");
        ESP_LOGW(TAG, "SD platform init failed: %s", esp_err_to_name(ret));
        return false;
    }

    buffer->ready = true;
    buffer->last_error = NULL;
    ESP_LOGI(TAG,
             "Raw circular buffer ready: base_block=%lu blocks=%lu capacity=%lu bytes",
             (unsigned long)buffer->base_block,
             (unsigned long)buffer->block_count,
             (unsigned long)SD_CARD_AUDIO_BUFFER_CAPACITY_BYTES);
    return true;
}

bool sd_card_buffer_run_startup_test(sd_card_buffer_t *buffer)
{
    ESP_LOGI(TAG,
             "Startup test begins for Adafruit XTSD 2 GB: SCK=GPIO%d MISO=GPIO%d MOSI=GPIO%d CS=GPIO%d",
             DEVICE_SD_PIN_SCK,
             DEVICE_SD_PIN_MISO,
             DEVICE_SD_PIN_MOSI,
             DEVICE_SD_PIN_CS);

    if (!sd_card_buffer_init(buffer))
    {
        ESP_LOGW(TAG, "Startup test failed during init: %s", sd_card_buffer_last_error(buffer));
        return false;
    }

    uint8_t *expected = heap_caps_malloc(SD_CARD_BLOCK_SIZE, MALLOC_CAP_DMA);
    uint8_t *actual = heap_caps_malloc(SD_CARD_BLOCK_SIZE, MALLOC_CAP_DMA);
    if (expected == NULL || actual == NULL)
    {
        heap_caps_free(expected);
        heap_caps_free(actual);
        set_error(buffer, "SD self-test allocation failed");
        return false;
    }

    for (size_t i = 0; i < SD_CARD_BLOCK_SIZE; ++i)
    {
        expected[i] = (uint8_t)(0xA5u ^ (uint8_t)i ^ (uint8_t)(i >> 3));
        actual[i] = 0;
    }

    bool ok = sd_write_block(buffer, 0u, expected) && sd_read_block(buffer, 0u, actual);
    if (ok && memcmp(expected, actual, SD_CARD_BLOCK_SIZE) != 0)
    {
        ok = false;
        set_error(buffer, "SD self-test verify failed");
    }

    heap_caps_free(expected);
    heap_caps_free(actual);

    if (!ok)
    {
        buffer->ready = false;
        ESP_LOGW(TAG, "Self-test failed: %s", sd_card_buffer_last_error(buffer));
        return false;
    }

    ESP_LOGI(TAG, "Self-test OK: raw write/read/verify block=%lu", (unsigned long)buffer->base_block);
    return true;
}

bool sd_card_buffer_is_ready(const sd_card_buffer_t *buffer)
{
    return buffer != NULL && buffer->ready;
}

bool sd_card_buffer_is_initialized(const sd_card_buffer_t *buffer)
{
    return buffer != NULL && buffer->initialized;
}

bool sd_card_buffer_write_audio(sd_card_buffer_t *buffer, const int16_t *samples, size_t sample_count)
{
    if (buffer == NULL || samples == NULL || sample_count == 0u || !buffer->ready)
    {
        return false;
    }

    const uint8_t *bytes = (const uint8_t *)samples;
    size_t bytes_remaining = sample_count * sizeof(int16_t);
    while (bytes_remaining > 0u)
    {
        size_t free_bytes = SD_CARD_BLOCK_SIZE - buffer->pending_bytes;
        size_t to_copy = bytes_remaining < free_bytes ? bytes_remaining : free_bytes;
        memcpy(&buffer->pending_block[buffer->pending_bytes], bytes, to_copy);
        buffer->pending_bytes += to_copy;
        bytes += to_copy;
        bytes_remaining -= to_copy;

        if (buffer->pending_bytes == SD_CARD_BLOCK_SIZE && !flush_pending_block(buffer))
        {
            return false;
        }
    }

    return true;
}

bool sd_card_buffer_finalize_snapshot(sd_card_buffer_t *buffer)
{
    if (buffer == NULL || !buffer->ready)
    {
        return false;
    }

    if (buffer->pending_bytes == 0u)
    {
        return true;
    }

    memset(&buffer->pending_block[buffer->pending_bytes], 0, SD_CARD_BLOCK_SIZE - buffer->pending_bytes);
    buffer->pending_bytes = SD_CARD_BLOCK_SIZE;
    return flush_pending_block(buffer);
}

uint32_t sd_card_buffer_snapshot_block_count(const sd_card_buffer_t *buffer)
{
    if (buffer == NULL || !buffer->ready)
    {
        return 0u;
    }

    return buffer->written_blocks < buffer->block_count ? buffer->written_blocks : buffer->block_count;
}

uint32_t sd_card_buffer_snapshot_size_bytes(const sd_card_buffer_t *buffer)
{
    return sd_card_buffer_snapshot_block_count(buffer) * SD_CARD_BLOCK_SIZE;
}

uint32_t sd_card_buffer_snapshot_duration_ms(const sd_card_buffer_t *buffer)
{
    uint32_t size_bytes = sd_card_buffer_snapshot_size_bytes(buffer);
    uint32_t bytes_per_second = SD_CARD_AUDIO_SAMPLE_RATE_HZ * SD_CARD_AUDIO_BYTES_PER_SAMPLE;
    if (bytes_per_second == 0u)
    {
        return 0u;
    }

    return (uint32_t)(((uint64_t)size_bytes * 1000ull) / bytes_per_second);
}

bool sd_card_buffer_read_snapshot_block(sd_card_buffer_t *buffer,
                                        uint32_t snapshot_block_index,
                                        uint8_t data[SD_CARD_BLOCK_SIZE])
{
    if (buffer == NULL || data == NULL || !buffer->ready)
    {
        return false;
    }

    uint32_t valid_blocks = sd_card_buffer_snapshot_block_count(buffer);
    if (snapshot_block_index >= valid_blocks)
    {
        set_error(buffer, "SD snapshot block out of range");
        return false;
    }

    uint32_t oldest_block = buffer->written_blocks >= buffer->block_count ? buffer->next_block_index : 0u;
    uint32_t relative_block = (oldest_block + snapshot_block_index) % buffer->block_count;
    return sd_read_block(buffer, relative_block, data);
}

const char *sd_card_buffer_last_error(const sd_card_buffer_t *buffer)
{
    if (buffer == NULL || buffer->last_error == NULL)
    {
        return "none";
    }
    return buffer->last_error;
}
