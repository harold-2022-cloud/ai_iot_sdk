/**
 * @file ai_rtc_agora_aosl_psram_wrap.c
 * @brief Redirect Agora AOSL memory HAL allocations to PSRAM on ESP32S3.
 */

#include "esp_heap_caps.h"

#include <stddef.h>

void *__wrap_aosl_hal_malloc(unsigned int size)
{
    void *ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (ptr == NULL)
    {
        ptr = heap_caps_malloc(size, MALLOC_CAP_DEFAULT);
    }
    return ptr;
}

void *__wrap_aosl_hal_calloc(unsigned int count, unsigned int size)
{
    void *ptr = heap_caps_calloc(count, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (ptr == NULL)
    {
        ptr = heap_caps_calloc(count, size, MALLOC_CAP_DEFAULT);
    }
    return ptr;
}

void *__wrap_aosl_hal_realloc(void *ptr, unsigned int size)
{
    void *new_ptr = heap_caps_realloc(ptr, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (new_ptr == NULL && size > 0)
    {
        new_ptr = heap_caps_realloc(ptr, size, MALLOC_CAP_DEFAULT);
    }
    return new_ptr;
}

void __wrap_aosl_hal_free(void *ptr)
{
    heap_caps_free(ptr);
}
