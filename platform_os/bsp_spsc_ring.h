//bsp_spsc_ring.h — 无锁单producer单consumer环(ISR↔task)
#pragma once

#include <stddef.h>
#include <stdint.h>
#include "bsp_status.h"

typedef struct
{
    uint8_t *buf;
    uint32_t capacity;        /* 必须 2 的幂 */
    uint32_t mask;            /* capacity - 1 */
    volatile uint32_t head;   /* 仅 producer 写(自由运行计数) */
    volatile uint32_t tail;   /* 仅 consumer 写 */
} Bsp_Spsc_Ring_t;

Bsp_Status_t Bsp_Spsc_Ring_Init(Bsp_Spsc_Ring_t *r, uint8_t *storage, uint32_t capacity_pow2);
uint32_t Bsp_Spsc_Ring_Fill(const Bsp_Spsc_Ring_t *r);
static inline uint32_t Bsp_Spsc_Ring_Free(const Bsp_Spsc_Ring_t *r)
{
    if (r == NULL)
    {
        return 0;
    }
    uint32_t fill = Bsp_Spsc_Ring_Fill(r);
    return (fill <= r->capacity) ? (r->capacity - fill) : 0;
}
uint32_t Bsp_Spsc_Ring_Push(Bsp_Spsc_Ring_t *r, const uint8_t *data, uint32_t len); /* 仅 producer 调 */
uint32_t Bsp_Spsc_Ring_Pop(Bsp_Spsc_Ring_t *r, uint8_t *out, uint32_t len);         /* 仅 consumer 调 */
