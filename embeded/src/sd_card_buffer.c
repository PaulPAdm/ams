#include "sd_card_buffer.h"

#include <stdio.h>
#include <string.h>

#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"

#define SD_SPI spi1
#define SD_PIN_MISO 8u
#define SD_PIN_CS 9u
#define SD_PIN_SCK 10u
#define SD_PIN_MOSI 11u

#define SD_SPI_INIT_BAUD_HZ 100000u
#define SD_SPI_RUN_BAUD_HZ 8000000u
#define SD_IDLE_TIMEOUT_MS 1500u
#define SD_READY_TIMEOUT_MS 10000u
#define SD_DATA_TIMEOUT_MS 500u
#define SD_COMMAND_READY_TIMEOUT_MS 200u
#define SD_COMMAND_RESPONSE_ATTEMPTS 64u
#define SD_BUFFER_BASE_BLOCK 32768u
#define SD_ACMD41_HCS_ARG 0x40000000u
#define SD_ACMD41_LEGACY_ARG 0u
#define SD_ACMD41_LOG_FIRST_ATTEMPTS 5u
#define SD_ACMD41_LOG_INTERVAL 100u
#define SD_WRAP_LOG_FIRST_COUNT 3u
#define SD_WRAP_LOG_INTERVAL 12u

#define SD_CMD0 0u
#define SD_CMD8 8u
#define SD_CMD16 16u
#define SD_CMD17 17u
#define SD_CMD24 24u
#define SD_CMD55 55u
#define SD_CMD58 58u
#define SD_ACMD41 41u

#define SD_R1_IDLE 0x01u
#define SD_R1_READY 0x00u
#define SD_DATA_TOKEN 0xFEu
#define SD_WRITE_ACCEPTED 0x05u

static void set_error(sd_card_buffer_t *buffer, const char *message)
{
    if (buffer != NULL)
    {
        buffer->last_error = message;
    }
}

static uint8_t spi_transfer(uint8_t value)
{
    uint8_t response = 0xFFu;
    spi_write_read_blocking(SD_SPI, &value, &response, 1);
    return response;
}

static bool sd_wait_ready(uint32_t timeout_ms);

static void sd_select(void)
{
    gpio_put(SD_PIN_CS, 0);
    spi_transfer(0xFFu);
}

static void sd_deselect(void)
{
    gpio_put(SD_PIN_CS, 1);
    spi_transfer(0xFFu);
}

static uint8_t sd_send_command(uint8_t cmd, uint32_t arg, uint8_t crc)
{
    uint8_t packet[6] = {
        (uint8_t)(0x40u | cmd),
        (uint8_t)(arg >> 24),
        (uint8_t)(arg >> 16),
        (uint8_t)(arg >> 8),
        (uint8_t)arg,
        crc};

    sd_select();
    if (!sd_wait_ready(SD_COMMAND_READY_TIMEOUT_MS))
    {
        sd_deselect();
        return 0xFFu;
    }
    spi_write_blocking(SD_SPI, packet, sizeof(packet));

    for (uint8_t i = 0; i < SD_COMMAND_RESPONSE_ATTEMPTS; ++i)
    {
        uint8_t response = spi_transfer(0xFFu);
        if ((response & 0x80u) == 0u)
        {
            return response;
        }
    }

    return 0xFFu;
}

static bool sd_wait_ready(uint32_t timeout_ms)
{
    absolute_time_t deadline = make_timeout_time_ms(timeout_ms);
    while (!time_reached(deadline))
    {
        if (spi_transfer(0xFFu) == 0xFFu)
        {
            return true;
        }
    }

    return false;
}

static bool sd_read_ocr(uint8_t ocr[4])
{
    uint8_t response = sd_send_command(SD_CMD58, 0u, 0x01u);
    if (response != SD_R1_READY && response != SD_R1_IDLE)
    {
        sd_deselect();
        return false;
    }

    for (size_t i = 0; i < 4u; ++i)
    {
        ocr[i] = spi_transfer(0xFFu);
    }

    sd_deselect();
    return true;
}

static bool sd_send_acmd41(uint32_t arg, uint8_t *cmd55_response, uint8_t *acmd41_response)
{
    if (cmd55_response != NULL)
    {
        *cmd55_response = 0xFFu;
    }
    if (acmd41_response != NULL)
    {
        *acmd41_response = 0xFFu;
    }

    uint8_t response = sd_send_command(SD_CMD55, 0u, 0x01u);
    if (cmd55_response != NULL)
    {
        *cmd55_response = response;
    }
    if (response > SD_R1_IDLE)
    {
        sd_deselect();
        return false;
    }

    response = sd_send_command(SD_ACMD41, arg, 0x01u);
    sd_deselect();
    if (acmd41_response != NULL)
    {
        *acmd41_response = response;
    }
    return response == SD_R1_READY;
}

static bool sd_wait_acmd41_ready(uint32_t arg,
                                 const char *label,
                                 uint32_t *attempts,
                                 uint8_t *last_cmd55_response,
                                 uint8_t *last_acmd41_response)
{
    absolute_time_t deadline = make_timeout_time_ms(SD_READY_TIMEOUT_MS);
    *attempts = 0u;
    *last_cmd55_response = 0xFFu;
    *last_acmd41_response = 0xFFu;

    while (!time_reached(deadline))
    {
        (*attempts)++;
        if (sd_send_acmd41(arg, last_cmd55_response, last_acmd41_response))
        {
            return true;
        }

        if (*attempts <= SD_ACMD41_LOG_FIRST_ATTEMPTS ||
            (*attempts % SD_ACMD41_LOG_INTERVAL) == 0u)
        {
            printf("[SD] ACMD41 %s attempt=%lu CMD55=0x%02X ACMD41=0x%02X\n",
                   label,
                   (unsigned long)*attempts,
                   *last_cmd55_response,
                   *last_acmd41_response);
        }

        sleep_ms(10);
    }

    return false;
}

static bool sd_initialize_card(sd_card_buffer_t *buffer)
{
    sd_deselect();
    for (uint8_t i = 0; i < 20u; ++i)
    {
        spi_transfer(0xFFu);
    }

    uint8_t response = 0xFFu;
    absolute_time_t idle_deadline = make_timeout_time_ms(SD_IDLE_TIMEOUT_MS);
    while (!time_reached(idle_deadline))
    {
        response = sd_send_command(SD_CMD0, 0u, 0x95u);
        sd_deselect();
        if (response == SD_R1_IDLE)
        {
            break;
        }
    }

    if (response != SD_R1_IDLE)
    {
        set_error(buffer, "SD CMD0 failed");
        return false;
    }
    printf("[SD] CMD0 OK: response=0x%02X\n", response);

    bool v2_card = false;
    response = sd_send_command(SD_CMD8, 0x000001AAu, 0x87u);
    if (response == SD_R1_IDLE)
    {
        uint8_t r7[4] = {0};
        for (size_t i = 0; i < 4u; ++i)
        {
            r7[i] = spi_transfer(0xFFu);
        }
        v2_card = r7[2] == 0x01u && r7[3] == 0xAAu;
        printf("[SD] CMD8 response=0x%02X R7=%02X %02X %02X %02X v2=%s\n",
               response,
               r7[0],
               r7[1],
               r7[2],
               r7[3],
               v2_card ? "yes" : "no");
    }
    else
    {
        printf("[SD] CMD8 response=0x%02X, treating card as SD v1/legacy\n", response);
    }
    sd_deselect();

    uint8_t last_cmd55_response = 0xFFu;
    uint8_t last_acmd41_response = 0xFFu;
    uint32_t acmd41_attempts = 0u;
    uint32_t acmd41_arg = v2_card ? SD_ACMD41_HCS_ARG : SD_ACMD41_LEGACY_ARG;
    bool ready = sd_wait_acmd41_ready(acmd41_arg,
                                      v2_card ? "HCS" : "legacy",
                                      &acmd41_attempts,
                                      &last_cmd55_response,
                                      &last_acmd41_response);

    if (!ready && v2_card)
    {
        printf("[SD] ACMD41 HCS did not finish; retrying legacy arg=0x%08lX\n",
               (unsigned long)SD_ACMD41_LEGACY_ARG);
        acmd41_arg = SD_ACMD41_LEGACY_ARG;
        ready = sd_wait_acmd41_ready(acmd41_arg,
                                     "legacy",
                                     &acmd41_attempts,
                                     &last_cmd55_response,
                                     &last_acmd41_response);
    }

    if (!ready)
    {
        printf("[SD] ACMD41 timeout after %lu attempts: last CMD55=0x%02X last ACMD41=0x%02X arg=0x%08lX\n",
               (unsigned long)acmd41_attempts,
               last_cmd55_response,
               last_acmd41_response,
               (unsigned long)acmd41_arg);
        set_error(buffer, "SD ACMD41 failed");
        return false;
    }
    printf("[SD] ACMD41 OK after %lu attempts: last CMD55=0x%02X last ACMD41=0x%02X\n",
           (unsigned long)acmd41_attempts,
           last_cmd55_response,
           last_acmd41_response);

    uint8_t ocr[4] = {0};
    if (!sd_read_ocr(ocr))
    {
        set_error(buffer, "SD OCR read failed");
        return false;
    }
    buffer->block_addressed = v2_card && ((ocr[0] & 0x40u) != 0u);
    printf("[SD] OCR=%02X %02X %02X %02X block_addressed=%s\n",
           ocr[0],
           ocr[1],
           ocr[2],
           ocr[3],
           buffer->block_addressed ? "yes" : "no");

    if (!buffer->block_addressed)
    {
        response = sd_send_command(SD_CMD16, SD_CARD_BLOCK_SIZE, 0x01u);
        sd_deselect();
        if (response != SD_R1_READY)
        {
            set_error(buffer, "SD CMD16 failed");
            return false;
        }
    }

    return true;
}

static bool sd_write_block(sd_card_buffer_t *buffer, uint32_t relative_block, const uint8_t data[SD_CARD_BLOCK_SIZE])
{
    uint32_t block = buffer->base_block + relative_block;
    uint32_t address = buffer->block_addressed ? block : block * SD_CARD_BLOCK_SIZE;
    uint8_t response = sd_send_command(SD_CMD24, address, 0x01u);
    if (response != SD_R1_READY)
    {
        sd_deselect();
        set_error(buffer, "SD CMD24 failed");
        return false;
    }

    spi_transfer(0xFFu);
    spi_transfer(SD_DATA_TOKEN);
    spi_write_blocking(SD_SPI, data, SD_CARD_BLOCK_SIZE);
    spi_transfer(0xFFu);
    spi_transfer(0xFFu);

    uint8_t data_response = (uint8_t)(spi_transfer(0xFFu) & 0x1Fu);
    if (data_response != SD_WRITE_ACCEPTED)
    {
        sd_deselect();
        set_error(buffer, "SD write rejected");
        return false;
    }

    if (!sd_wait_ready(SD_DATA_TIMEOUT_MS))
    {
        sd_deselect();
        set_error(buffer, "SD write timeout");
        return false;
    }

    sd_deselect();
    return true;
}

static bool sd_read_block(sd_card_buffer_t *buffer, uint32_t relative_block, uint8_t data[SD_CARD_BLOCK_SIZE])
{
    uint32_t block = buffer->base_block + relative_block;
    uint32_t address = buffer->block_addressed ? block : block * SD_CARD_BLOCK_SIZE;
    uint8_t response = sd_send_command(SD_CMD17, address, 0x01u);
    if (response != SD_R1_READY)
    {
        sd_deselect();
        set_error(buffer, "SD CMD17 failed");
        return false;
    }

    absolute_time_t deadline = make_timeout_time_ms(SD_DATA_TIMEOUT_MS);
    while (!time_reached(deadline))
    {
        uint8_t token = spi_transfer(0xFFu);
        if (token == SD_DATA_TOKEN)
        {
            spi_read_blocking(SD_SPI, 0xFFu, data, SD_CARD_BLOCK_SIZE);
            spi_transfer(0xFFu);
            spi_transfer(0xFFu);
            sd_deselect();
            return true;
        }
    }

    sd_deselect();
    set_error(buffer, "SD read timeout");
    return false;
}

static bool sd_flush_pending_block(sd_card_buffer_t *buffer)
{
    if (buffer == NULL || !buffer->ready || buffer->pending_bytes != SD_CARD_BLOCK_SIZE)
    {
        return false;
    }

    if (!sd_write_block(buffer, buffer->next_block_index, buffer->pending_block))
    {
        buffer->ready = false;
        buffer->failed_writes++;
        printf("SD circular audio buffer write failed: %s\n", sd_card_buffer_last_error(buffer));
        return false;
    }

    buffer->next_block_index = (buffer->next_block_index + 1u) % buffer->block_count;
    buffer->pending_bytes = 0u;
    buffer->written_blocks++;
    if (buffer->next_block_index == 0u)
    {
        buffer->wrap_count++;
        if (buffer->wrap_count <= SD_WRAP_LOG_FIRST_COUNT ||
            (buffer->wrap_count % SD_WRAP_LOG_INTERVAL) == 0u)
        {
            printf("[SD] Audio circular buffer wrapped: wraps=%lu written_blocks=%lu capacity=%lu bytes\n",
                   (unsigned long)buffer->wrap_count,
                   (unsigned long)buffer->written_blocks,
                   (unsigned long)SD_CARD_AUDIO_BUFFER_CAPACITY_BYTES);
        }
    }
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
    buffer->last_error = "Not initialized";
    buffer->block_count = SD_CARD_AUDIO_BUFFER_BLOCKS;

    spi_init(SD_SPI, SD_SPI_INIT_BAUD_HZ);
    gpio_set_function(SD_PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(SD_PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(SD_PIN_MOSI, GPIO_FUNC_SPI);
    gpio_init(SD_PIN_CS);
    gpio_put(SD_PIN_CS, 1);
    gpio_set_dir(SD_PIN_CS, GPIO_OUT);
    gpio_pull_up(SD_PIN_MISO);
    gpio_pull_up(SD_PIN_CS);
    sleep_ms(10);

    if (!sd_initialize_card(buffer))
    {
        printf("SD card buffer unavailable: %s\n", sd_card_buffer_last_error(buffer));
        return false;
    }

    buffer->base_block = SD_BUFFER_BASE_BLOCK;
    spi_set_baudrate(SD_SPI, SD_SPI_RUN_BAUD_HZ);
    memset(buffer->pending_block, 0, sizeof(buffer->pending_block));
    buffer->ready = true;
    buffer->last_error = NULL;

    printf("SD circular audio buffer ready: %u sec, %u bytes requested, %lu bytes capacity, base_block=%lu, blocks=%lu\n",
           (unsigned)SD_CARD_AUDIO_BUFFER_SECONDS,
           (unsigned)SD_CARD_AUDIO_BUFFER_BYTES,
           (unsigned long)SD_CARD_AUDIO_BUFFER_CAPACITY_BYTES,
           (unsigned long)buffer->base_block,
           (unsigned long)buffer->block_count);
    return true;
}

bool sd_card_buffer_run_startup_test(sd_card_buffer_t *buffer)
{
    printf("[SD] Startup test begins on SPI1: MISO=GP8 CS=GP9 SCK=GP10 MOSI=GP11\n");
    if (!sd_card_buffer_init(buffer))
    {
        printf("[SD] Startup test failed during init: %s\n", sd_card_buffer_last_error(buffer));
        return false;
    }

    uint8_t expected[SD_CARD_BLOCK_SIZE];
    uint8_t actual[SD_CARD_BLOCK_SIZE];
    for (size_t i = 0; i < SD_CARD_BLOCK_SIZE; ++i)
    {
        expected[i] = (uint8_t)(0xA5u ^ (uint8_t)i ^ (uint8_t)(i >> 3));
    }

    printf("[SD] Self-test writes raw test block=%lu bytes=%u\n",
           (unsigned long)buffer->base_block,
           (unsigned)SD_CARD_BLOCK_SIZE);

    if (!sd_write_block(buffer, 0u, expected))
    {
        buffer->ready = false;
        buffer->failed_writes++;
        printf("[SD] Self-test write failed: %s\n", sd_card_buffer_last_error(buffer));
        return false;
    }

    memset(actual, 0, sizeof(actual));
    if (!sd_read_block(buffer, 0u, actual))
    {
        buffer->ready = false;
        printf("[SD] Self-test read failed: %s\n", sd_card_buffer_last_error(buffer));
        return false;
    }

    for (size_t i = 0; i < SD_CARD_BLOCK_SIZE; ++i)
    {
        if (actual[i] != expected[i])
        {
            buffer->ready = false;
            set_error(buffer, "SD self-test verify failed");
            printf("[SD] Self-test verify failed at byte=%lu expected=0x%02X actual=0x%02X\n",
                   (unsigned long)i,
                   expected[i],
                   actual[i]);
            return false;
        }
    }

    printf("[SD] Self-test OK: write/read/verify passed for raw block=%lu\n",
           (unsigned long)buffer->base_block);
    return true;
}

bool sd_card_buffer_is_initialized(const sd_card_buffer_t *buffer)
{
    return buffer != NULL && buffer->initialized;
}

bool sd_card_buffer_is_ready(const sd_card_buffer_t *buffer)
{
    return buffer != NULL && buffer->ready;
}

bool sd_card_buffer_write_audio(sd_card_buffer_t *buffer, const int16_t *samples, size_t sample_count)
{
    if (buffer == NULL || samples == NULL || sample_count == 0u)
    {
        return false;
    }

    if (!buffer->ready)
    {
        return false;
    }

    const uint8_t *source = (const uint8_t *)samples;
    size_t remaining = sample_count * sizeof(int16_t);
    while (remaining > 0u)
    {
        size_t free_bytes = SD_CARD_BLOCK_SIZE - buffer->pending_bytes;
        size_t copy_bytes = remaining < free_bytes ? remaining : free_bytes;
        memcpy(&buffer->pending_block[buffer->pending_bytes], source, copy_bytes);
        buffer->pending_bytes = (uint16_t)(buffer->pending_bytes + copy_bytes);
        source += copy_bytes;
        remaining -= copy_bytes;

        if (buffer->pending_bytes == SD_CARD_BLOCK_SIZE && !sd_flush_pending_block(buffer))
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
    return sd_flush_pending_block(buffer);
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
        return "";
    }

    return buffer->last_error;
}
