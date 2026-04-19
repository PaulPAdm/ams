#ifndef AUDIO_STREAM_QUEUE_H
#define AUDIO_STREAM_QUEUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AUDIO_STREAM_QUEUE_SLOTS 64u
#define AUDIO_STREAM_QUEUE_CHUNK_SAMPLES 160u
#define AUDIO_STREAM_QUEUE_DOWNSAMPLE_FACTOR 3u
#define AUDIO_STREAM_QUEUE_ATTENUATION_SHIFT 1

typedef struct
{
    uint16_t slot;
    size_t sample_count;
    const int16_t *samples;
} audio_chunk_view_t;

void audio_stream_queue_init(void);
void audio_stream_queue_push_from_buffer(const int32_t *buffer, size_t words);
bool audio_stream_queue_peek(audio_chunk_view_t *chunk);
void audio_stream_queue_pop_if_matches(uint16_t slot);

#endif
