//bsp_dma.c
#include "bsp_dma.h"
#include "esp_heap_caps.h"

void *Bsp_Dma_Alloc_Aligned(size_t size, size_t align)
{
    if (size == 0)
    {
        return NULL;
    }
    return heap_caps_aligned_alloc(align, size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
}

void *Bsp_Dma_Alloc(size_t size)
{
    return Bsp_Dma_Alloc_Aligned(size, BSP_DMA_ALIGN);
}

void Bsp_Dma_Free(void *ptr)
{
    if (ptr != NULL)
    {
        heap_caps_free(ptr);
    }
}

/* 内部 DMA-capable SRAM 对 DMA 是 cache-coherent,故 cache 操作为 no-op。
 * 若未来改用 PSRAM-backed DMA buffer,在此改用 esp_cache_msync()(需 REQUIRES esp_mm)。*/
Bsp_Status_t Bsp_Dma_Cache_Writeback(void *addr, size_t size)
{
    (void)addr;
    (void)size;
    return BSP_OK;
}

Bsp_Status_t Bsp_Dma_Cache_Invalidate(void *addr, size_t size)
{
    (void)addr;
    (void)size;
    return BSP_OK;
}
