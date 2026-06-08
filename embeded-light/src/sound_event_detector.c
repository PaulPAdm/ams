#include "sound_event_detector.h"

#include <string.h>

#define SOUND_EVENT_MIN_PEAK_ABS 12000u
#define SOUND_EVENT_MIN_MEAN_ABS 1800u
#define SOUND_EVENT_MEAN_TO_NOISE_RATIO_X100 350u
#define SOUND_EVENT_PEAK_TO_NOISE_RATIO_X100 700u
#define SOUND_EVENT_NOISE_FLOOR_ALPHA_SHIFT 4u
#define SOUND_EVENT_COOLDOWN_CHUNKS 500u
#define SOUND_EVENT_BOOT_SETTLE_CHUNKS 100u

static uint16_t abs_i16(int16_t value)
{
    return value >= 0 ? (uint16_t)value : (uint16_t)(-(int32_t)value);
}

static bool find_calibrated_band_trigger(sound_event_detector_t *detector,
                                         const int16_t *samples,
                                         size_t sample_count,
                                         sound_event_detector_result_t *result)
{
    if (detector == NULL || samples == NULL || !audio_calibration_is_valid(detector->calibration))
    {
        return false;
    }

    uint32_t bin_energy[AUDIO_SPECTRUM_BIN_COUNT] = {0};
    if (!audio_spectrum_analyze_q15(&detector->spectrum_analyzer, samples, sample_count, bin_energy))
    {
        return false;
    }

    bool matched = false;
    uint32_t best_ratio_x100 = 0u;
    const audio_calibration_band_t *best_band = NULL;
    uint32_t best_energy = 0u;

    for (size_t i = 0; i < detector->calibration->band_count; ++i)
    {
        const audio_calibration_band_t *band = &detector->calibration->bands[i];
        uint32_t baseline = band->baseline_energy > 0u ? band->baseline_energy : 1u;
        uint32_t current_energy = audio_spectrum_band_energy(bin_energy, band->center_hz);
        uint32_t ratio_x100 = (uint32_t)(((uint64_t)current_energy * 100u) / baseline);
        uint64_t threshold = ((uint64_t)baseline * band->threshold_multiplier_x100) / 100u;

        if ((uint64_t)current_energy >= threshold && ratio_x100 >= best_ratio_x100)
        {
            matched = true;
            best_ratio_x100 = ratio_x100;
            best_band = band;
            best_energy = current_energy;
        }
    }

    if (result != NULL && best_band != NULL)
    {
        result->matched_center_hz = best_band->center_hz;
        result->matched_multiplier_x100 = best_band->threshold_multiplier_x100;
        result->matched_band_energy = best_energy;
        result->matched_baseline_energy = best_band->baseline_energy;
        result->matched_ratio_x100 = best_ratio_x100;
    }

    return matched;
}

void sound_event_detector_init(sound_event_detector_t *detector, const audio_calibration_t *calibration)
{
    if (detector == NULL)
    {
        return;
    }

    memset(detector, 0, sizeof(*detector));
    detector->calibration = calibration;
    audio_spectrum_analyzer_init(&detector->spectrum_analyzer);
}

bool sound_event_detector_process_chunk(sound_event_detector_t *detector,
                                        const int16_t *samples,
                                        size_t sample_count,
                                        sound_event_detector_result_t *result)
{
    if (detector == NULL || samples == NULL || sample_count == 0u)
    {
        return false;
    }

    uint16_t peak_abs = 0u;
    uint64_t abs_sum = 0u;
    for (size_t i = 0; i < sample_count; ++i)
    {
        uint16_t sample_abs = abs_i16(samples[i]);
        if (sample_abs > peak_abs)
        {
            peak_abs = sample_abs;
        }
        abs_sum += sample_abs;
    }

    uint16_t mean_abs = (uint16_t)(abs_sum / sample_count);
    detector->chunks_seen++;

    if (!detector->noise_floor_ready)
    {
        detector->noise_floor_abs = mean_abs;
        detector->noise_floor_ready = true;
    }
    else if (detector->cooldown_chunks == 0u)
    {
        detector->noise_floor_abs += ((int32_t)mean_abs - (int32_t)detector->noise_floor_abs) >> SOUND_EVENT_NOISE_FLOOR_ALPHA_SHIFT;
    }

    if (detector->cooldown_chunks > 0u)
    {
        detector->cooldown_chunks--;
    }

    if (result != NULL)
    {
        result->peak_abs = peak_abs;
        result->mean_abs = mean_abs;
        result->noise_floor_abs = detector->noise_floor_abs;
        result->matched_center_hz = 0u;
        result->matched_multiplier_x100 = 0u;
        result->matched_band_energy = 0u;
        result->matched_baseline_energy = 0u;
        result->matched_ratio_x100 = 0u;
    }

    if (detector->chunks_seen < SOUND_EVENT_BOOT_SETTLE_CHUNKS || detector->cooldown_chunks > 0u)
    {
        return false;
    }

    uint32_t noise_floor = detector->noise_floor_abs > 1u ? detector->noise_floor_abs : 1u;
    bool loud_enough = peak_abs >= SOUND_EVENT_MIN_PEAK_ABS && mean_abs >= SOUND_EVENT_MIN_MEAN_ABS;
    if (audio_calibration_is_valid(detector->calibration))
    {
        if (loud_enough && find_calibrated_band_trigger(detector, samples, sample_count, result))
        {
            detector->cooldown_chunks = SOUND_EVENT_COOLDOWN_CHUNKS;
            return true;
        }

        return false;
    }

    bool sharp_mean_rise = ((uint32_t)mean_abs * 100u) >= noise_floor * SOUND_EVENT_MEAN_TO_NOISE_RATIO_X100;
    bool sharp_peak_rise = ((uint32_t)peak_abs * 100u) >= noise_floor * SOUND_EVENT_PEAK_TO_NOISE_RATIO_X100;

    if (loud_enough && sharp_mean_rise && sharp_peak_rise)
    {
        detector->cooldown_chunks = SOUND_EVENT_COOLDOWN_CHUNKS;
        return true;
    }

    return false;
}
