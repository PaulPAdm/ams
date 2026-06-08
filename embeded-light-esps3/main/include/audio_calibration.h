#ifndef AUDIO_CALIBRATION_H
#define AUDIO_CALIBRATION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AUDIO_CALIBRATION_VERSION 1u
#define AUDIO_CALIBRATION_BAND_COUNT 6u
#define AUDIO_CALIBRATION_DEFAULT_MULTIPLIER_X100 200u
#define AUDIO_CALIBRATION_MIN_MULTIPLIER_X100 110u
#define AUDIO_CALIBRATION_MAX_MULTIPLIER_X100 1000u

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    uint16_t center_hz;
    uint16_t threshold_multiplier_x100;
    uint32_t baseline_energy;
} audio_calibration_band_t;

typedef struct
{
    uint32_t version;
    uint8_t band_count;
    bool enabled;
    audio_calibration_band_t bands[AUDIO_CALIBRATION_BAND_COUNT];
} audio_calibration_t;

void audio_calibration_set_defaults(audio_calibration_t *calibration);
bool audio_calibration_is_valid(const audio_calibration_t *calibration);
void audio_calibration_print(const audio_calibration_t *calibration);
bool audio_calibration_set_multiplier(audio_calibration_t *calibration,
                                      size_t band_index,
                                      uint16_t multiplier_x100);
bool audio_calibration_build_from_spectrum(audio_calibration_t *calibration,
                                           const uint32_t *average_bin_energy,
                                           size_t bin_count);

#ifdef __cplusplus
}
#endif

#endif
