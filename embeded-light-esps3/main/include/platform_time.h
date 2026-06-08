#ifndef PLATFORM_TIME_H
#define PLATFORM_TIME_H

#include <stdbool.h>
#include <stdint.h>

typedef uint64_t absolute_time_t;

absolute_time_t get_absolute_time(void);
absolute_time_t make_timeout_time_ms(uint32_t delay_ms);
bool time_reached(absolute_time_t deadline);
int64_t absolute_time_diff_us(absolute_time_t from, absolute_time_t to);
void sleep_ms(uint32_t delay_ms);

#endif
