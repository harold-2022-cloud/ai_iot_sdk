#include "platform_storage.h"

#include <stdlib.h>

struct platform_storage {
    const platform_storage_ops_t *ops;
    void *ctx;
    bool initialized;
};

static bool platform_storage_ops_valid(const platform_storage_ops_t *ops)
{
    return (ops != NULL) &&
           (ops->init != NULL) &&
           (ops->read != NULL) &&
           (ops->write != NULL) &&
           (ops->erase != NULL);
}

int platform_storage_create(const platform_storage_config_t *config, platform_storage_t **out_storage)
{
    if (out_storage != NULL) {
        *out_storage = NULL;
    }

    if ((config == NULL) || (out_storage == NULL) || !platform_storage_ops_valid(config->ops)) {
        return PLATFORM_STORAGE_ERR_INVALID_ARG;
    }

    platform_storage_t *storage = (platform_storage_t *)calloc(1, sizeof(*storage));
    if (storage == NULL) {
        return PLATFORM_STORAGE_ERR_NO_MEM;
    }

    storage->ops = config->ops;
    storage->ctx = config->ctx;
    *out_storage = storage;
    return PLATFORM_STORAGE_OK;
}

int platform_storage_init(platform_storage_t *storage)
{
    if ((storage == NULL) || !platform_storage_ops_valid(storage->ops)) {
        return PLATFORM_STORAGE_ERR_INVALID_ARG;
    }

    int ret = storage->ops->init(storage->ctx);
    if (ret == PLATFORM_STORAGE_OK) {
        storage->initialized = true;
    }
    return ret;
}

int platform_storage_read(platform_storage_t *storage, const char *key, void *buf, size_t buf_len, size_t *out_len)
{
    if ((storage == NULL) || !storage->initialized || (storage->ops == NULL)) {
        return PLATFORM_STORAGE_ERR_INVALID_STATE;
    }
    return storage->ops->read(storage->ctx, key, buf, buf_len, out_len);
}

int platform_storage_write(platform_storage_t *storage, const char *key, const void *data, size_t len)
{
    if ((storage == NULL) || !storage->initialized || (storage->ops == NULL)) {
        return PLATFORM_STORAGE_ERR_INVALID_STATE;
    }
    return storage->ops->write(storage->ctx, key, data, len);
}

int platform_storage_erase(platform_storage_t *storage, const char *key)
{
    if ((storage == NULL) || !storage->initialized || (storage->ops == NULL)) {
        return PLATFORM_STORAGE_ERR_INVALID_STATE;
    }
    return storage->ops->erase(storage->ctx, key);
}

void platform_storage_destroy(platform_storage_t *storage)
{
    if (storage == NULL) {
        return;
    }

    if ((storage->ops != NULL) && (storage->ops->deinit != NULL)) {
        storage->ops->deinit(storage->ctx);
    }
    free(storage);
}
