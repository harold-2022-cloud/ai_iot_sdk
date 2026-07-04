//bsp_dma.h — DMA-capable 对齐分配 + cache 一致性
#pragma once

#include <stddef.h>
#include "bsp_status.h"

#ifndef BSP_DMA_ALIGN
#define BSP_DMA_ALIGN  32   /* = S3 data cache line(CONFIG_ESP32S3_DATA_CACHE_LINE_SIZE);板级可 override */
#endif

void *Bsp_Dma_Alloc(size_t size);                       /* 对齐 BSP_DMA_ALIGN,内部 DMA-capable RAM */
void *Bsp_Dma_Alloc_Aligned(size_t size, size_t align);
void  Bsp_Dma_Free(void *ptr);
Bsp_Status_t Bsp_Dma_Cache_Writeback(void *addr, size_t size);
Bsp_Status_t Bsp_Dma_Cache_Invalidate(void *addr, size_t size);
