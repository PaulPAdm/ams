#ifndef MICROPHONE_H
#define MICROPHONE_H

#include <stddef.h>
#include <stdint.h>

// Called from DMA IRQ context when a half-buffer is fully filled.
// Keep the callback lightweight and non-blocking.
typedef void (*microphone_buffer_callback_t)(const int32_t *buffer, size_t words, uint64_t buffer_ready_us);

void microphone_set_buffer_callback(microphone_buffer_callback_t callback);
void microphone_init(void);

#endif
