//bsp_isr.h — ISR 上下文助手 + IRAM 放置(Type A 单平台,直映 esp)
#pragma once

#include <stdbool.h>
#include "bsp_status.h"
#include "bsp_system.h"   /* Bsp_Sema_t */
#include "esp_attr.h"     /* IRAM_ATTR */

#define BSP_IRAM_ATTR  IRAM_ATTR

/* 契约:sema 须由 Bsp_Semaphore_Init(非 _Psram)创建(slot 持原生 handle)。
 * ISR 不得触碰 PSRAM-backed 对象(cache 关闭期访问 PSRAM 会崩)。
 * queue 的 ISR send 用既有 Bsp_Msg_Queue_Send_From_Isr(bsp_system.h)。*/
Bsp_Status_t Bsp_Sema_Give_From_ISR(Bsp_Sema_t *sema, bool *need_yield);
void         Bsp_Yield_From_ISR(bool need_yield);
