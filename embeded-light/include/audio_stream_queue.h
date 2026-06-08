#ifndef AUDIO_STREAM_QUEUE_H
#define AUDIO_STREAM_QUEUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "arm_math.h"

#define AUDIO_STREAM_QUEUE_SLOTS 64u
#define AUDIO_STREAM_QUEUE_CHUNK_SAMPLES 160u
#define AUDIO_STREAM_QUEUE_DOWNSAMPLE_FACTOR 3u
#define AUDIO_STREAM_QUEUE_I2S_Q31_GAIN_SHIFT 6
#define AUDIO_STREAM_QUEUE_INPUT_SAMPLE_RATE_HZ 48000u
#define AUDIO_STREAM_QUEUE_OUTPUT_SAMPLE_RATE_HZ (AUDIO_STREAM_QUEUE_INPUT_SAMPLE_RATE_HZ / AUDIO_STREAM_QUEUE_DOWNSAMPLE_FACTOR)
#define AUDIO_STREAM_QUEUE_CAPTURE_CHANNEL_INDEX 0u
#define AUDIO_DSP_FRAME_SLOTS 8u
#define AUDIO_DSP_FRAME_SAMPLES 256u

typedef struct
{
    uint16_t slot;
    size_t sample_count;
    const int16_t *samples;
} audio_chunk_view_t;

typedef struct
{
    uint16_t slot;
    size_t sample_count;
    uint64_t captured_at_us;
    const q15_t *samples;
} audio_dsp_frame_view_t;

void audio_stream_queue_init(void);
void audio_stream_queue_push_from_buffer(const int32_t *buffer, size_t words, uint64_t captured_at_us);
bool audio_stream_queue_peek(audio_chunk_view_t *chunk);
void audio_stream_queue_pop_if_matches(uint16_t slot);
uint16_t audio_stream_queue_depth(void);
uint32_t audio_stream_queue_dropped_chunks(void);
uint32_t audio_stream_queue_processed_buffers(void);

bool audio_dsp_frame_peek(audio_dsp_frame_view_t *frame);
void audio_dsp_frame_pop_if_matches(uint16_t slot);
uint16_t audio_dsp_frame_depth(void);
uint32_t audio_dsp_frame_dropped(void);
bool audio_dsp_frame_copy_q15(const audio_dsp_frame_view_t *frame, q15_t *out_samples, size_t out_count);
bool audio_dsp_frame_to_float32(const audio_dsp_frame_view_t *frame, float32_t *out_samples, size_t out_count);

#endif
