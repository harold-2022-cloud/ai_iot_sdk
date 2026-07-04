#include "platform_storage_exec_esp32.h"
#include "bsp_system.h"     /* Bsp_Pthread_Create, Bsp_Msg_Queue_*, Bsp_Pthread_Delete */
#include <stdlib.h>

typedef struct { void (*fn)(void *); void *arg; } exec_job_t;

typedef struct
{
    Bsp_Thread_t thread;
    Bsp_Queue_t  queue;
    volatile int running;
} exec_esp32_ctx_t;

static void exec_esp32_worker_task(void *para)
{
    exec_esp32_ctx_t *c = (exec_esp32_ctx_t *)para;
    while (c->running)
    {
        exec_job_t job = {0};
        uint32_t sz = 0;
        if (Bsp_Msg_Queue_Wait(&c->queue, &job, &sz, 0xFFFFFFFFu) == 0 && job.fn != NULL)
        {
            job.fn(job.arg);            /* runs on this Internal-SRAM stack */
        }
    }
    Bsp_Pthread_Delete(NULL);
}

static int exec_esp32_post(void *exec_ctx, void (*fn)(void *), void *arg)
{
    exec_esp32_ctx_t *c = (exec_esp32_ctx_t *)exec_ctx;
    exec_job_t job = { .fn = fn, .arg = arg };
    /* non-blocking send; queue_len >= slot_count guarantees space */
    return (Bsp_Msg_Queue_Send(&c->queue, &job, sizeof(job), 0) == 0) ? 0 : -1;
}

static void exec_esp32_destroy(void *exec_ctx)
{
    exec_esp32_ctx_t *c = (exec_esp32_ctx_t *)exec_ctx;
    if (c == NULL) { return; }
    c->running = 0;
    if (c->queue != NULL) { Bsp_Msg_Queue_Delete(&c->queue); }
    free(c);
}

static const platform_storage_exec_ops_t s_esp32_ops = {
    .post = exec_esp32_post,
    .destroy = exec_esp32_destroy,
};

platform_storage_exec_t platform_storage_exec_esp32_worker(unsigned int queue_len,
                                                           unsigned int stack_bytes,
                                                           unsigned int prio)
{
    platform_storage_exec_t e = { .ops = NULL, .ctx = NULL };
    exec_esp32_ctx_t *c = calloc(1, sizeof(*c));
    if (c == NULL) { return e; }
    c->running = 1;
    if (Bsp_Msg_Queue_Create(&c->queue, (uint16_t)queue_len, sizeof(exec_job_t)) != 0) { free(c); return e; }
    if (Bsp_Pthread_Create(&c->thread, "storage worker", stack_bytes, prio, exec_esp32_worker_task, c) != 0)
    {
        Bsp_Msg_Queue_Delete(&c->queue); free(c); return e;
    }
    e.ops = &s_esp32_ops;
    e.ctx = c;
    return e;
}
