#ifndef CONSOLE_HELPERS_H
#define CONSOLE_HELPERS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool console_read_line(char *out, size_t out_size);
bool console_prompt_required_field(const char *label, char *out, size_t out_size);
bool console_prompt_u32(const char *label,
                        uint32_t min_value,
                        uint32_t max_value,
                        uint32_t default_value,
                        uint32_t *out_value);
bool console_prompt_multiplier_x100(const char *label,
                                     uint16_t default_value_x100,
                                     uint16_t min_value_x100,
                                     uint16_t max_value_x100,
                                     uint16_t *out_value_x100);

#endif
