#include "microphone.h"

#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "i2s.h"

static const uint SEL_PIN = 3;
static const uint SEL_LEVEL = 1;

static __attribute__((aligned(8))) pio_i2s g_i2s;
static volatile microphone_buffer_callback_t g_buffer_callback = NULL;

static void dma_i2s_in_handler(void)
{
    int32_t *completed = NULL;
    uint64_t buffer_ready_us = time_us_64();
    if (*(int32_t **)dma_hw->ch[g_i2s.dma_ch_in_ctrl].read_addr == g_i2s.input_buffer)
    {
        completed = &g_i2s.input_buffer[STEREO_BUFFER_SIZE];
    }
    else
    {
        completed = g_i2s.input_buffer;
    }

    microphone_buffer_callback_t callback = g_buffer_callback;
    if (callback != NULL)
    {
        callback(completed, STEREO_BUFFER_SIZE, buffer_ready_us);
    }

    dma_hw->ints0 = 1u << g_i2s.dma_ch_in_data;
}

void microphone_set_buffer_callback(microphone_buffer_callback_t callback)
{
    g_buffer_callback = callback;
}

void microphone_init(void)
{
    // 132 MHz gives clean dividers for common I2S rates such as 48 kHz.
    set_sys_clock_khz(132000, true);

    // Keep SEL high; diagnostics show this appears as capture slot 0 in this I2S pipeline.
    gpio_init(SEL_PIN);
    gpio_set_dir(SEL_PIN, GPIO_OUT);
    gpio_put(SEL_PIN, SEL_LEVEL);

    i2s_config mic_cfg = i2s_config_default;
    mic_cfg.sck_enable = false;
    mic_cfg.dout_pin = 12; // Required by sync init; keep this GPIO unconnected.
    mic_cfg.din_pin = 0;
    mic_cfg.clock_pin_base = 1;

    i2s_program_start_synched(pio0, &mic_cfg, dma_i2s_in_handler, &g_i2s);
}
