#include <string.h>
#include "hardware/sync.h"
#include "audio_stream_queue.h"

static int16_t g_audio_queue[AUDIO_STREAM_QUEUE_SLOTS][AUDIO_STREAM_QUEUE_CHUNK_SAMPLES];
static uint16_t g_audio_queue_samples[AUDIO_STREAM_QUEUE_SLOTS];
static int16_t g_pending_chunk[AUDIO_STREAM_QUEUE_CHUNK_SAMPLES];
static size_t g_pending_count = 0;
static volatile uint16_t g_audio_write_idx = 0;
static volatile uint16_t g_audio_read_idx = 0;
static volatile uint16_t g_audio_count = 0;
static volatile uint32_t g_audio_dropped = 0;

static void commit_pending_chunk(void)
{
    if (g_pending_count == 0)
    {
        return;
    }

    if (g_audio_count >= AUDIO_STREAM_QUEUE_SLOTS)
    {
        g_audio_dropped++;
        g_pending_count = 0;
        return;
    }

    uint16_t slot = g_audio_write_idx;
    memcpy(g_audio_queue[slot], g_pending_chunk, g_pending_count * sizeof(int16_t));
    g_audio_queue_samples[slot] = (uint16_t)g_pending_count;
    g_audio_write_idx = (uint16_t)((g_audio_write_idx + 1u) % AUDIO_STREAM_QUEUE_SLOTS);
    g_audio_count++;
    g_pending_count = 0;
}

void audio_stream_queue_init(void)
{
    memset(g_audio_queue, 0, sizeof(g_audio_queue));
    memset(g_audio_queue_samples, 0, sizeof(g_audio_queue_samples));
    memset(g_pending_chunk, 0, sizeof(g_pending_chunk));
    g_pending_count = 0;
    g_audio_write_idx = 0;
    g_audio_read_idx = 0;
    g_audio_count = 0;
    g_audio_dropped = 0;
}

void audio_stream_queue_push_from_buffer(const int32_t *buffer, size_t words)
{
    if (buffer == NULL || words < 2)
    {
        return;
    }

    size_t frames = words / 2;

    const size_t step = AUDIO_STREAM_QUEUE_DOWNSAMPLE_FACTOR;
    for (size_t i = 0; i + (step - 1u) < frames; i += step)
    {
        // Anti-alias filter for decimation: average one downsample window.
        int64_t acc = 0;
        for (size_t j = 0; j < step; ++j)
        {
            acc += (int64_t)buffer[((i + j) * 2) + 1];
        }

        int32_t sample_r = (int32_t)(acc / (int64_t)step);
        int32_t sample16 = (sample_r >> 8) >> AUDIO_STREAM_QUEUE_ATTENUATION_SHIFT;
        if (sample16 > 32767)
        {
            sample16 = 32767;
        }
        else if (sample16 < -32768)
        {
            sample16 = -32768;
        }

        g_pending_chunk[g_pending_count++] = (int16_t)sample16;
        if (g_pending_count >= AUDIO_STREAM_QUEUE_CHUNK_SAMPLES)
        {
            commit_pending_chunk();
        }
    }
}

bool audio_stream_queue_peek(audio_chunk_view_t *chunk)
{
    if (chunk == NULL)
    {
        return false;
    }

    chunk->slot = 0;
    chunk->sample_count = 0;
    chunk->samples = NULL;

    uint32_t irq_state = save_and_disable_interrupts();
    if (g_audio_count > 0)
    {
        uint16_t slot = g_audio_read_idx;
        chunk->slot = slot;
        chunk->sample_count = g_audio_queue_samples[slot];
        chunk->samples = g_audio_queue[slot];
        restore_interrupts(irq_state);
        return true;
    }
    restore_interrupts(irq_state);
    return false;
}

void audio_stream_queue_pop_if_matches(uint16_t slot)
{
    uint32_t irq_state = save_and_disable_interrupts();
    if (g_audio_count > 0 && g_audio_read_idx == slot)
    {
        g_audio_read_idx = (uint16_t)((g_audio_read_idx + 1u) % AUDIO_STREAM_QUEUE_SLOTS);
        g_audio_count--;
    }
    restore_interrupts(irq_state);
}
