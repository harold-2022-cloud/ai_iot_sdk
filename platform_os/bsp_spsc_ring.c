//bsp_spsc_ring.c
#include "bsp_spsc_ring.h"
#include <string.h>

static int Bsp_Is_Pow2(uint32_t x)
{
    return (x != 0) && ((x & (x - 1)) == 0);
}

Bsp_Status_t Bsp_Spsc_Ring_Init(Bsp_Spsc_Ring_t *r, uint8_t *storage, uint32_t capacity_pow2)
{
    if (r == NULL || storage == NULL || !Bsp_Is_Pow2(capacity_pow2))
    {
        return BSP_ERR_INVALID_ARG;
    }
    r->buf = storage;
    r->capacity = capacity_pow2;
    r->mask = capacity_pow2 - 1U;
    r->head = 0;
    r->tail = 0;
    return BSP_OK;
}

uint32_t Bsp_Spsc_Ring_Fill(const Bsp_Spsc_Ring_t *r)
{
    uint32_t h = __atomic_load_n(&r->head, __ATOMIC_ACQUIRE);
    uint32_t t = __atomic_load_n(&r->tail, __ATOMIC_ACQUIRE);
    return h - t;   /* 无符号差,自由运行计数下安全(capacity ≤ 2^31) */
}

uint32_t Bsp_Spsc_Ring_Push(Bsp_Spsc_Ring_t *r, const uint8_t *data, uint32_t len)
{
    uint32_t h = __atomic_load_n(&r->head, __ATOMIC_RELAXED);  /* producer 独占 head */
    uint32_t t = __atomic_load_n(&r->tail, __ATOMIC_ACQUIRE);  /* 观察 consumer 进度 */
    uint32_t free_space = r->capacity - (h - t);
    if (len > free_space)
    {
        len = free_space;
    }
    /* 分两段 memcpy:先写到 buffer 尾的连续空间,跨界则回绕写余量 */
    uint32_t off = h & r->mask;
    uint32_t first = r->capacity - off;
    if (first > len)
    {
        first = len;
    }
    memcpy(&r->buf[off], data, first);
    if (len > first)
    {
        memcpy(&r->buf[0], data + first, len - first);
    }
    __atomic_store_n(&r->head, h + len, __ATOMIC_RELEASE);     /* 发布数据 */
    return len;
}

uint32_t Bsp_Spsc_Ring_Pop(Bsp_Spsc_Ring_t *r, uint8_t *out, uint32_t len)
{
    uint32_t t = __atomic_load_n(&r->tail, __ATOMIC_RELAXED);  /* consumer 独占 tail */
    uint32_t h = __atomic_load_n(&r->head, __ATOMIC_ACQUIRE);  /* 观察 producer 进度 */
    uint32_t avail = h - t;
    if (len > avail)
    {
        len = avail;
    }
    /* 分两段 memcpy:先读 buffer 尾的连续数据,跨界则回绕读余量 */
    uint32_t off = t & r->mask;
    uint32_t first = r->capacity - off;
    if (first > len)
    {
        first = len;
    }
    memcpy(out, &r->buf[off], first);
    if (len > first)
    {
        memcpy(out + first, &r->buf[0], len - first);
    }
    __atomic_store_n(&r->tail, t + len, __ATOMIC_RELEASE);     /* 发布空间 */
    return len;
}
