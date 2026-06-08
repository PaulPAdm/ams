#include "platform_time.h"

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

absolute_time_t get_absolute_time(void)
{
    return (absolute_time_t)esp_timer_get_time();
}

absolute_time_t make_timeout_time_ms(uint32_t delay_ms)
{
    return get_absolute_time() + ((absolute_time_t)delay_ms * 1000ull);
}

bool time_reached(absolute_time_t deadline)
{
    return (int64_t)(get_absolute_time() - deadline) >= 0;
}

int64_t absolute_time_diff_us(absolute_time_t from, absolute_time_t to)
{
    return (int64_t)(to - from);
}

void sleep_ms(uint32_t delay_ms)
{
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
}
