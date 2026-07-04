//bsp_sync.h — SMP 临界区/自旋锁(含 ISR 变体)
#pragma once

#include <stdint.h>

/* 不透明储存:容纳 FreeRTOS portMUX_TYPE(2×uint32),header 不暴露 esp 类型 */
typedef struct
{
    volatile uint32_t opaque[2];
} Bsp_Spinlock_t;

void Bsp_Spinlock_Init(Bsp_Spinlock_t *lock);
void Bsp_Spinlock_Enter(Bsp_Spinlock_t *lock);      /* 任务上下文,SMP 安全 */
void Bsp_Spinlock_Exit(Bsp_Spinlock_t *lock);
void Bsp_Spinlock_Enter_ISR(Bsp_Spinlock_t *lock);  /* ISR 上下文 */
void Bsp_Spinlock_Exit_ISR(Bsp_Spinlock_t *lock);
