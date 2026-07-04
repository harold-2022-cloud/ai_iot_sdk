#ifndef PLATFORM_STORAGE_PROXY_H
#define PLATFORM_STORAGE_PROXY_H

#include <stddef.h>
#include <stdint.h>
#include "platform_storage.h"   /* platform_storage_ops_t, status codes */

#ifdef __cplusplus
extern "C" {
#endif

#define PLATFORM_STORAGE_PROXY_KEY_MAX 16

typedef struct platform_storage_proxy platform_storage_proxy_t;

/* Execution policy: cause fn(arg) to run on a cache-safe (Internal-SRAM stack)
 * context. exec_sync runs it inline; exec_esp32_worker dispatches to a worker. */
typedef struct
{
    int  (*post)(void *exec_ctx, void (*fn)(void *), void *arg);
    void (*destroy)(void *exec_ctx);
} platform_storage_exec_ops_t;

typedef struct
{
    const platform_storage_exec_ops_t *ops;
    void                              *ctx;
} platform_storage_exec_t;

typedef struct
{
    const platform_storage_ops_t *backend_ops;
    void                         *backend_ctx;
    platform_storage_exec_t       exec;
    unsigned int                  slot_count;   /* 0 => default 4 */
} platform_storage_proxy_config_t;

int  platform_storage_proxy_create(const platform_storage_proxy_config_t *cfg,
                                   platform_storage_proxy_t **out_proxy);
void platform_storage_proxy_destroy(platform_storage_proxy_t *proxy);

int  platform_storage_proxy_write(platform_storage_proxy_t *proxy,
                                  const char *key, const void *data, size_t len,
                                  uint32_t timeout_ms);
int  platform_storage_proxy_read(platform_storage_proxy_t *proxy,
                                 const char *key, void *buf, size_t buf_len,
                                 size_t *out_len, uint32_t timeout_ms);
int  platform_storage_proxy_erase(platform_storage_proxy_t *proxy,
                                  const char *key, uint32_t timeout_ms);

int  platform_storage_proxy_post_write(platform_storage_proxy_t *proxy,
                                       const char *key, const void *data, size_t len);
int  platform_storage_proxy_post_erase(platform_storage_proxy_t *proxy,
                                       const char *key);

/* Inline (synchronous) execution policy. ESP32: only for proven Internal-stack
 * callers. Host/Linux: always safe. */
platform_storage_exec_t platform_storage_exec_sync(void);

#ifdef __cplusplus
}
#endif
#endif /* PLATFORM_STORAGE_PROXY_H */
