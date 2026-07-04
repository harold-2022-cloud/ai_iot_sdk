#include "platform_storage.h"

#include <stdint.h>
#include <string.h>

#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_memory_utils.h"
#include "nvs.h"
#include "nvs_flash.h"

#ifndef NVS_KEY_NAME_MAX_SIZE
#define NVS_KEY_NAME_MAX_SIZE 16
#endif

static bool platform_storage_nvs_key_valid(const char *key)
{
    if (key == NULL) {
        return false;
    }

    size_t key_len = strnlen(key, NVS_KEY_NAME_MAX_SIZE);
    return (key_len > 0U) && (key_len < NVS_KEY_NAME_MAX_SIZE);
}

static const char *platform_storage_nvs_namespace(const platform_storage_esp32_nvs_config_t *config)
{
    if ((config == NULL) || (config->namespace_name == NULL) || (config->namespace_name[0] == '\0')) {
        return PLATFORM_STORAGE_ESP32_NVS_DEFAULT_NAMESPACE;
    }
    return config->namespace_name;
}

static int platform_storage_nvs_init(void *ctx)
{
    const platform_storage_esp32_nvs_config_t *config = (const platform_storage_esp32_nvs_config_t *)ctx;
    if ((config != NULL) && !config->init_flash) {
        return PLATFORM_STORAGE_OK;
    }

    esp_err_t ret = nvs_flash_init();
    if ((ret == ESP_ERR_NVS_NO_FREE_PAGES) || (ret == ESP_ERR_NVS_NEW_VERSION_FOUND)) {
        ret = nvs_flash_erase();
        if (ret != ESP_OK) {
            return PLATFORM_STORAGE_ERR_BACKEND;
        }
        ret = nvs_flash_init();
    }

    return (ret == ESP_OK) ? PLATFORM_STORAGE_OK : PLATFORM_STORAGE_ERR_BACKEND;
}

static int platform_storage_nvs_read(void *ctx, const char *key, void *buf, size_t buf_len, size_t *out_len)
{
    if (!platform_storage_nvs_key_valid(key) || (out_len == NULL) || ((buf_len > 0U) && (buf == NULL))) {
        return PLATFORM_STORAGE_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    const char *namespace_name = platform_storage_nvs_namespace((const platform_storage_esp32_nvs_config_t *)ctx);
    esp_err_t ret = nvs_open(namespace_name, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        return PLATFORM_STORAGE_ERR_BACKEND;
    }

    size_t read_len = 0;
    ret = nvs_get_blob(handle, key, NULL, &read_len);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(handle);
        *out_len = 0;
        return PLATFORM_STORAGE_ERR_NOT_FOUND;
    }
    if (ret != ESP_OK) {
        nvs_close(handle);
        *out_len = 0;
        return PLATFORM_STORAGE_ERR_BACKEND;
    }
    if (read_len > buf_len) {
        nvs_close(handle);
        *out_len = read_len;
        return PLATFORM_STORAGE_ERR_BUFFER_TOO_SMALL;
    }
    if (read_len == 0U) {
        nvs_close(handle);
        *out_len = 0;
        return PLATFORM_STORAGE_OK;
    }

    /* nvs_get_blob 在 cache 开启时拷贝数据,无需把 payload 强塞进连续内部 SRAM
     * (cache-disable 只影响任务栈,栈安全由调用侧 worker 的内部栈保证)。
     * 直接读入调用方缓冲,避免大 blob 的大块连续内部分配在碎片化下失败。 */
    ret = nvs_get_blob(handle, key, buf, &read_len);
    nvs_close(handle);

    if (ret == ESP_OK) {
        *out_len = read_len;
        return PLATFORM_STORAGE_OK;
    }

    *out_len = 0;
    return (ret == ESP_ERR_NVS_INVALID_LENGTH) ? PLATFORM_STORAGE_ERR_BUFFER_TOO_SMALL : PLATFORM_STORAGE_ERR_BACKEND;
}

static int platform_storage_nvs_write(void *ctx, const char *key, const void *data, size_t len)
{
    if (!platform_storage_nvs_key_valid(key) || ((len > 0U) && (data == NULL))) {
        return PLATFORM_STORAGE_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    const char *namespace_name = platform_storage_nvs_namespace((const platform_storage_esp32_nvs_config_t *)ctx);
    esp_err_t ret = nvs_open(namespace_name, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        return PLATFORM_STORAGE_ERR_BACKEND;
    }

    /* nvs_set_blob 在 cache 开启时把 payload 拷入 NVS 内部页,commit 前已自有副本;
     * payload 在 PSRAM 安全,无需强塞连续内部 SRAM(栈安全由 worker 内部栈保证)。
     * 直接传入,避免大 blob 的大块连续内部分配在碎片化下失败。 */
    const uint8_t empty = 0;
    const void *write_data = (len > 0U) ? data : (const void *)&empty;

    ret = nvs_set_blob(handle, key, write_data, len);
    if (ret == ESP_OK) {
        ret = nvs_commit(handle);
    }
    nvs_close(handle);

    return (ret == ESP_OK) ? PLATFORM_STORAGE_OK : PLATFORM_STORAGE_ERR_BACKEND;
}

static int platform_storage_nvs_erase(void *ctx, const char *key)
{
    if (!platform_storage_nvs_key_valid(key)) {
        return PLATFORM_STORAGE_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    const char *namespace_name = platform_storage_nvs_namespace((const platform_storage_esp32_nvs_config_t *)ctx);
    esp_err_t ret = nvs_open(namespace_name, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        return PLATFORM_STORAGE_ERR_BACKEND;
    }

    ret = nvs_erase_key(handle, key);
    if (ret == ESP_OK) {
        ret = nvs_commit(handle);
    }
    nvs_close(handle);

    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        return PLATFORM_STORAGE_ERR_NOT_FOUND;
    }
    return (ret == ESP_OK) ? PLATFORM_STORAGE_OK : PLATFORM_STORAGE_ERR_BACKEND;
}

static const platform_storage_ops_t s_platform_storage_nvs_ops = {
    .init = platform_storage_nvs_init,
    .read = platform_storage_nvs_read,
    .write = platform_storage_nvs_write,
    .erase = platform_storage_nvs_erase,
    .deinit = NULL,
};

platform_storage_esp32_nvs_config_t platform_storage_esp32_nvs_default_config(void)
{
    platform_storage_esp32_nvs_config_t config = {
        .namespace_name = PLATFORM_STORAGE_ESP32_NVS_DEFAULT_NAMESPACE,
        .init_flash = true,
    };
    return config;
}

const platform_storage_ops_t *platform_storage_esp32_nvs_ops(void)
{
    return &s_platform_storage_nvs_ops;
}
