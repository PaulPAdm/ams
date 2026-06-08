#ifndef SOUND_EVENT_DETECTOR_H
#define SOUND_EVENT_DETECTOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "audio_calibration.h"
#include "audio_spectrum.h"

typedef struct
{
    audio_spectrum_analyzer_t spectrum_analyzer;
    const audio_calibration_t *calibration;
    uint32_t chunks_seen;
    uint32_t cooldown_chunks;
    uint32_t noise_floor_abs;
    bool noise_floor_ready;
} sound_event_detector_t;

typedef struct
{
    uint16_t peak_abs;
    uint16_t mean_abs;
    uint32_t noise_floor_abs;
    uint16_t matched_center_hz;
    uint16_t matched_multiplier_x100;
    uint32_t matched_band_energy;
    uint32_t matched_baseline_energy;
    uint32_t matched_ratio_x100;
} sound_event_detector_result_t;

void sound_event_detector_init(sound_event_detector_t *detector, const audio_calibration_t *calibration);
bool sound_event_detector_process_chunk(sound_event_detector_t *detector,
                                        const int16_t *samples,
                                        size_t sample_count,
                                        sound_event_detector_result_t *result);

#endif
