#ifndef AUDIO_SPECTRUM_H
#define AUDIO_SPECTRUM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "audio_types.h"

#define AUDIO_SPECTRUM_FFT_SIZE 256u
#define AUDIO_SPECTRUM_BIN_COUNT (AUDIO_SPECTRUM_FFT_SIZE / 2u)
#define AUDIO_SPECTRUM_SAMPLE_RATE_HZ 16000u
#define AUDIO_SPECTRUM_MIN_BIN 2u
#define AUDIO_SPECTRUM_MAX_BIN (AUDIO_SPECTRUM_BIN_COUNT - 2u)
#define AUDIO_SPECTRUM_BAND_HALF_WIDTH_BINS 1u

typedef struct
{
    float32_t input[AUDIO_SPECTRUM_FFT_SIZE];
    float32_t coeffs[AUDIO_SPECTRUM_BIN_COUNT];
    bool ready;
} audio_spectrum_analyzer_t;

bool audio_spectrum_analyzer_init(audio_spectrum_analyzer_t *analyzer);
bool audio_spectrum_analyze_q15(audio_spectrum_analyzer_t *analyzer,
                                const int16_t *samples,
                                size_t sample_count,
                                uint32_t out_bin_energy[AUDIO_SPECTRUM_BIN_COUNT]);
uint16_t audio_spectrum_bin_to_hz(size_t bin);
size_t audio_spectrum_hz_to_bin(uint16_t hz);
uint32_t audio_spectrum_band_energy(const uint32_t bin_energy[AUDIO_SPECTRUM_BIN_COUNT],
                                    uint16_t center_hz);

#endif
