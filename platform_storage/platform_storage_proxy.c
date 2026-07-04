#include "platform_storage_proxy.h"
#include "platform_storage.h"
#include "bsp_system.h"      /* Bsp_Sema_t, Bsp_Semaphore_* */
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* payload/scratch 缓冲分配: 中立 PSRAM-prefer 入口 */
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

#define PROXY_DEFAULT_SLOTS 4
#define PROXY_FOREVER       0xFFFFFFFFu

typedef enum { SLOT_FREE = 0, SLOT_PENDING, SLOT_DONE, SLOT_ABANDONED } slot_state_e;
typedef enum { OP_WRITE = 0, OP_READ, OP_ERASE } slot_op_e;

typedef struct
{
    slot_state_e state;
    slot_op_e    op;
    bool         has_waiter;
    char         key[PLATFORM_STORAGE_PROXY_KEY_MAX];
    void        *payload;       /* internal: write copy, or read scratch buffer */
    size_t       payload_len;   /* write len, or read buf_len */
    size_t       out_len;       /* read result length */
    int          result;
    Bsp_Sema_t   sem;           /* per-slot completion (binary) */
    struct platform_storage_proxy *proxy;
} proxy_slot_t;

struct platform_storage_proxy
{
    platform_storage_t     *backend;
    platform_storage_exec_t exec;
    Bsp_Sema_t              lock;     /* mutex via binary semaphore (count 1) */
    proxy_slot_t           *slots;
    unsigned int            slot_count;
};

/* payload/scratch 缓冲分配:优先 PSRAM(大 blob 不挤占连续内部 SRAM),
 * 无 PSRAM 时回退任意 8-bit RAM。NVS 在 cache 开启时访问 payload,故无需内部。 */
static void *proxy_buf_alloc(size_t len)
{
    return _neutral_big_malloc(len);
}

static void proxy_lock(platform_storage_proxy_t *p)   { Bsp_Get_Semaphore(&p->lock, PROXY_FOREVER); }
static void proxy_unlock(platform_storage_proxy_t *p) { Bsp_Set_Semaphore(&p->lock); }

/* drain a binary sem to 0 (non-blocking) */
static void sem_drain(Bsp_Sema_t *sem)
{
    while (Bsp_Get_Semaphore(sem, 0) == 0) { }
}

/* must hold lock */
static void slot_release_locked(proxy_slot_t *slot)
{
    if (slot->payload != NULL) { _neutral_big_free(slot->payload); slot->payload = NULL; }
    sem_drain(&slot->sem);            /* maintain FREE => sem == 0 */
    slot->has_waiter = false;
    slot->state = SLOT_FREE;
}

/* must hold lock; returns NULL if pool exhausted */
static proxy_slot_t *slot_acquire_locked(platform_storage_proxy_t *p)
{
    for (unsigned int i = 0; i < p->slot_count; i++)
    {
        if (p->slots[i].state == SLOT_FREE)
        {
            p->slots[i].state = SLOT_PENDING;
            return &p->slots[i];
        }
    }
    return NULL;
}

/* runs on a cache-safe context (worker thread or inline). */
static void proxy_run_op(void *arg)
{
    proxy_slot_t *slot = (proxy_slot_t *)arg;
    platform_storage_proxy_t *p = slot->proxy;
    int rc;
    size_t out_len = 0;

    switch (slot->op)
    {
        case OP_WRITE: rc = platform_storage_write(p->backend, slot->key, slot->payload, slot->payload_len); break;
        case OP_READ:  rc = platform_storage_read(p->backend, slot->key, slot->payload, slot->payload_len, &out_len); break;
        case OP_ERASE: rc = platform_storage_erase(p->backend, slot->key); break;
        default:       rc = PLATFORM_STORAGE_ERR_INVALID_ARG; break;
    }

    proxy_lock(p);
    if (slot->state == SLOT_ABANDONED || !slot->has_waiter)
    {
        slot_release_locked(slot);     /* timeout or fire-forget: worker reclaims */
        proxy_unlock(p);
        return;
    }
    slot->result = rc;
    slot->out_len = out_len;
    slot->state = SLOT_DONE;
    proxy_unlock(p);
    Bsp_Set_Semaphore(&slot->sem);     /* give outside lock */
}

static int proxy_submit(platform_storage_proxy_t *proxy, slot_op_e op,
                        const char *key, const void *data, size_t len,
                        void *read_buf, size_t read_buf_len,
                        bool wait, uint32_t timeout_ms,
                        size_t *out_len)
{
    if (proxy == NULL || key == NULL) { return PLATFORM_STORAGE_ERR_INVALID_ARG; }
    if (strlen(key) >= PLATFORM_STORAGE_PROXY_KEY_MAX) { return PLATFORM_STORAGE_ERR_INVALID_ARG; }

    proxy_lock(proxy);
    proxy_slot_t *slot = slot_acquire_locked(proxy);
    if (slot == NULL) { proxy_unlock(proxy); return PLATFORM_STORAGE_ERR_BUSY; }

    /* submit-time copy: caller buffers free-able after this returns */
    strcpy(slot->key, key);
    slot->op = op;
    slot->has_waiter = wait;
    slot->payload = NULL;
    slot->payload_len = 0;
    slot->out_len = 0;
    slot->result = PLATFORM_STORAGE_ERR_INVALID_STATE;

    size_t alloc_len = (op == OP_WRITE) ? len : ((op == OP_READ) ? read_buf_len : 0);
    if (alloc_len > 0)
    {
        slot->payload = proxy_buf_alloc(alloc_len);
        if (slot->payload == NULL) { slot_release_locked(slot); proxy_unlock(proxy); return PLATFORM_STORAGE_ERR_NO_MEM; }
        if (op == OP_WRITE) { memcpy(slot->payload, data, len); }
        slot->payload_len = alloc_len;
    }
    proxy_unlock(proxy);

    int post_rc = proxy->exec.ops->post(proxy->exec.ctx, proxy_run_op, slot);
    if (post_rc != 0)
    {
        proxy_lock(proxy);
        slot_release_locked(slot);
        proxy_unlock(proxy);
        return PLATFORM_STORAGE_ERR_BUSY;
    }

    if (!wait) { return PLATFORM_STORAGE_OK; }   /* fire-and-forget */

    uint32_t wrc = Bsp_Get_Semaphore(&slot->sem, timeout_ms);

    proxy_lock(proxy);
    if (slot->state == SLOT_DONE)
    {
        int result = slot->result;
        size_t rlen = slot->out_len;
        if (op == OP_READ && result == PLATFORM_STORAGE_OK && read_buf != NULL)
        {
            memcpy(read_buf, slot->payload, rlen);  /* copy on caller context */
            if (out_len != NULL) { *out_len = rlen; }
        }
        slot_release_locked(slot);
        proxy_unlock(proxy);
        return result;
    }
    /* not DONE => timed out; abandon, worker will reclaim */
    (void)wrc;
    slot->state = SLOT_ABANDONED;
    proxy_unlock(proxy);
    return PLATFORM_STORAGE_ERR_TIMEOUT;
}

int platform_storage_proxy_create(const platform_storage_proxy_config_t *cfg,
                                  platform_storage_proxy_t **out_proxy)
{
    if (cfg == NULL || out_proxy == NULL || cfg->backend_ops == NULL || cfg->exec.ops == NULL)
    {
        return PLATFORM_STORAGE_ERR_INVALID_ARG;
    }
    platform_storage_proxy_t *p = calloc(1, sizeof(*p));
    if (p == NULL) { return PLATFORM_STORAGE_ERR_NO_MEM; }

    p->slot_count = (cfg->slot_count == 0) ? PROXY_DEFAULT_SLOTS : cfg->slot_count;
    p->exec = cfg->exec;

    platform_storage_config_t bcfg = { .ops = cfg->backend_ops, .ctx = cfg->backend_ctx };
    int rc = platform_storage_create(&bcfg, &p->backend);
    if (rc != PLATFORM_STORAGE_OK) { free(p); return rc; }
    rc = platform_storage_init(p->backend);
    if (rc != PLATFORM_STORAGE_OK) { platform_storage_destroy(p->backend); free(p); return rc; }

    if (Bsp_Semaphore_Init(&p->lock, 1) != 0) { platform_storage_destroy(p->backend); free(p); return PLATFORM_STORAGE_ERR_NO_MEM; }
    Bsp_Set_Semaphore(&p->lock);   /* mutex starts unlocked (count 1) */

    p->slots = calloc(p->slot_count, sizeof(proxy_slot_t));
    if (p->slots == NULL) { Bsp_Semaphore_Deinit(&p->lock); platform_storage_destroy(p->backend); free(p); return PLATFORM_STORAGE_ERR_NO_MEM; }
    for (unsigned int i = 0; i < p->slot_count; i++)
    {
        p->slots[i].state = SLOT_FREE;
        p->slots[i].proxy = p;
        if (Bsp_Semaphore_Init(&p->slots[i].sem, 1) != 0) { /* best-effort cleanup */ }
    }
    *out_proxy = p;
    return PLATFORM_STORAGE_OK;
}

void platform_storage_proxy_destroy(platform_storage_proxy_t *proxy)
{
    if (proxy == NULL) { return; }
    if (proxy->exec.ops != NULL && proxy->exec.ops->destroy != NULL)
    {
        proxy->exec.ops->destroy(proxy->exec.ctx);   /* joins worker if any */
    }
    if (proxy->slots != NULL)
    {
        for (unsigned int i = 0; i < proxy->slot_count; i++)
        {
            if (proxy->slots[i].payload != NULL) { _neutral_big_free(proxy->slots[i].payload); }
            Bsp_Semaphore_Deinit(&proxy->slots[i].sem);
        }
        free(proxy->slots);
    }
    Bsp_Semaphore_Deinit(&proxy->lock);
    platform_storage_destroy(proxy->backend);
    free(proxy);
}

int platform_storage_proxy_write(platform_storage_proxy_t *proxy, const char *key,
                                 const void *data, size_t len, uint32_t timeout_ms)
{
    if (len > 0 && data == NULL) { return PLATFORM_STORAGE_ERR_INVALID_ARG; }
    return proxy_submit(proxy, OP_WRITE, key, data, len, NULL, 0, true, timeout_ms, NULL);
}

int platform_storage_proxy_read(platform_storage_proxy_t *proxy, const char *key,
                                void *buf, size_t buf_len, size_t *out_len, uint32_t timeout_ms)
{
    if (buf == NULL || buf_len == 0) { return PLATFORM_STORAGE_ERR_INVALID_ARG; }
    return proxy_submit(proxy, OP_READ, key, NULL, 0, buf, buf_len, true, timeout_ms, out_len);
}

int platform_storage_proxy_erase(platform_storage_proxy_t *proxy, const char *key, uint32_t timeout_ms)
{
    return proxy_submit(proxy, OP_ERASE, key, NULL, 0, NULL, 0, true, timeout_ms, NULL);
}

int platform_storage_proxy_post_write(platform_storage_proxy_t *proxy, const char *key,
                                      const void *data, size_t len)
{
    if (len > 0 && data == NULL) { return PLATFORM_STORAGE_ERR_INVALID_ARG; }
    return proxy_submit(proxy, OP_WRITE, key, data, len, NULL, 0, false, 0, NULL);
}

int platform_storage_proxy_post_erase(platform_storage_proxy_t *proxy, const char *key)
{
    return proxy_submit(proxy, OP_ERASE, key, NULL, 0, NULL, 0, false, 0, NULL);
}
