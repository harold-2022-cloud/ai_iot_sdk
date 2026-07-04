#pragma once

#include <stddef.h>

void *os_malloc(size_t size);
void os_free(void *ptr);
void *os_realloc(void *ptr, size_t size);
