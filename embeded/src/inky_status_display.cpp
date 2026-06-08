#include "inky_status_display.h"

#include <cstdio>
#include <cstring>

#include "pico/time.h"
#include "uc8151.hpp"
#include "pico_graphics.hpp"

namespace {

using namespace pimoroni;

constexpr uint16_t kDisplayWidth = 296;
constexpr uint16_t kDisplayHeight = 128;
constexpr size_t kFrameBufferSize = (kDisplayWidth * kDisplayHeight) / 8;

constexpr uint kDisplayCsPin = 17;
constexpr uint kDisplayClockPin = 18;
constexpr uint kDisplayMosiPin = 19;
constexpr uint kDisplayDcPin = 20;
constexpr uint kDisplayResetPin = 21;
constexpr uint kDisplayBusyPin = 26;

constexpr int kTextLeft = 4;
constexpr int kTextTop = 4;
constexpr int kLineStep = 12;
constexpr int kTextWrap = kDisplayWidth - (kTextLeft * 2);
constexpr uint32_t kDisplayRefreshIntervalMs = 5000;

struct display_context_t {
    uint8_t framebuffer[kFrameBufferSize];
    UC8151 display;
    PicoGraphics_Pen1BitY graphics;
    device_status_snapshot_t last_status;
    device_config_t last_config;
    absolute_time_t last_refresh_at;
    bool has_rendered;

    display_context_t()
        : framebuffer{},
          display(kDisplayWidth,
                  kDisplayHeight,
                  ROTATE_0,
                  {spi0, kDisplayCsPin, kDisplayClockPin, kDisplayMosiPin, PIN_UNUSED, kDisplayDcPin, PIN_UNUSED},
                  kDisplayBusyPin,
                  kDisplayResetPin),
          graphics(kDisplayWidth, kDisplayHeight, framebuffer),
          last_status{},
          last_config{},
          last_refresh_at{},
          has_rendered(false)
    {
        graphics.set_font("bitmap8");
        display.set_update_speed(1);
        device_status_snapshot_init(&last_status);
    }
};

display_context_t *get_display_context()
{
    static display_context_t context;
    return &context;
}

void draw_line(PicoGraphics_Pen1BitY &graphics, int line_index, const char *text)
{
    graphics.text(text, Point(kTextLeft, kTextTop + (line_index * kLineStep)), kTextWrap, 1.0f);
}

void format_line(char *out, size_t out_size, const char *label, const char *value)
{
    if (out == nullptr || out_size == 0)
    {
        return;
    }

    const char *safe_value = (value != nullptr && value[0] != '\0') ? value : "-";
    std::snprintf(out, out_size, "%s%.28s", label, safe_value);
    out[out_size - 1] = '\0';
}

void format_status_line(char *out, size_t out_size, const char *label, const char *value)
{
    std::snprintf(out, out_size, "%s%s", label, value);
    out[out_size - 1] = '\0';
}

void format_power_line(char *out, size_t out_size, const device_status_snapshot_t *status)
{
    if (status != nullptr && status->has_power_reading)
    {
        std::snprintf(out, out_size, "Power: %.2fV %.0fmA", status->bus_voltage_v, status->current_ma);
    }
    else
    {
        std::snprintf(out, out_size, "Power: -");
    }
    out[out_size - 1] = '\0';
}

bool config_equals(const device_config_t *lhs, const device_config_t *rhs)
{
    return std::strncmp(lhs->ssid, rhs->ssid, sizeof(lhs->ssid)) == 0 &&
           std::strncmp(lhs->device_id, rhs->device_id, sizeof(lhs->device_id)) == 0;
}

bool display_status_equals(const device_status_snapshot_t *lhs, const device_status_snapshot_t *rhs)
{
    return device_status_snapshot_equals(lhs, rhs);
}

void render_screen(display_context_t *context,
                   const device_config_t *config,
                   const device_status_snapshot_t *status,
                   int raw_audio_sample_rate_hz,
                   int audio_downsample_factor)
{
    (void)raw_audio_sample_rate_hz;
    (void)audio_downsample_factor;

    PicoGraphics_Pen1BitY &graphics = context->graphics;
    char line_buffer[40];

    graphics.set_pen(15);
    graphics.clear();
    graphics.set_pen(0);

    draw_line(graphics, 0, "AMS status");
    format_line(line_buffer, sizeof(line_buffer), "ID: ", config != nullptr ? config->device_id : nullptr);
    draw_line(graphics, 1, line_buffer);
    format_line(line_buffer, sizeof(line_buffer), "WiFi: ", config != nullptr ? config->ssid : nullptr);
    draw_line(graphics, 2, line_buffer);

    format_status_line(line_buffer, sizeof(line_buffer), "Mic: ", device_component_state_label(status->microphone));
    draw_line(graphics, 3, line_buffer);
    format_status_line(line_buffer, sizeof(line_buffer), "INA219: ", device_component_state_label(status->ina219));
    draw_line(graphics, 4, line_buffer);
    format_status_line(line_buffer, sizeof(line_buffer), "Wi-Fi: ", device_wifi_state_label(status->wifi));
    draw_line(graphics, 5, line_buffer);
    format_status_line(line_buffer, sizeof(line_buffer), "Server: ", device_server_state_label(status->server));
    draw_line(graphics, 6, line_buffer);
    format_power_line(line_buffer, sizeof(line_buffer), status);
    draw_line(graphics, 7, line_buffer);

    context->display.update(&graphics);
}

void render_missing_settings_screen(display_context_t *context, bool usb_console_connected)
{
    PicoGraphics_Pen1BitY &graphics = context->graphics;

    graphics.set_pen(15);
    graphics.clear();
    graphics.set_pen(0);

    draw_line(graphics, 0, "AMS setup required");
    draw_line(graphics, 2, "No saved settings");
    if (usb_console_connected)
    {
        draw_line(graphics, 4, "USB console ready");
        draw_line(graphics, 5, "Enter WiFi/server/id");
    }
    else
    {
        draw_line(graphics, 4, "Connect USB console");
        draw_line(graphics, 5, "then reboot to setup");
    }

    context->display.update(&graphics);
}

void render_audio_calibration_screen(display_context_t *context, const audio_calibration_t *calibration)
{
    PicoGraphics_Pen1BitY &graphics = context->graphics;
    char line_buffer[40];

    graphics.set_pen(15);
    graphics.clear();
    graphics.set_pen(0);

    draw_line(graphics, 0, "Audio calibration");
    if (!audio_calibration_is_valid(calibration))
    {
        draw_line(graphics, 2, "No valid bands");
        draw_line(graphics, 3, "Use USB console");
        context->display.update(&graphics);
        return;
    }

    std::snprintf(line_buffer, sizeof(line_buffer), "Bands: %u", (unsigned int)calibration->band_count);
    draw_line(graphics, 1, line_buffer);
    for (size_t i = 0; i < calibration->band_count && i < 5u; ++i)
    {
        const audio_calibration_band_t &band = calibration->bands[i];
        std::snprintf(line_buffer,
                      sizeof(line_buffer),
                      "%uHz x%u.%02u",
                      (unsigned int)band.center_hz,
                      (unsigned int)(band.threshold_multiplier_x100 / 100u),
                      (unsigned int)(band.threshold_multiplier_x100 % 100u));
        draw_line(graphics, (int)i + 2, line_buffer);
    }

    context->display.update(&graphics);
}

} // namespace

extern "C" void inky_status_display_reset(void)
{
    display_context_t *context = get_display_context();
    context->has_rendered = false;
    device_status_snapshot_init(&context->last_status);
    std::memset(&context->last_config, 0, sizeof(context->last_config));
}

extern "C" void inky_status_display_show_missing_settings(bool usb_console_connected)
{
    display_context_t *context = get_display_context();
    render_missing_settings_screen(context, usb_console_connected);
    context->last_refresh_at = get_absolute_time();
    context->has_rendered = false;
}

extern "C" void inky_status_display_update(const device_config_t *config,
                                           const device_status_snapshot_t *status,
                                           int raw_audio_sample_rate_hz,
                                           int audio_downsample_factor)
{
    if (config == nullptr || status == nullptr)
    {
        return;
    }

    display_context_t *context = get_display_context();
    bool config_changed = !context->has_rendered || !config_equals(config, &context->last_config);
    bool status_changed = !context->has_rendered || !display_status_equals(status, &context->last_status);

    if (!config_changed && !status_changed)
    {
        return;
    }

    absolute_time_t now = get_absolute_time();
    bool interval_elapsed = !context->has_rendered ||
                            absolute_time_diff_us(context->last_refresh_at, now) >=
                                ((int64_t)kDisplayRefreshIntervalMs * 1000ll);

    if (!config_changed && !interval_elapsed)
    {
        return;
    }

    render_screen(context, config, status, raw_audio_sample_rate_hz, audio_downsample_factor);
    context->last_status = *status;
    context->last_config = *config;
    context->last_refresh_at = now;
    context->has_rendered = true;
}

extern "C" void inky_status_display_show_audio_calibration(const audio_calibration_t *calibration)
{
    display_context_t *context = get_display_context();
    render_audio_calibration_screen(context, calibration);
    context->last_refresh_at = get_absolute_time();
    context->has_rendered = false;
}
