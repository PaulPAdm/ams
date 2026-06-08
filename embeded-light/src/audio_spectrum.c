#include "audio_spectrum.h"

#include <limits.h>
#include <string.h>

#define AUDIO_SPECTRUM_ENERGY_SCALE 100000.0f
#define AUDIO_SPECTRUM_Q15_SCALE (1.0f / 32768.0f)

static uint32_t scale_energy(float32_t energy)
{
    if (energy <= 0.0f)
    {
        return 0u;
    }

    float32_t scaled = energy * AUDIO_SPECTRUM_ENERGY_SCALE;
    if (scaled >= (float32_t)UINT32_MAX)
    {
        return UINT32_MAX;
    }

    return (uint32_t)scaled;
}

bool audio_spectrum_analyzer_init(audio_spectrum_analyzer_t *analyzer)
{
    if (analyzer == NULL)
    {
        return false;
    }

    memset(analyzer, 0, sizeof(*analyzer));
    analyzer->ready = arm_rfft_fast_init_f32(&analyzer->fft, AUDIO_SPECTRUM_FFT_SIZE) == ARM_MATH_SUCCESS;
    return analyzer->ready;
}

bool audio_spectrum_analyze_q15(audio_spectrum_analyzer_t *analyzer,
                                const int16_t *samples,
                                size_t sample_count,
                                uint32_t out_bin_energy[AUDIO_SPECTRUM_BIN_COUNT])
{
    if (analyzer == NULL || samples == NULL || out_bin_energy == NULL || !analyzer->ready)
    {
        return false;
    }

    size_t copy_count = sample_count < AUDIO_SPECTRUM_FFT_SIZE ? sample_count : AUDIO_SPECTRUM_FFT_SIZE;
    for (size_t i = 0; i < copy_count; ++i)
    {
        analyzer->input[i] = (float32_t)samples[i] * AUDIO_SPECTRUM_Q15_SCALE;
    }
    for (size_t i = copy_count; i < AUDIO_SPECTRUM_FFT_SIZE; ++i)
    {
        analyzer->input[i] = 0.0f;
    }

    arm_rfft_fast_f32(&analyzer->fft, analyzer->input, analyzer->output, 0);

    out_bin_energy[0] = scale_energy((analyzer->output[0] * analyzer->output[0]) / AUDIO_SPECTRUM_FFT_SIZE);
    out_bin_energy[AUDIO_SPECTRUM_BIN_COUNT - 1u] =
        scale_energy((analyzer->output[1] * analyzer->output[1]) / AUDIO_SPECTRUM_FFT_SIZE);

    for (size_t bin = 1u; bin < AUDIO_SPECTRUM_BIN_COUNT - 1u; ++bin)
    {
        float32_t real = analyzer->output[bin * 2u];
        float32_t imag = analyzer->output[(bin * 2u) + 1u];
        float32_t energy = ((real * real) + (imag * imag)) / AUDIO_SPECTRUM_FFT_SIZE;
        out_bin_energy[bin] = scale_energy(energy);
    }

    return true;
}

uint16_t audio_spectrum_bin_to_hz(size_t bin)
{
    return (uint16_t)((bin * AUDIO_SPECTRUM_SAMPLE_RATE_HZ) / AUDIO_SPECTRUM_FFT_SIZE);
}

size_t audio_spectrum_hz_to_bin(uint16_t hz)
{
    size_t bin = ((size_t)hz * AUDIO_SPECTRUM_FFT_SIZE + (AUDIO_SPECTRUM_SAMPLE_RATE_HZ / 2u)) /
                 AUDIO_SPECTRUM_SAMPLE_RATE_HZ;
    if (bin >= AUDIO_SPECTRUM_BIN_COUNT)
    {
        return AUDIO_SPECTRUM_BIN_COUNT - 1u;
    }
    return bin;
}

uint32_t audio_spectrum_band_energy(const uint32_t bin_energy[AUDIO_SPECTRUM_BIN_COUNT],
                                    uint16_t center_hz)
{
    if (bin_energy == NULL || center_hz == 0u)
    {
        return 0u;
    }

    size_t center_bin = audio_spectrum_hz_to_bin(center_hz);
    size_t first_bin = center_bin > AUDIO_SPECTRUM_BAND_HALF_WIDTH_BINS
                           ? center_bin - AUDIO_SPECTRUM_BAND_HALF_WIDTH_BINS
                           : 0u;
    size_t last_bin = center_bin + AUDIO_SPECTRUM_BAND_HALF_WIDTH_BINS;
    if (last_bin >= AUDIO_SPECTRUM_BIN_COUNT)
    {
        last_bin = AUDIO_SPECTRUM_BIN_COUNT - 1u;
    }

    uint64_t sum = 0u;
    for (size_t bin = first_bin; bin <= last_bin; ++bin)
    {
        sum += bin_energy[bin];
    }

    return sum > UINT32_MAX ? UINT32_MAX : (uint32_t)sum;
}
