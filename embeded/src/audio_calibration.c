#include "audio_calibration.h"

#include <stdio.h>
#include <string.h>

#include "audio_spectrum.h"

#define AUDIO_CALIBRATION_BIN_EXCLUSION_RADIUS 3u

static void sort_bands_by_frequency(audio_calibration_t *calibration)
{
    if (calibration == NULL)
    {
        return;
    }

    for (size_t i = 0; i < calibration->band_count; ++i)
    {
        for (size_t j = i + 1u; j < calibration->band_count; ++j)
        {
            if (calibration->bands[j].center_hz < calibration->bands[i].center_hz)
            {
                audio_calibration_band_t tmp = calibration->bands[i];
                calibration->bands[i] = calibration->bands[j];
                calibration->bands[j] = tmp;
            }
        }
    }
}

void audio_calibration_set_defaults(audio_calibration_t *calibration)
{
    if (calibration == NULL)
    {
        return;
    }

    memset(calibration, 0, sizeof(*calibration));
    calibration->version = AUDIO_CALIBRATION_VERSION;
    calibration->band_count = 0u;
    calibration->enabled = false;
    for (size_t i = 0; i < AUDIO_CALIBRATION_BAND_COUNT; ++i)
    {
        calibration->bands[i].threshold_multiplier_x100 = AUDIO_CALIBRATION_DEFAULT_MULTIPLIER_X100;
    }
}

bool audio_calibration_is_valid(const audio_calibration_t *calibration)
{
    if (calibration == NULL ||
        calibration->version != AUDIO_CALIBRATION_VERSION ||
        !calibration->enabled ||
        calibration->band_count == 0u ||
        calibration->band_count > AUDIO_CALIBRATION_BAND_COUNT)
    {
        return false;
    }

    for (size_t i = 0; i < calibration->band_count; ++i)
    {
        const audio_calibration_band_t *band = &calibration->bands[i];
        if (band->center_hz == 0u ||
            band->baseline_energy == 0u ||
            band->threshold_multiplier_x100 < AUDIO_CALIBRATION_MIN_MULTIPLIER_X100 ||
            band->threshold_multiplier_x100 > AUDIO_CALIBRATION_MAX_MULTIPLIER_X100)
        {
            return false;
        }
    }

    return true;
}

void audio_calibration_print(const audio_calibration_t *calibration)
{
    printf("\n===== Audio calibration =====\n");
    if (!audio_calibration_is_valid(calibration))
    {
        printf("Status: not calibrated\n");
        printf("=============================\n\n");
        return;
    }

    printf("Status: enabled, bands=%u\n", (unsigned int)calibration->band_count);
    printf("Band | Center Hz | Baseline | Trigger\n");
    for (size_t i = 0; i < calibration->band_count; ++i)
    {
        const audio_calibration_band_t *band = &calibration->bands[i];
        printf("%4lu | %9u | %8lu | %u.%02ux\n",
               (unsigned long)(i + 1u),
               (unsigned int)band->center_hz,
               (unsigned long)band->baseline_energy,
               (unsigned int)(band->threshold_multiplier_x100 / 100u),
               (unsigned int)(band->threshold_multiplier_x100 % 100u));
    }
    printf("=============================\n\n");
}

bool audio_calibration_set_multiplier(audio_calibration_t *calibration,
                                      size_t band_index,
                                      uint16_t multiplier_x100)
{
    if (calibration == NULL ||
        band_index >= calibration->band_count ||
        multiplier_x100 < AUDIO_CALIBRATION_MIN_MULTIPLIER_X100 ||
        multiplier_x100 > AUDIO_CALIBRATION_MAX_MULTIPLIER_X100)
    {
        return false;
    }

    calibration->bands[band_index].threshold_multiplier_x100 = multiplier_x100;
    return true;
}

bool audio_calibration_build_from_spectrum(audio_calibration_t *calibration,
                                           const uint32_t *average_bin_energy,
                                           size_t bin_count)
{
    if (calibration == NULL || average_bin_energy == NULL || bin_count < AUDIO_SPECTRUM_BIN_COUNT)
    {
        return false;
    }

    bool excluded[AUDIO_SPECTRUM_BIN_COUNT] = {false};
    audio_calibration_set_defaults(calibration);

    for (size_t band_index = 0; band_index < AUDIO_CALIBRATION_BAND_COUNT; ++band_index)
    {
        size_t best_bin = 0u;
        uint32_t best_energy = 0u;
        for (size_t bin = AUDIO_SPECTRUM_MIN_BIN; bin <= AUDIO_SPECTRUM_MAX_BIN; ++bin)
        {
            if (excluded[bin])
            {
                continue;
            }

            uint32_t center_hz = audio_spectrum_bin_to_hz(bin);
            uint32_t band_energy = audio_spectrum_band_energy(average_bin_energy, (uint16_t)center_hz);
            if (band_energy > best_energy)
            {
                best_energy = band_energy;
                best_bin = bin;
            }
        }

        if (best_bin == 0u || best_energy == 0u)
        {
            break;
        }

        calibration->bands[band_index].center_hz = audio_spectrum_bin_to_hz(best_bin);
        calibration->bands[band_index].baseline_energy = best_energy;
        calibration->bands[band_index].threshold_multiplier_x100 = AUDIO_CALIBRATION_DEFAULT_MULTIPLIER_X100;
        calibration->band_count++;

        size_t first_excluded = best_bin > AUDIO_CALIBRATION_BIN_EXCLUSION_RADIUS
                                    ? best_bin - AUDIO_CALIBRATION_BIN_EXCLUSION_RADIUS
                                    : 0u;
        size_t last_excluded = best_bin + AUDIO_CALIBRATION_BIN_EXCLUSION_RADIUS;
        if (last_excluded >= AUDIO_SPECTRUM_BIN_COUNT)
        {
            last_excluded = AUDIO_SPECTRUM_BIN_COUNT - 1u;
        }
        for (size_t bin = first_excluded; bin <= last_excluded; ++bin)
        {
            excluded[bin] = true;
        }
    }

    calibration->enabled = calibration->band_count == AUDIO_CALIBRATION_BAND_COUNT;
    sort_bands_by_frequency(calibration);
    return audio_calibration_is_valid(calibration);
}
