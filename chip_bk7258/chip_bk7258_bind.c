/* BK7258 平台后端：memory snapshot + net set-option 函数。
 * 指针赋值（Entity_Get_Memory_Snapshot、Entity_Net_Set_Option 等）
 * 延至 Task 6.2 (Entity_Chip_Bk7258_Bind_Special) 进行，
 * 本文件只提供函数体。
 * 本文件是 SDK 内唯一允许直接调用 armino rtos_* heap API 的代码。 */
#include "entity_iot_func.h"
#include "entity_network/entity_network.h"  /* Entity_Net_Opt_e */
#include "lwip/sockets.h"                   /* lwip_setsockopt, SOL_SOCKET, SO_* */
#include <sys/time.h>                       /* struct timeval */
/* 注：BK7258 lwip 头路径 Phase 8 验证；若 armino 使用不同前缀需调整。 */

/**
 * @name    bk_mem_snapshot
 * @brief   填充三区内存快照（internal / dma / external(PSRAM)）。
 *          BK7258 单堆无独立 DMA 区、无 largest-free-block 查询 API，详见注释。
 * @param   o  输出快照，调用方提供；NULL 时提前返回。
 */
void bk_mem_snapshot(Entity_Memory_Snapshot_t *o)
{
    if (!o) { return; }

    uint32_t free_sz = (uint32_t)rtos_get_free_heap_size();

    /* internal_free：当前可用堆字节数 */
    o->internal_free    = free_sz;

    /* internal_largest：BK armino 未暴露 largest-free-block 查询；
     * 以 free_sz 退化填充（保守近似：实际最大连续块 ≤ free_sz）。
     * 注意：rtos_get_minimum_free_heap_size() 是历史最低水位（min-ever-free），
     * 语义不同，不可用于此字段。 */
    o->internal_largest = free_sz;

    /* dma_free / dma_largest：BK7258 DMA 与主堆同源，无独立 DMA 堆；
     * 直接镜像 internal 字段。 */
    o->dma_free         = free_sz;
    o->dma_largest      = free_sz;

#if CONFIG_PSRAM_AS_SYS_MEMORY
    /* external（PSRAM）：armino 提供 rtos_get_psram_free_heap_size()；
     * 无 psram largest-free-block 查询，同样退化为 free 值。 */
    uint32_t psram_free = (uint32_t)rtos_get_psram_free_heap_size();
    o->external_free    = psram_free;
    o->external_largest = psram_free;   /* 退化：无 psram largest-block API */
#else
    /* 无 PSRAM 或 PSRAM 未并入系统堆：填 0 */
    o->external_free    = 0;
    o->external_largest = 0;
#endif
}

/**
 * @name    bk_net_set_option
 * @brief   通过 lwip setsockopt 设置 socket 超时和接收缓冲区大小。
 *          体与 esp32 版（esp_net_set_option）一致，均为 chip-neutral lwip 调用。
 *          指针赋值 Entity_Net_Set_Option = bk_net_set_option 延至 Task 6.2。
 * @param   fd   socket 文件描述符
 * @param   opt  选项枚举（RECV_TIMEOUT_MS / SEND_TIMEOUT_MS / RECV_BUF_BYTES）
 * @param   val  选项值（超时单位：ms；缓冲区单位：bytes）
 * @retval  0 成功，-1 失败
 */
int bk_net_set_option(int fd, Entity_Net_Opt_e opt, int val)
{
    switch (opt)
    {
        case ENTITY_NET_OPT_RECV_TIMEOUT_MS:
        case ENTITY_NET_OPT_SEND_TIMEOUT_MS:
        {
            struct timeval tv = { (long)(val / 1000), (long)((val % 1000) * 1000) };
            int name = (opt == ENTITY_NET_OPT_RECV_TIMEOUT_MS) ? SO_RCVTIMEO : SO_SNDTIMEO;
            return (lwip_setsockopt(fd, SOL_SOCKET, name, &tv, sizeof tv) == 0) ? 0 : -1;
        }
        case ENTITY_NET_OPT_RECV_BUF_BYTES:
            return (lwip_setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &val, sizeof val) == 0) ? 0 : -1;
        default:
            return -1;
    }
}

/* 这两个全局指针在本文件定义（唯一 target-side 定义），由 Entity_Chip_Bk7258_Bind_Special()
   显式赋值——与 entity_hal_binding_bk7258.c 的 Entity_Chip_Bk7258_Bind() 配套（后者调用本函数）。
   注意：esp32 用 static-init（chip_esp32s3_bind.c），BK 用显式赋值。 */
Entity_Get_Memory_Snapshot_f Entity_Get_Memory_Snapshot = NULL;
Entity_Net_Set_Option_f      Entity_Net_Set_Option      = NULL;

void Entity_Chip_Bk7258_Bind_Special(void)
{
    Entity_Get_Memory_Snapshot = bk_mem_snapshot;
    Entity_Net_Set_Option      = bk_net_set_option;
}
