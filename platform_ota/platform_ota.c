#include "platform_ota.h"
#include "bsp_system.h"      /* Bsp_Sema_t, Bsp_Semaphore_* */
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* OTA chunk 缓冲分配: 中立 PSRAM-prefer 入口 */
#if defined(ESP_PLATFORM)
#include "esp_heap_caps.h"
static inline void *_neutral_big_malloc(size_t len) {
    void *p = heap_caps_malloc(len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (p == NULL) { p = heap_caps_malloc(len, MALLOC_CAP_8BIT); }
    return p;
}
static inline void _neutral_big_free(void *p) { heap_caps_free(p); }
#else
static inline void *_neutral_big_malloc(size_t len) { return malloc(len); }
static inline void _neutral_big_free(void *p) { free(p); }
#endif

#define OTA_FOREVER 0xFFFFFFFFu

typedef enum { SLOT_FREE = 0, SLOT_PENDING, SLOT_DONE, SLOT_ABANDONED } ota_slot_state_e;
typedef enum { OP_BEGIN = 0, OP_WRITE, OP_COMMIT, OP_ABORT } ota_op_e;

struct platform_ota
{
    const platform_ota_backend_ops_t *backend;
    void                             *backend_ctx;
    platform_ota_exec_t               exec;
    Bsp_Sema_t                        lock;       /* mutex via binary sem */
    Bsp_Sema_t                        done;       /* completion sem */
    ota_slot_state_e                  state;
    bool                              has_waiter;
    ota_op_e                          op;
    void                             *payload;    /* copied write chunk (PSRAM-pref) */
    size_t                            payload_len;
    uint32_t                          total;
    int                               result;
};

static void ota_lock(platform_ota_t *o)   { Bsp_Get_Semaphore(&o->lock, OTA_FOREVER); }
static void ota_unlock(platform_ota_t *o) { Bsp_Set_Semaphore(&o->lock); }

static void *ota_buf_alloc(size_t len)
{
    return _neutral_big_malloc(len);
}

static void sem_drain(Bsp_Sema_t *sem)
{
    while (Bsp_Get_Semaphore(sem, 0) == 0) { }
}

/* must hold lock */
static void ota_slot_release_locked(platform_ota_t *o)
{
    if (o->payload != NULL) { _neutral_big_free(o->payload); o->payload = NULL; }
    o->payload_len = 0;
    sem_drain(&o->done);
    o->has_waiter = false;
    o->state = SLOT_FREE;
}

static void ota_run_op(void *arg)
{
    platform_ota_t *o = (platform_ota_t *)arg;
    int rc;
    switch (o->op)
    {
        case OP_BEGIN:  rc = o->backend->begin(o->backend_ctx); break;
        case OP_WRITE:  rc = o->backend->write(o->backend_ctx, o->payload, o->payload_len, o->total); break;
        case OP_COMMIT: rc = o->backend->commit(o->backend_ctx); break;
        case OP_ABORT:  rc = o->backend->abort(o->backend_ctx); break;
        default:        rc = PLATFORM_OTA_ERR_INVALID_ARG; break;
    }

    ota_lock(o);
    if (o->state == SLOT_ABANDONED || !o->has_waiter)
    {
        ota_slot_release_locked(o);
        ota_unlock(o);
        return;
    }
    o->result = rc;
    o->state = SLOT_DONE;
    ota_unlock(o);
    Bsp_Set_Semaphore(&o->done);
}

static int ota_submit(platform_ota_t *o, ota_op_e op, const void *data, size_t len,
                      uint32_t total, uint32_t timeout_ms)
{
    if (o == NULL) { return PLATFORM_OTA_ERR_INVALID_ARG; }

    ota_lock(o);
    if (o->state != SLOT_FREE) { ota_unlock(o); return PLATFORM_OTA_ERR_BUSY; }
    o->state = SLOT_PENDING;
    o->has_waiter = true;
    o->op = op;
    o->total = total;
    o->payload = NULL;
    o->payload_len = 0;
    o->result = PLATFORM_OTA_ERR_BACKEND;

    if (op == OP_WRITE && len > 0)
    {
        o->payload = ota_buf_alloc(len);
        if (o->payload == NULL) { ota_slot_release_locked(o); ota_unlock(o); return PLATFORM_OTA_ERR_NO_MEM; }
        memcpy(o->payload, data, len);
        o->payload_len = len;
    }
    ota_unlock(o);

    if (o->exec.ops->post(o->exec.ctx, ota_run_op, o) != 0)
    {
        ota_lock(o);
        ota_slot_release_locked(o);
        ota_unlock(o);
        return PLATFORM_OTA_ERR_BUSY;
    }

    Bsp_Get_Semaphore(&o->done, timeout_ms);

    ota_lock(o);
    if (o->state == SLOT_DONE)
    {
        int rc = o->result;
        ota_slot_release_locked(o);
        ota_unlock(o);
        return rc;
    }
    o->state = SLOT_ABANDONED;
    ota_unlock(o);
    return PLATFORM_OTA_ERR_TIMEOUT;
}

int platform_ota_begin(const platform_ota_config_t *cfg, platform_ota_t **out_ota)
{
    if (cfg == NULL || out_ota == NULL || cfg->backend_ops == NULL || cfg->exec.ops == NULL)
    {
        return PLATFORM_OTA_ERR_INVALID_ARG;
    }
    platform_ota_t *o = calloc(1, sizeof(*o));
    if (o == NULL) { return PLATFORM_OTA_ERR_NO_MEM; }
    o->backend = cfg->backend_ops;
    o->backend_ctx = cfg->backend_ctx;
    o->exec = cfg->exec;
    o->state = SLOT_FREE;
    if (Bsp_Semaphore_Init(&o->lock, 1) != 0) { free(o); return PLATFORM_OTA_ERR_NO_MEM; }
    Bsp_Set_Semaphore(&o->lock);
    if (Bsp_Semaphore_Init(&o->done, 1) != 0) { Bsp_Semaphore_Deinit(&o->lock); free(o); return PLATFORM_OTA_ERR_NO_MEM; }

    *out_ota = o;
    int rc = ota_submit(o, OP_BEGIN, NULL, 0, 0, OTA_FOREVER);
    if (rc != PLATFORM_OTA_OK)
    {
        platform_ota_abort(o);
        *out_ota = NULL;
    }
    return rc;
}

int platform_ota_write(platform_ota_t *ota, const void *data, size_t len, uint32_t total, uint32_t timeout_ms)
{
    if (len > 0 && data == NULL) { return PLATFORM_OTA_ERR_INVALID_ARG; }
    return ota_submit(ota, OP_WRITE, data, len, total, timeout_ms);
}

int platform_ota_commit(platform_ota_t *ota, uint32_t timeout_ms)
{
    return ota_submit(ota, OP_COMMIT, NULL, 0, 0, timeout_ms);
}

void platform_ota_abort(platform_ota_t *ota)
{
    if (ota == NULL) { return; }
    /* best-effort backend abort on the exec context (ignore result) */
    (void)ota_submit(ota, OP_ABORT, NULL, 0, 0, OTA_FOREVER);
    if (ota->exec.ops != NULL && ota->exec.ops->destroy != NULL)
    {
        ota->exec.ops->destroy(ota->exec.ctx);
    }
    Bsp_Semaphore_Deinit(&ota->done);
    Bsp_Semaphore_Deinit(&ota->lock);
    if (ota->payload != NULL) { _neutral_big_free(ota->payload); }
    free(ota);
}
