#include <string.h>

#include "arm_math.h"
#include "hardware/sync.h"
#include "audio_stream_queue.h"

#define AUDIO_STREAM_QUEUE_INPUT_BLOCK_FRAMES 48u
#define AUDIO_STREAM_QUEUE_DECIMATOR_TAPS 63u
#define AUDIO_STREAM_QUEUE_DECIMATOR_OUTPUT_SAMPLES (AUDIO_STREAM_QUEUE_INPUT_BLOCK_FRAMES / AUDIO_STREAM_QUEUE_DOWNSAMPLE_FACTOR)

static const q31_t AUDIO_DECIMATOR_COEFFS[AUDIO_STREAM_QUEUE_DECIMATOR_TAPS] = {
    1108251, 1873413, 1324122, -512263, -2641051, -3304044, -1147653,
    3191637, 6609458, 5363783, -1508115, -9970289, -12749508, -4975422,
    10282831, 21916538, 18120509, -3106204, -28848703, -37845341, -16508912,
    26726931, 61526088, 54320548, -4535821, -84425699, -124209435, -66802319,
    101088932, 330600555, 529071503, 607415002, 529071503, 330600555, 101088932,
    -66802319, -124209435, -84425699, -4535821, 54320548, 61526088, 26726931,
    -16508912, -37845341, -28848703, -3106204, 18120509, 21916538, 10282831,
    -4975422, -12749508, -9970289, -1508115, 5363783, 6609458, 3191637,
    -1147653, -3304044, -2641051, -512263, 1324122, 1873413, 1108251};

static q15_t g_audio_queue[AUDIO_STREAM_QUEUE_SLOTS][AUDIO_STREAM_QUEUE_CHUNK_SAMPLES];
static uint16_t g_audio_queue_samples[AUDIO_STREAM_QUEUE_SLOTS];
static uint64_t g_audio_capture_us[AUDIO_STREAM_QUEUE_SLOTS];
static q15_t g_pending_chunk[AUDIO_STREAM_QUEUE_CHUNK_SAMPLES];
static q15_t g_dsp_frame_queue[AUDIO_DSP_FRAME_SLOTS][AUDIO_DSP_FRAME_SAMPLES];
static uint16_t g_dsp_frame_samples[AUDIO_DSP_FRAME_SLOTS];
static uint64_t g_dsp_frame_capture_us[AUDIO_DSP_FRAME_SLOTS];
static q15_t g_pending_dsp_frame[AUDIO_DSP_FRAME_SAMPLES];
static size_t g_pending_count = 0;
static size_t g_pending_dsp_count = 0;
static volatile uint16_t g_audio_write_idx = 0;
static volatile uint16_t g_audio_read_idx = 0;
static volatile uint16_t g_audio_count = 0;
static volatile uint32_t g_audio_dropped = 0;
static volatile uint16_t g_dsp_frame_write_idx = 0;
static volatile uint16_t g_dsp_frame_read_idx = 0;
static volatile uint16_t g_dsp_frame_count = 0;
static volatile uint32_t g_dsp_frame_dropped = 0;
static volatile uint32_t g_processed_input_buffers = 0;
static arm_fir_decimate_instance_q31 g_decimator;
static q31_t g_decimator_state[AUDIO_STREAM_QUEUE_DECIMATOR_TAPS + AUDIO_STREAM_QUEUE_INPUT_BLOCK_FRAMES - 1u];
static q31_t g_decimator_input[AUDIO_STREAM_QUEUE_INPUT_BLOCK_FRAMES];
static q31_t g_decimator_output[AUDIO_STREAM_QUEUE_DECIMATOR_OUTPUT_SAMPLES];
static q31_t g_decimator_scaled[AUDIO_STREAM_QUEUE_DECIMATOR_OUTPUT_SAMPLES];
static q15_t g_decimator_pcm[AUDIO_STREAM_QUEUE_DECIMATOR_OUTPUT_SAMPLES];
static bool g_decimator_ready = false;

static void init_decimator(void)
{
    arm_status status = arm_fir_decimate_init_q31(&g_decimator,
                                                  AUDIO_STREAM_QUEUE_DECIMATOR_TAPS,
                                                  AUDIO_STREAM_QUEUE_DOWNSAMPLE_FACTOR,
                                                  AUDIO_DECIMATOR_COEFFS,
                                                  g_decimator_state,
                                                  AUDIO_STREAM_QUEUE_INPUT_BLOCK_FRAMES);
    g_decimator_ready = status == ARM_MATH_SUCCESS;
}

static void commit_pending_samples(q15_t *pending_samples,
                                   size_t *pending_count,
                                   size_t slot_capacity,
                                   q15_t *queue_samples,
                                   uint16_t *queued_sample_counts,
                                   uint64_t *capture_times,
                                   uint64_t captured_at_us,
                                   volatile uint16_t *write_idx,
                                   volatile uint16_t *queued_count,
                                   volatile uint32_t *dropped_count,
                                   uint16_t slot_count)
{
    if (*pending_count == 0)
    {
        return;
    }

    if (*queued_count >= slot_count)
    {
        (*dropped_count)++;
        *pending_count = 0;
        return;
    }

    uint16_t slot = *write_idx;
    q15_t *slot_samples = queue_samples + ((size_t)slot * slot_capacity);
    arm_copy_q15(pending_samples, slot_samples, (uint32_t)(*pending_count));
    queued_sample_counts[slot] = (uint16_t)(*pending_count);
    if (capture_times != NULL)
    {
        capture_times[slot] = captured_at_us;
    }

    *write_idx = (uint16_t)((slot + 1u) % slot_count);
    (*queued_count)++;
    *pending_count = 0;
}

static bool peek_queue_samples(uint16_t *slot,
                               size_t *sample_count,
                               const q15_t **samples,
                               uint64_t *captured_at_us,
                               const q15_t *queue_samples,
                               const uint16_t *queued_sample_counts,
                               const uint64_t *capture_times,
                               volatile uint16_t *read_idx,
                               volatile uint16_t *queued_count,
                               size_t slot_capacity)
{
    uint32_t irq_state = save_and_disable_interrupts();
    if (*queued_count == 0)
    {
        restore_interrupts(irq_state);
        return false;
    }

    uint16_t queue_slot = *read_idx;
    *slot = queue_slot;
    *sample_count = queued_sample_counts[queue_slot];
    if (captured_at_us != NULL)
    {
        *captured_at_us = capture_times != NULL ? capture_times[queue_slot] : 0;
    }
    *samples = queue_samples + ((size_t)queue_slot * slot_capacity);
    restore_interrupts(irq_state);
    return true;
}

static void pop_queue_slot_if_matches(uint16_t slot,
                                      volatile uint16_t *read_idx,
                                      volatile uint16_t *queued_count,
                                      uint16_t slot_count)
{
    uint32_t irq_state = save_and_disable_interrupts();
    if (*queued_count > 0 && *read_idx == slot)
    {
        *read_idx = (uint16_t)((*read_idx + 1u) % slot_count);
        (*queued_count)--;
    }
    restore_interrupts(irq_state);
}

static uint16_t read_queue_depth(volatile uint16_t *queued_count)
{
    uint32_t irq_state = save_and_disable_interrupts();
    uint16_t depth = *queued_count;
    restore_interrupts(irq_state);
    return depth;
}

static uint32_t read_queue_counter(volatile uint32_t *counter)
{
    uint32_t irq_state = save_and_disable_interrupts();
    uint32_t value = *counter;
    restore_interrupts(irq_state);
    return value;
}

static bool validate_dsp_frame_output(const audio_dsp_frame_view_t *frame,
                                      size_t out_count,
                                      uint32_t *sample_count)
{
    if (frame == NULL || sample_count == NULL || out_count < frame->sample_count || frame->samples == NULL)
    {
        return false;
    }

    *sample_count = (uint32_t)frame->sample_count;
    return true;
}

static uint64_t g_pending_chunk_capture_us = 0;

static void commit_pending_chunk(uint64_t captured_at_us)
{
    commit_pending_samples(g_pending_chunk,
                           &g_pending_count,
                           AUDIO_STREAM_QUEUE_CHUNK_SAMPLES,
                           &g_audio_queue[0][0],
                           g_audio_queue_samples,
                           g_audio_capture_us,
                           captured_at_us,
                           &g_audio_write_idx,
                           &g_audio_count,
                           &g_audio_dropped,
                           AUDIO_STREAM_QUEUE_SLOTS);
}

static void commit_pending_dsp_frame(uint64_t captured_at_us)
{
    commit_pending_samples(g_pending_dsp_frame,
                           &g_pending_dsp_count,
                           AUDIO_DSP_FRAME_SAMPLES,
                           &g_dsp_frame_queue[0][0],
                           g_dsp_frame_samples,
                           g_dsp_frame_capture_us,
                           captured_at_us,
                           &g_dsp_frame_write_idx,
                           &g_dsp_frame_count,
                           &g_dsp_frame_dropped,
                           AUDIO_DSP_FRAME_SLOTS);
}

static void append_output_sample(q15_t sample, uint64_t captured_at_us)
{
    g_pending_chunk[g_pending_count++] = sample;
    if (g_pending_count == 1u)
    {
        g_pending_chunk_capture_us = captured_at_us;
    }
    if (g_pending_count >= AUDIO_STREAM_QUEUE_CHUNK_SAMPLES)
    {
        commit_pending_chunk(g_pending_chunk_capture_us);
    }

    g_pending_dsp_frame[g_pending_dsp_count++] = sample;
}

void audio_stream_queue_init(void)
{
    memset(g_audio_queue, 0, sizeof(g_audio_queue));
    memset(g_audio_queue_samples, 0, sizeof(g_audio_queue_samples));
    memset(g_audio_capture_us, 0, sizeof(g_audio_capture_us));
    memset(g_dsp_frame_queue, 0, sizeof(g_dsp_frame_queue));
    memset(g_dsp_frame_samples, 0, sizeof(g_dsp_frame_samples));
    memset(g_dsp_frame_capture_us, 0, sizeof(g_dsp_frame_capture_us));
    memset(g_pending_chunk, 0, sizeof(g_pending_chunk));
    memset(g_pending_dsp_frame, 0, sizeof(g_pending_dsp_frame));
    g_pending_count = 0;
    g_pending_dsp_count = 0;
    g_audio_write_idx = 0;
    g_audio_read_idx = 0;
    g_audio_count = 0;
    g_audio_dropped = 0;
    g_dsp_frame_write_idx = 0;
    g_dsp_frame_read_idx = 0;
    g_dsp_frame_count = 0;
    g_dsp_frame_dropped = 0;
    g_processed_input_buffers = 0;
    init_decimator();
}

void audio_stream_queue_push_from_buffer(const int32_t *buffer, size_t words, uint64_t captured_at_us)
{
    if (buffer == NULL || words < 2)
    {
        return;
    }

    size_t frames = words / 2;
    if (!g_decimator_ready || frames != AUDIO_STREAM_QUEUE_INPUT_BLOCK_FRAMES)
    {
        return;
    }

    for (size_t i = 0; i < frames; ++i)
    {
        const int32_t *frame = &buffer[i * 2u];
        g_decimator_input[i] = (q31_t)frame[AUDIO_STREAM_QUEUE_CAPTURE_CHANNEL_INDEX];
    }

    arm_fir_decimate_q31(&g_decimator, g_decimator_input, g_decimator_output, (uint32_t)frames);
    arm_shift_q31(g_decimator_output,
                  AUDIO_STREAM_QUEUE_I2S_Q31_GAIN_SHIFT,
                  g_decimator_scaled,
                  AUDIO_STREAM_QUEUE_DECIMATOR_OUTPUT_SAMPLES);
    arm_q31_to_q15(g_decimator_scaled, g_decimator_pcm, AUDIO_STREAM_QUEUE_DECIMATOR_OUTPUT_SAMPLES);

    for (size_t i = 0; i < AUDIO_STREAM_QUEUE_DECIMATOR_OUTPUT_SAMPLES; ++i)
    {
        append_output_sample(g_decimator_pcm[i], captured_at_us);
        if (g_pending_dsp_count >= AUDIO_DSP_FRAME_SAMPLES)
        {
            commit_pending_dsp_frame(captured_at_us);
        }
    }

    g_processed_input_buffers++;
}

bool audio_stream_queue_peek(audio_chunk_view_t *chunk)
{
    if (chunk == NULL)
    {
        return false;
    }

    chunk->slot = 0;
    chunk->sample_count = 0;
    chunk->captured_at_us = 0;
    chunk->samples = NULL;

    const q15_t *samples = NULL;
    if (!peek_queue_samples(&chunk->slot,
                            &chunk->sample_count,
                            &samples,
                            &chunk->captured_at_us,
                            &g_audio_queue[0][0],
                            g_audio_queue_samples,
                            g_audio_capture_us,
                            &g_audio_read_idx,
                            &g_audio_count,
                            AUDIO_STREAM_QUEUE_CHUNK_SAMPLES))
    {
        return false;
    }

    chunk->samples = (const int16_t *)samples;
    return true;
}

void audio_stream_queue_pop_if_matches(uint16_t slot)
{
    pop_queue_slot_if_matches(slot, &g_audio_read_idx, &g_audio_count, AUDIO_STREAM_QUEUE_SLOTS);
}

uint16_t audio_stream_queue_depth(void)
{
    return read_queue_depth(&g_audio_count);
}

uint32_t audio_stream_queue_dropped_chunks(void)
{
    return read_queue_counter(&g_audio_dropped);
}

uint32_t audio_stream_queue_processed_buffers(void)
{
    return read_queue_counter(&g_processed_input_buffers);
}

bool audio_dsp_frame_peek(audio_dsp_frame_view_t *frame)
{
    if (frame == NULL)
    {
        return false;
    }

    frame->slot = 0;
    frame->sample_count = 0;
    frame->captured_at_us = 0;
    frame->samples = NULL;

    return peek_queue_samples(&frame->slot,
                              &frame->sample_count,
                              &frame->samples,
                              &frame->captured_at_us,
                              &g_dsp_frame_queue[0][0],
                              g_dsp_frame_samples,
                              g_dsp_frame_capture_us,
                              &g_dsp_frame_read_idx,
                              &g_dsp_frame_count,
                              AUDIO_DSP_FRAME_SAMPLES);
}

void audio_dsp_frame_pop_if_matches(uint16_t slot)
{
    pop_queue_slot_if_matches(slot, &g_dsp_frame_read_idx, &g_dsp_frame_count, AUDIO_DSP_FRAME_SLOTS);
}

uint16_t audio_dsp_frame_depth(void)
{
    return read_queue_depth(&g_dsp_frame_count);
}

uint32_t audio_dsp_frame_dropped(void)
{
    return read_queue_counter(&g_dsp_frame_dropped);
}

bool audio_dsp_frame_copy_q15(const audio_dsp_frame_view_t *frame, q15_t *out_samples, size_t out_count)
{
    uint32_t sample_count = 0;
    if (out_samples == NULL || !validate_dsp_frame_output(frame, out_count, &sample_count))
    {
        return false;
    }

    arm_copy_q15(frame->samples, out_samples, sample_count);
    return true;
}

bool audio_dsp_frame_to_float32(const audio_dsp_frame_view_t *frame, float32_t *out_samples, size_t out_count)
{
    uint32_t sample_count = 0;
    if (out_samples == NULL || !validate_dsp_frame_output(frame, out_count, &sample_count))
    {
        return false;
    }

    arm_q15_to_float(frame->samples, out_samples, sample_count);
    return true;
}
