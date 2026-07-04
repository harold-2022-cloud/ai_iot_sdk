// bsp_dma_bk7258.c — BK7258 chip backend：DMA-capable 對齊分配 + cache 一致性。
//
// 範圍（Task 2.3）：
//   - Bsp_Dma_Alloc_Aligned(size, align)：可恢復釋放的手動對齊分配（recoverable-aligned-malloc）
//   - Bsp_Dma_Alloc(size)：BSP_DMA_ALIGN 對齊的便捷包裝
//   - Bsp_Dma_Free(ptr)：從對齊指標恢復原始基底並釋放
//   - Bsp_Dma_Cache_Writeback：no-op（見 PHASE-8R 警告）
//   - Bsp_Dma_Cache_Invalidate：no-op（見 PHASE-8R 警告）
//
// 設計決策：DMA 分配器
//   BK7258 rino 參考（common_components/chip_bk7258/）未暴露 DMA 專用對齊分配 API。
//   控制面（Smoke/control-plane）不做大緩衝 DMA，故安全使用一般堆並手動對齊：
//   1. 以 os_malloc 過度分配 (size + align + sizeof(void*)) 字節
//   2. 在返回給呼叫端的對齊指標「緊前方」藏入原始基底指標（1 個 void* 寬度）
//   3. Bsp_Dma_Free 從 ptr 往前取出 raw base 並 os_free
//   此方案保證對任意 align 都能恢復原始配置、不洩漏記憶體。
//   配置器使用 os_malloc / os_free（非 header-set 的 Bsp_Mem_*），
//   遵循 bsp_queue_wrap_t / bsp_timer_wrap_t 慣例（內部元資料用原生堆）。
//
// PHASE-8R 警告（cache 一致性）：
//   BK7258 DMA cache-coherency NOT handled here — rino 在此層未暴露 BK cache 維護 API。
//   控制面不做 DMA，目前安全。
//   PHASE-8R：音頻/大緩衝 DMA 路徑必須確認 BK7258 是否需要顯式 cache writeback/invalidate；
//   若需要，請以 BK cache API 實作本兩個函式，否則將引發資料損毀。
//
// 簽名以 platform_os/bsp_dma.h 為唯一真源，逐一對齊。

#include "bsp_dma.h"    /* 含 bsp_status.h（Bsp_Status_t, BSP_OK）、BSP_DMA_ALIGN 定義 */
#include <stddef.h>
#include <stdint.h>

/* os_malloc / os_free 宣告於 os/mem.h（非 os/os.h，後者只有 rtos_*）。 */
#include "os/mem.h"

// ─────────────────────────────────────────────────────────────────────────────
// @name    Bsp_Dma_Alloc_Aligned
// @brief   過度分配堆空間並手動對齊，將原始基底藏於返回指標緊前方以供 Free 恢復。
// @param   size   請求的有效區塊大小（bytes）
// @param   align  對齊要求（bytes，必須為 2 的冪；最小取 sizeof(void*)）
// @retval  對齊後的可用指標；失敗返回 NULL
// ─────────────────────────────────────────────────────────────────────────────
void *Bsp_Dma_Alloc_Aligned(size_t size, size_t align)
{
    /* align 最小需能容納藏入的 void* 指標本身，確保指標地址合法 */
    if (align < sizeof(void *))
    {
        align = sizeof(void *);
    }

    /* 過度分配：原始區塊 = 1 個 void*（存 raw base）+ (align - 1) 填充 + size */
    size_t overhead = sizeof(void *) + (align - 1u);
    if (size > (size_t)(SIZE_MAX - overhead))
    {
        /* 溢位防護 */
        return NULL;
    }

    void *raw = os_malloc(size + overhead);
    if (raw == NULL)
    {
        return NULL;
    }

    /* 計算對齊後的使用者指標：先跳過 1 個 void* 的空間，再向上對齊 */
    uintptr_t raw_addr    = (uintptr_t)raw;
    uintptr_t skip_addr   = raw_addr + (uintptr_t)sizeof(void *);
    uintptr_t aligned_addr = (skip_addr + (uintptr_t)(align - 1u)) & ~(uintptr_t)(align - 1u);

    /* 在使用者指標緊前方（向低 1 個 void*）藏入原始基底指標，供 Free 恢復 */
    void **back_ptr = (void **)(aligned_addr - sizeof(void *));
    *back_ptr = raw;

    return (void *)aligned_addr;
}

// ─────────────────────────────────────────────────────────────────────────────
// @name    Bsp_Dma_Alloc
// @brief   以 BSP_DMA_ALIGN 對齊的 DMA 分配（呼叫 Bsp_Dma_Alloc_Aligned）。
// @param   size  請求的有效區塊大小（bytes）
// @retval  對齊後的可用指標；失敗返回 NULL
// ─────────────────────────────────────────────────────────────────────────────
void *Bsp_Dma_Alloc(size_t size)
{
    return Bsp_Dma_Alloc_Aligned(size, (size_t)BSP_DMA_ALIGN);
}

// ─────────────────────────────────────────────────────────────────────────────
// @name    Bsp_Dma_Free
// @brief   從對齊指標恢復原始基底並釋放。必須與 Bsp_Dma_Alloc/_Aligned 配對使用。
// @param   ptr  Bsp_Dma_Alloc / Bsp_Dma_Alloc_Aligned 回傳的對齊指標（允許 NULL）
// ─────────────────────────────────────────────────────────────────────────────
void Bsp_Dma_Free(void *ptr)
{
    if (ptr == NULL)
    {
        return;
    }
    /* 從使用者指標緊前方（向低 1 個 void*）讀取原始基底並釋放 */
    void **back_ptr = (void **)((uintptr_t)ptr - sizeof(void *));
    void  *raw      = *back_ptr;
    os_free(raw);
}

// ─────────────────────────────────────────────────────────────────────────────
// @name    Bsp_Dma_Cache_Writeback
// @brief   【no-op】強制 CPU cache 回寫至記憶體。
//          BK7258 DMA cache-coherency NOT handled here — rino 在此層未暴露 BK cache 維護 API。
//          控制面（Smoke/control-plane）不做 DMA，目前安全。
//          PHASE-8R WARNING: 音頻/大緩衝 DMA 路徑必須確認 BK7258 是否需要顯式 cache writeback；
//          若需要，請以 BK cache API（如 bk_dcache_clean_range 或同等 API）實作此函式，
//          否則將引發 DMA 資料損毀（CPU 寫入的資料仍在 cache 未刷出，DMA 讀到舊值）。
// @param   addr  緩衝區起始地址（此 no-op 實作忽略）
// @param   size  緩衝區大小（此 no-op 實作忽略）
// @retval  BSP_OK（固定）
// ─────────────────────────────────────────────────────────────────────────────
Bsp_Status_t Bsp_Dma_Cache_Writeback(void *addr, size_t size)
{
    (void)addr;
    (void)size;
    /* BK7258 DMA cache-coherency NOT handled here — rino 無 BK cache 維護 API 暴露。
     * PHASE-8R WARNING: 音頻/大緩衝 DMA 路徑需驗證並補全；如有 BK cache API 請替換此 no-op。 */
    return BSP_OK;
}

// ─────────────────────────────────────────────────────────────────────────────
// @name    Bsp_Dma_Cache_Invalidate
// @brief   【no-op】使 CPU cache 無效，強制後續讀取從記憶體取得 DMA 寫入的新資料。
//          BK7258 DMA cache-coherency NOT handled here — rino 在此層未暴露 BK cache 維護 API。
//          控制面（Smoke/control-plane）不做 DMA，目前安全。
//          PHASE-8R WARNING: 音頻/大緩衝 DMA 路徑必須確認 BK7258 是否需要顯式 cache invalidate；
//          若需要，請以 BK cache API（如 bk_dcache_invalidate_range 或同等 API）實作此函式，
//          否則 CPU 將讀到 cache 中的舊資料而非 DMA 實際寫入的新資料。
// @param   addr  緩衝區起始地址（此 no-op 實作忽略）
// @param   size  緩衝區大小（此 no-op 實作忽略）
// @retval  BSP_OK（固定）
// ─────────────────────────────────────────────────────────────────────────────
Bsp_Status_t Bsp_Dma_Cache_Invalidate(void *addr, size_t size)
{
    (void)addr;
    (void)size;
    /* BK7258 DMA cache-coherency NOT handled here — rino 無 BK cache 維護 API 暴露。
     * PHASE-8R WARNING: 音頻/大緩衝 DMA 路徑需驗證並補全；如有 BK cache API 請替換此 no-op。 */
    return BSP_OK;
}
