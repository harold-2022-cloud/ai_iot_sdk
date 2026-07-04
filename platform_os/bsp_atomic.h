//bsp_atomic.h — 原子操作 + 内存屏障(SMP)
#pragma once

#include <stdbool.h>

typedef struct
{
    volatile int v;
} Bsp_Atomic_t;

static inline int Bsp_Atomic_Load(const Bsp_Atomic_t *a)
{
    return __atomic_load_n(&a->v, __ATOMIC_ACQUIRE);
}

static inline void Bsp_Atomic_Store(Bsp_Atomic_t *a, int val)
{
    __atomic_store_n(&a->v, val, __ATOMIC_RELEASE);
}

static inline int Bsp_Atomic_Add(Bsp_Atomic_t *a, int val)
{
    return __atomic_add_fetch(&a->v, val, __ATOMIC_ACQ_REL);
}

static inline int Bsp_Atomic_Sub(Bsp_Atomic_t *a, int val)
{
    return __atomic_sub_fetch(&a->v, val, __ATOMIC_ACQ_REL);
}

static inline int Bsp_Atomic_Cas(Bsp_Atomic_t *a, int expected, int desired)
{
    return __atomic_compare_exchange_n(&a->v, &expected, desired, false,
                                       __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE) ? 1 : 0;
}

static inline void Bsp_Memory_Barrier(void)
{
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
}
