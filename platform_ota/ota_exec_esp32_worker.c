#include "platform_ota.h"
#include "bsp_system.h"     /* Bsp_Pthread_Create, Bsp_Msg_Queue_*, Bsp_Pthread_Delete */
#include <stdlib.h>

#define OTA_EXEC_QUEUE_LEN 2   /* single serial session; 2 = head room */

typedef struct { void (*fn)(void *); void *arg; } ota_job_t;

typedef struct
{
    Bsp_Thread_t thread;
    Bsp_Queue_t  queue;
    volatile int running;
} ota_exec_ctx_t;

static void ota_exec_worker_task(void *para)
{
    ota_exec_ctx_t *c = (ota_exec_ctx_t *)para;
    while (c->running)
    {
        ota_job_t job = {0};
        uint32_t sz = 0;
        if (Bsp_Msg_Queue_Wait(&c->queue, &job, &sz, 0xFFFFFFFFu) == 0 && job.fn != NULL)
        {
            job.fn(job.arg);            /* runs on this Internal-SRAM stack */
        }
    }
    Bsp_Pthread_Delete(NULL);
}

static int ota_exec_post(void *exec_ctx, void (*fn)(void *), void *arg)
{
    ota_exec_ctx_t *c = (ota_exec_ctx_t *)exec_ctx;
    ota_job_t job = { .fn = fn, .arg = arg };
    return (Bsp_Msg_Queue_Send(&c->queue, &job, sizeof(job), 0) == 0) ? 0 : -1;
}

static void ota_exec_destroy(void *exec_ctx)
{
    ota_exec_ctx_t *c = (ota_exec_ctx_t *)exec_ctx;
    if (c == NULL) { return; }
    c->running = 0;
    if (c->queue != NULL) { Bsp_Msg_Queue_Delete(&c->queue); }
    free(c);
}

static const platform_ota_exec_ops_t s_esp32_ops = {
    .post = ota_exec_post,
    .destroy = ota_exec_destroy,
};

platform_ota_exec_t platform_ota_exec_esp32_worker(unsigned int stack_bytes, unsigned int prio)
{
    platform_ota_exec_t e = { .ops = NULL, .ctx = NULL };
    ota_exec_ctx_t *c = calloc(1, sizeof(*c));
    if (c == NULL) { return e; }
    c->running = 1;
    if (Bsp_Msg_Queue_Create(&c->queue, OTA_EXEC_QUEUE_LEN, sizeof(ota_job_t)) != 0) { free(c); return e; }
    if (Bsp_Pthread_Create(&c->thread, "ota worker", stack_bytes, prio, ota_exec_worker_task, c) != 0)
    {
        Bsp_Msg_Queue_Delete(&c->queue); free(c); return e;
    }
    e.ops = &s_esp32_ops;
    e.ctx = c;
    return e;
}
