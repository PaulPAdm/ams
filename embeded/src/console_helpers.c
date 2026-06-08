#include "console_helpers.h"

#include <stdio.h>
#include <stdlib.h>

bool console_read_line(char *out, size_t out_size)
{
    if (out == NULL || out_size == 0u)
    {
        return false;
    }

    size_t idx = 0;

    while (true)
    {
        int ch = getchar();
        if (ch < 0)
        {
            continue;
        }

        if (ch == '\r' || ch == '\n')
        {
            out[idx] = '\0';
            printf("\n");
            fflush(stdout);
            return true;
        }

        if (ch == 0x08 || ch == 0x7F)
        {
            if (idx > 0u)
            {
                idx--;
                printf("\b \b");
                fflush(stdout);
            }
            continue;
        }

        if (ch >= 32 && ch < 127 && idx + 1u < out_size)
        {
            out[idx++] = (char)ch;
            putchar((char)ch);
            fflush(stdout);
        }
    }
}

bool console_prompt_required_field(const char *label, char *out, size_t out_size)
{
    while (true)
    {
        printf("%s: ", label);
        if (!console_read_line(out, out_size))
        {
            return false;
        }
        if (out[0] != '\0')
        {
            return true;
        }
        printf("Value cannot be empty.\n");
    }
}

bool console_prompt_u32(const char *label,
                        uint32_t min_value,
                        uint32_t max_value,
                        uint32_t default_value,
                        uint32_t *out_value)
{
    if (label == NULL || out_value == NULL || min_value > max_value)
    {
        return false;
    }

    char line[24];
    while (true)
    {
        printf("%s [%lu]: ", label, (unsigned long)default_value);
        if (!console_read_line(line, sizeof(line)))
        {
            return false;
        }
        if (line[0] == '\0')
        {
            *out_value = default_value;
            return true;
        }

        char *end = NULL;
        unsigned long value = strtoul(line, &end, 10);
        if (end != line && *end == '\0' && value >= min_value && value <= max_value)
        {
            *out_value = (uint32_t)value;
            return true;
        }

        printf("Enter a value from %lu to %lu.\n", (unsigned long)min_value, (unsigned long)max_value);
    }
}

bool console_prompt_multiplier_x100(const char *label,
                                     uint16_t default_value_x100,
                                     uint16_t min_value_x100,
                                     uint16_t max_value_x100,
                                     uint16_t *out_value_x100)
{
    if (label == NULL || out_value_x100 == NULL || min_value_x100 > max_value_x100)
    {
        return false;
    }

    char line[24];
    while (true)
    {
        printf("%s [%u.%02u]: ",
               label,
               (unsigned int)(default_value_x100 / 100u),
               (unsigned int)(default_value_x100 % 100u));
        if (!console_read_line(line, sizeof(line)))
        {
            return false;
        }
        if (line[0] == '\0')
        {
            *out_value_x100 = default_value_x100;
            return true;
        }

        char *end = NULL;
        float value = strtof(line, &end);
        uint32_t multiplier_x100 = (uint32_t)((value * 100.0f) + 0.5f);
        if (end != line && *end == '\0' &&
            multiplier_x100 >= min_value_x100 &&
            multiplier_x100 <= max_value_x100)
        {
            *out_value_x100 = (uint16_t)multiplier_x100;
            return true;
        }

        printf("Enter a multiplier from %u.%02u to %u.%02u.\n",
               (unsigned int)(min_value_x100 / 100u),
               (unsigned int)(min_value_x100 % 100u),
               (unsigned int)(max_value_x100 / 100u),
               (unsigned int)(max_value_x100 % 100u));
    }
}
