#include "platform_storage_proxy.h"

static int sync_post(void *exec_ctx, void (*fn)(void *), void *arg)
{
    (void)exec_ctx;
    fn(arg);              /* run inline on the caller context */
    return 0;
}

static void sync_destroy(void *exec_ctx)
{
    (void)exec_ctx;
}

static const platform_storage_exec_ops_t s_sync_ops = {
    .post = sync_post,
    .destroy = sync_destroy,
};

platform_storage_exec_t platform_storage_exec_sync(void)
{
    platform_storage_exec_t e;
    e.ops = &s_sync_ops;
    e.ctx = NULL;
    return e;
}
