#ifndef SD_CARD_BUFFER_H
#define SD_CARD_BUFFER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SD_CARD_AUDIO_BUFFER_SECONDS 5u
#define SD_CARD_AUDIO_SAMPLE_RATE_HZ 16000u
#define SD_CARD_AUDIO_BYTES_PER_SAMPLE 2u
#define SD_CARD_BLOCK_SIZE 512u
#define SD_CARD_AUDIO_BUFFER_BYTES (SD_CARD_AUDIO_BUFFER_SECONDS * SD_CARD_AUDIO_SAMPLE_RATE_HZ * SD_CARD_AUDIO_BYTES_PER_SAMPLE)
#define SD_CARD_AUDIO_BUFFER_BLOCKS ((SD_CARD_AUDIO_BUFFER_BYTES + SD_CARD_BLOCK_SIZE - 1u) / SD_CARD_BLOCK_SIZE)
#define SD_CARD_AUDIO_BUFFER_CAPACITY_BYTES (SD_CARD_AUDIO_BUFFER_BLOCKS * SD_CARD_BLOCK_SIZE)

typedef struct
{
    bool initialized;
    bool ready;
    bool block_addressed;
    uint32_t base_block;
    uint32_t block_count;
    uint32_t next_block_index;
    uint32_t written_blocks;
    uint32_t wrap_count;
    uint32_t failed_writes;
    uint16_t pending_bytes;
    uint8_t pending_block[SD_CARD_BLOCK_SIZE];
    const char *last_error;
} sd_card_buffer_t;

bool sd_card_buffer_init(sd_card_buffer_t *buffer);
bool sd_card_buffer_run_startup_test(sd_card_buffer_t *buffer);
bool sd_card_buffer_is_initialized(const sd_card_buffer_t *buffer);
bool sd_card_buffer_is_ready(const sd_card_buffer_t *buffer);
bool sd_card_buffer_write_audio(sd_card_buffer_t *buffer, const int16_t *samples, size_t sample_count);
bool sd_card_buffer_finalize_snapshot(sd_card_buffer_t *buffer);
uint32_t sd_card_buffer_snapshot_block_count(const sd_card_buffer_t *buffer);
uint32_t sd_card_buffer_snapshot_size_bytes(const sd_card_buffer_t *buffer);
uint32_t sd_card_buffer_snapshot_duration_ms(const sd_card_buffer_t *buffer);
bool sd_card_buffer_read_snapshot_block(sd_card_buffer_t *buffer,
                                        uint32_t snapshot_block_index,
                                        uint8_t data[SD_CARD_BLOCK_SIZE]);
const char *sd_card_buffer_last_error(const sd_card_buffer_t *buffer);

#endif
