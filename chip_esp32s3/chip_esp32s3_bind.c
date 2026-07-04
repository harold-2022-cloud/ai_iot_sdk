/* ESP32-S3 平台后端：HAL 缝指针的唯一 target-side definition（load 时静态绑定）。
 * 本文件位于 guard 扫描范围外，是 SDK 内唯一允许直接用 esp_heap_caps.h / lwIP 的代码。 */
#include "entity_iot_func.h"
#include "entity_network.h"
#include "esp_heap_caps.h"
#include <sys/time.h>
#include <lwip/sockets.h>

/* @brief 内存快照：internal/dma/psram 的 free + largest free block */
static void esp_mem_snapshot(Entity_Memory_Snapshot_t *o)
{
    if (!o) { return; }
    o->internal_free    = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    o->internal_largest = (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
    o->dma_free         = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_DMA);
    o->dma_largest      = (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_DMA);
    o->external_free    = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    o->external_largest = (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
}

/* @brief 设 socket 选项：中立枚举 → SO_RCVTIMEO/SO_SNDTIMEO(timeval) / SO_RCVBUF(int) */
static int esp_net_set_option(int fd, Entity_Net_Opt_e opt, int val)
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

/* 唯一 target-side definition：静态初始化，load 时绑定 */
Entity_Get_Memory_Snapshot_f Entity_Get_Memory_Snapshot = esp_mem_snapshot;
Entity_Net_Set_Option_f      Entity_Net_Set_Option      = esp_net_set_option;
