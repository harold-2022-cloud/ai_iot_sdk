#ifndef PLATFORM_OTA_H
#define PLATFORM_OTA_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    PLATFORM_OTA_OK = 0,
    PLATFORM_OTA_ERR_INVALID_ARG = -1,
    PLATFORM_OTA_ERR_BACKEND = -2,
    PLATFORM_OTA_ERR_TIMEOUT = -3,
    PLATFORM_OTA_ERR_BUSY = -4,
    PLATFORM_OTA_ERR_NO_MEM = -5,
} platform_ota_status_e;

typedef struct platform_ota platform_ota_t;

/* backend = "what OTA does on the medium" */
typedef struct
{
    int (*begin)(void *ctx);
    int (*write)(void *ctx, const void *data, size_t len, uint32_t total);
    int (*commit)(void *ctx);
    int (*abort)(void *ctx);
} platform_ota_backend_ops_t;

/* exec = "where/how it runs" (own type — not shared with storage, decision O2) */
typedef struct
{
    int  (*post)(void *exec_ctx, void (*fn)(void *), void *arg);
    void (*destroy)(void *exec_ctx);
} platform_ota_exec_ops_t;

typedef struct
{
    const platform_ota_exec_ops_t *ops;
    void                          *ctx;
} platform_ota_exec_t;

typedef struct
{
    const platform_ota_backend_ops_t *backend_ops;
    void                             *backend_ctx;
    platform_ota_exec_t               exec;
} platform_ota_config_t;

int  platform_ota_begin (const platform_ota_config_t *cfg, platform_ota_t **out_ota);
int  platform_ota_write (platform_ota_t *ota, const void *data, size_t len,
                         uint32_t total, uint32_t timeout_ms);
int  platform_ota_commit(platform_ota_t *ota, uint32_t timeout_ms);
void platform_ota_abort (platform_ota_t *ota);   /* destroys handle + worker */

platform_ota_exec_t platform_ota_exec_sync(void);

/* ESP32 on-demand Internal-SRAM worker exec policy (transient: destroyed via abort). */
platform_ota_exec_t platform_ota_exec_esp32_worker(unsigned int stack_bytes, unsigned int prio);

#ifdef __cplusplus
}
#endif
#endif /* PLATFORM_OTA_H */
