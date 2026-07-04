#pragma once

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PLATFORM_STORAGE_ESP32_NVS_DEFAULT_NAMESPACE "iot_namespace"

typedef enum {
    PLATFORM_STORAGE_OK = 0,
    PLATFORM_STORAGE_ERR_INVALID_ARG = -1,
    PLATFORM_STORAGE_ERR_NO_MEM = -2,
    PLATFORM_STORAGE_ERR_NOT_FOUND = -3,
    PLATFORM_STORAGE_ERR_BACKEND = -4,
    PLATFORM_STORAGE_ERR_INVALID_STATE = -5,
    PLATFORM_STORAGE_ERR_BUFFER_TOO_SMALL = -6,
    PLATFORM_STORAGE_ERR_TIMEOUT = -7,
    PLATFORM_STORAGE_ERR_BUSY = -8,
} platform_storage_status_t;

typedef struct platform_storage platform_storage_t;

typedef struct {
    int (*init)(void *ctx);
    int (*read)(void *ctx, const char *key, void *buf, size_t buf_len, size_t *out_len);
    int (*write)(void *ctx, const char *key, const void *data, size_t len);
    int (*erase)(void *ctx, const char *key);
    void (*deinit)(void *ctx);
} platform_storage_ops_t;

typedef struct {
    const platform_storage_ops_t *ops;
    void *ctx;
} platform_storage_config_t;

typedef struct {
    const char *namespace_name;
    bool init_flash;
} platform_storage_esp32_nvs_config_t;

int platform_storage_create(const platform_storage_config_t *config, platform_storage_t **out_storage);
int platform_storage_init(platform_storage_t *storage);
int platform_storage_read(platform_storage_t *storage, const char *key, void *buf, size_t buf_len, size_t *out_len);
int platform_storage_write(platform_storage_t *storage, const char *key, const void *data, size_t len);
int platform_storage_erase(platform_storage_t *storage, const char *key);
void platform_storage_destroy(platform_storage_t *storage);

platform_storage_esp32_nvs_config_t platform_storage_esp32_nvs_default_config(void);
const platform_storage_ops_t *platform_storage_esp32_nvs_ops(void);

#ifdef __cplusplus
}
#endif
