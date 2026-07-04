// bsp_system_bk7258.c — BK7258 chip backend：OS HAL 記憶體 + 時間/雜項原語。
//
// 範圍（Task 1.1）：僅實作 platform_os/bsp_system.h 中的記憶體 + 時間/延時/雜項子集。
// 執行緒(Bsp_Pthread_*)、互斥鎖/信號量/隊列(Task 1.2)、定時器(Task 1.3)、
// 臨界區/自旋鎖/DMA/log(Phase2)、時區/cipher/AT/heap-debug 等其餘原語
// 由後續 Task/Phase 在同元件其它檔案實作，本檔不碰，避免符號衝突。
//
// 簽名以 platform_os/bsp_system.h 為唯一真源，逐一對齊（型別/回傳值）。
// BK API 形態撈自 rino common_components/chip_bk7258/hal_system_bk7258.c（已驗）。

#include "bsp_system.h"        /* Bsp_* 原型（唯一簽名真源） */

#include <string.h>
#include <stdlib.h>
#include <stdint.h>

/* BK / armino SDK 頭。實際路徑以 armino 工程為準；此處列出 rino 參考所用的
 * SDK 介面所在頭，Phase 8 target 編譯時若路徑不符再對齊。 */
#include "os/os.h"             /* rtos_*（delay/get_time/create_thread/mutex/sema/queue/timer） */
#include "os/mem.h"            /* os_malloc / os_free / os_realloc（非 os/os.h） */
#ifndef BSP_SYSTEM_BK7258_ALLOCATOR_ONLY
#include "timer_driver.h"      /* bk_timer_delay_us（非 driver/timer.h） */
#include "driver/aon_rtc.h"    /* bk_aon_rtc_get_ms */
#include "components/system.h" /* bk_rtc_settimeofday */
#include "sys_driver.h"        /* sys_drv_get_device_id（armino driver component 公開,裸 include） */
#include <sys/time.h>          /* struct timeval（bk_rtc_settimeofday 參數） */
#endif
#include <limits.h>

// ─── 記憶體 ────────────────────────────────────────────────────────────────────
//
// 配置器不變式（必讀）：
//   Bsp_Mem_Malloc 配出的每塊記憶體在使用者指標「之前」攜帶一個 8-byte 標頭
//   （前 4 byte 存原始 size，後 4 byte 存 ownership magic，同時保 8-byte 對齊）。
//   Bsp_Mem_Free / Bsp_Mem_Realloc 依賴此佈局 —— 只能對 Bsp_Mem_Malloc/Calloc/Realloc
//   回傳的指標呼叫；切勿把 os_malloc 等其它配置器的裸指標餵給 Bsp_Mem_Free，反之亦然。
//   若誤用，guard 會 fail-loud：log + 拒絕操作，寧可泄漏 foreign block，也不破壞 heap。
//
// 為何用標頭組而非裸 os_realloc：
//   BK 的 os_realloc 不被信任能保留舊資料（且部分 armino 版本未必提供），
//   故鏡像 rino 的 matched-set 做法：realloc = malloc-new + memcpy(min(old,new)) + free-old，
//   舊長度由標頭取得，保證保留舊內容至 min(old,new)。
//   （刻意不採 brief 範例的 "Bsp_Mem_Realloc = os_realloc" 一行式。）

#define BSP_MEM_HDR           8u
#define BSP_MEM_SIZE_OFF      0u
#define BSP_MEM_MAGIC_OFF     4u
#define BSP_MEM_MAGIC_ALIVE   0x6D656D31u  /* "mem1" */
#define BSP_MEM_MAGIC_FREED   0x6D656D30u  /* "mem0" */
#define BSP_MEM_LOGE(...)     do { os_printf(__VA_ARGS__); os_printf("\r\n"); } while (0)

static uint32_t Bsp_Mem_Block_Size_Get(const void *addr)
{
    return *(const uint32_t *)((const uint8_t *)addr - BSP_MEM_HDR + BSP_MEM_SIZE_OFF);
}

static uint32_t Bsp_Mem_Block_Magic_Get(const void *addr)
{
    return *(const uint32_t *)((const uint8_t *)addr - BSP_MEM_HDR + BSP_MEM_MAGIC_OFF);
}

static void Bsp_Mem_Block_Magic_Set(void *addr, uint32_t magic)
{
    *(uint32_t *)((uint8_t *)addr - BSP_MEM_HDR + BSP_MEM_MAGIC_OFF) = magic;
}

static int Bsp_Mem_Guard_Check(const void *addr, const char *op)
{
#ifndef BSP_MEM_GUARD_DISABLE
    uint32_t magic = Bsp_Mem_Block_Magic_Get(addr);

    if (magic == BSP_MEM_MAGIC_ALIVE)
    {
        return 0;
    }

    if (magic == BSP_MEM_MAGIC_FREED)
    {
        BSP_MEM_LOGE("[BSP_MEM] %s double-free/UAF ptr=%p", op, addr);
    }
    else
    {
        BSP_MEM_LOGE("[BSP_MEM] %s foreign/corrupt ptr=%p magic=0x%08lx (allocator misuse)",
                     op, addr, (unsigned long)magic);
    }
    return -1;
#else
    (void)addr;
    (void)op;
    return 0;
#endif
}

void *Bsp_Mem_Malloc(unsigned int size)
{
    /* 溢位防護：size + 標頭不得回繞。 */
    if (((size_t)size > (size_t)(SIZE_MAX - BSP_MEM_HDR)) ||
        ((size_t)size > (size_t)UINT32_MAX))
    {
        return NULL;
    }
    uint8_t *raw = (uint8_t *)os_malloc((size_t)size + BSP_MEM_HDR);
    if (raw == NULL)
    {
        return NULL;
    }
    *(uint32_t *)(raw + BSP_MEM_SIZE_OFF) = (uint32_t)size;
    *(uint32_t *)(raw + BSP_MEM_MAGIC_OFF) = BSP_MEM_MAGIC_ALIVE;
    return raw + BSP_MEM_HDR;
}

void Bsp_Mem_Free(void *addr)
{
    if (addr == NULL)
    {
        return;
    }
#ifndef BSP_MEM_GUARD_DISABLE
    if (Bsp_Mem_Guard_Check(addr, "free") != 0)
    {
        return;
    }
    Bsp_Mem_Block_Magic_Set(addr, BSP_MEM_MAGIC_FREED);
#endif
    os_free((uint8_t *)addr - BSP_MEM_HDR);
}

void *Bsp_Mem_Calloc(size_t num, size_t size)
{
    /* num * size 溢位防護。 */
    if (num != 0 && size > (size_t)(SIZE_MAX / num))
    {
        return NULL;
    }
    size_t total = num * size;
    if (total > (size_t)UINT32_MAX)
    {
        return NULL;
    }
    void *p = Bsp_Mem_Malloc((unsigned int)total);
    if (p != NULL)
    {
        memset(p, 0, total);
    }
    return p;
}

void *Bsp_Mem_Realloc(void *ptr, size_t size)
{
    if (ptr == NULL)
    {
        if (size > (size_t)UINT32_MAX)
        {
            return NULL;
        }
        return Bsp_Mem_Malloc((unsigned int)size);
    }
    if (size > (size_t)UINT32_MAX)
    {
        return NULL;
    }
    if (Bsp_Mem_Guard_Check(ptr, "realloc") != 0)
    {
        return NULL;
    }
    uint32_t old_size = Bsp_Mem_Block_Size_Get(ptr);
    void *np = Bsp_Mem_Malloc((unsigned int)size);
    if (np == NULL)
    {
        return NULL;   /* 原塊保持有效，符合 realloc 失敗語義 */
    }
    memcpy(np, ptr, old_size < size ? old_size : size);
    Bsp_Mem_Free(ptr);
    return np;
}

void *Bsp_Psram_Malloc(unsigned int size)
{
    return Bsp_Mem_Malloc(size);
}

void *Bsp_Psram_Zalloc(unsigned int size)
{
    return Bsp_Mem_Calloc(1, size);
}

void *Bsp_Psram_Calloc(size_t num, size_t size)
{
    return Bsp_Mem_Calloc(num, size);
}

void *Bsp_Psram_Realloc(void *ptr, size_t size)
{
    return Bsp_Mem_Realloc(ptr, size);
}

void Bsp_Psram_Free(void *addr)
{
    Bsp_Mem_Free(addr);
}

#ifndef BSP_SYSTEM_BK7258_ALLOCATOR_ONLY
// ─── 時間 / 延時 / 雜項 ─────────────────────────────────────────────────────────

void Bsp_Block_Delay_Us(uint32_t nus)
{
    bk_timer_delay_us(nus);
}

void Bsp_Block_Delay_Ms(uint32_t nus)
{
    /* 阻塞毫秒延時：以 1ms 為單位累呼叫微秒忙等，不讓出 CPU（與 _Us 同語義）。 */
    bk_timer_delay_us(nus * 1000u);
}

void Bsp_Sleep_Ms(uint32_t nms)
{
    /* 非阻塞毫秒延時：讓出 CPU 給排程器。 */
    rtos_delay_milliseconds(nms);
}

uint32_t Bsp_Get_Run_Time_Ms(void)
{
    /* 自啟動單調毫秒（RTOS tick 換算）。 */
    return rtos_get_time();
}

int64_t Bsp_Get_Run_Time_Us(void)
{
    /* 自啟動單調微秒。BK 無原生 us uptime，由 ms uptime 換算（×1000）。 */
    return (int64_t)rtos_get_time() * 1000LL;
}

uint64_t Bsp_Get_Time_Stamp_Ms(void)
{
    /* 牆鐘毫秒（AON RTC）。 */
    return bk_aon_rtc_get_ms();
}

int Bsp_Set_Time_Stamp(unsigned int stamp)
{
    /* 設定牆鐘為 unix 秒。回傳狀態（0 成功）。 */
    struct timeval tv;
    tv.tv_sec  = (time_t)stamp;
    tv.tv_usec = 0;
    bk_rtc_settimeofday(&tv, NULL);
    return 0;
}

uint64_t Bsp_Get_Time_Stamp(void)
{
    /* 牆鐘 unix 秒（由牆鐘毫秒換算）。 */
    return bk_aon_rtc_get_ms() / 1000ULL;
}

uint32_t Bsp_Random(void)
{
    return (uint32_t)rand();
}

unsigned int Bsp_Get_Device_Id(void)
{
    return (unsigned int)sys_drv_get_device_id();
}

void Bsp_Debug_Sram_Heap_Info(void)
{
    os_printf("heap total=%u free=%u min=%u peak=%u\r\n",
              (unsigned int)rtos_get_total_heap_size(),
              (unsigned int)rtos_get_free_heap_size(),
              (unsigned int)rtos_get_minimum_free_heap_size(),
              (unsigned int)(rtos_get_total_heap_size() - rtos_get_minimum_free_heap_size()));
}

void Bsp_Debug_Psram_Heap_Info(void)
{
    os_printf("psram total=%u free=%u min=%u peak=%u\r\n",
              (unsigned int)rtos_get_psram_total_heap_size(),
              (unsigned int)rtos_get_psram_free_heap_size(),
              (unsigned int)rtos_get_psram_minimum_free_heap_size(),
              (unsigned int)(rtos_get_psram_total_heap_size() - rtos_get_psram_minimum_free_heap_size()));
}

void Bsp_Debug_Heap_Info(void)
{
    Bsp_Debug_Sram_Heap_Info();
    Bsp_Debug_Psram_Heap_Info();
}

// ═══════════════════════════════════════════════════════════════════════════════
// Task 1.2：執行緒 / 互斥鎖 / 信號量 / 消息隊列 原語
// ═══════════════════════════════════════════════════════════════════════════════
//
// BK7258 是單核心，無 PSRAM。設計決策：
//   1. 所有 _Psram 變體 → 直接委派到對應基底函式（BK 無獨立 PSRAM 堆分配器）。
//   2. 所有 _Pinned_To_Core 變體 → 忽略 core 參數，委派到基底 create（單核不需綁核）。
//   3. _Ex 統一入口 → 忽略 region / core 參數，委派到對應基底函式。
//   4. 真正的邏輯體寫在基底函式；_Ex / _Psram / _Pinned 三類皆為 thin delegators。
//
// BK API 來源：
//   rtos_create_thread / rtos_delete_thread — os/os.h
//   rtos_init_mutex / rtos_deinit_mutex / rtos_lock_mutex / rtos_lock_mutex_timeout
//     / rtos_unlock_mutex — os/os.h
//   rtos_init_semaphore / rtos_deinit_semaphore / rtos_set_semaphore / rtos_get_semaphore — os/os.h
//   rtos_init_queue / rtos_deinit_queue / rtos_push_to_queue / rtos_pop_from_queue
//     / rtos_is_queue_empty — os/os.h
//   rtos_push_to_queue 在 ISR 語境中自動切換 xQueueSendToBackFromISR（見 rtos_pub.c line 487）。
//
// queue count 解決方案：
//   BK os.h 未暴露 rtos_get_queue_message_count。
//   beken_queue_t 底層是 FreeRTOS QueueHandle_t（rtos_pub.c xQueueCreate 驗證）。
//   因此 Bsp_Msg_Queue_Get_Msg_Num 直接呼叫 uxQueueMessagesWaiting(*(QueueHandle_t*)q)
//   取得真實排隊訊息數，非 stub。
//   需 #include "queue.h"（FreeRTOS）；Phase 8 armino 路徑若有偏差以實際 include path 為準。
//
// Bsp_Msg_Queue_Wait 的 msg_size OUT 參數：
//   BK rtos_pop_from_queue 為定長 pop，不回報大小。
//   隊列以固定 msg_size 建立（rtos_init_queue 第三參數）。
//   本層追蹤方式：Bsp_Msg_Queue_Create 把 msg_size 存入輕量 wrapper struct bsp_queue_wrap_t，
//   Wait 時由 wrap->item_size 填入 *msg_size，不留懸空值。
//
// ─────────────────────────────────────────────────────────────────────────────
// FreeRTOS 標頭（用於 uxQueueMessagesWaiting）
// armino 內 FreeRTOS queue.h 的實際 include path 由工程 CMake 的 include 路徑決定，
// 此處用通用路徑；Phase 8 如找不到，可改為 "freertos/queue.h" 或添加 include dir。
// FreeRTOS 鐵則：FreeRTOS.h 必須在 task.h/queue.h/list.h 之前先 include，
// 否則 list.h 報 "FreeRTOS.h must be included before list.h"
// （TickType_t / BaseType_t / PRIVILEGED_FUNCTION 等全未定義 → 大量級聯錯誤）。
#include "FreeRTOS.h"
#include "queue.h"

// ─── 輕量隊列包裝（追蹤 item_size 以解決 Wait msg_size OUT 參數問題）──────────
typedef struct
{
    beken_queue_t bk_queue;   /* 底層 BK 隊列控制代碼 */
    uint32_t      item_size;  /* 每條訊息的固定大小（bytes） */
} bsp_queue_wrap_t;

// ─────────────────────────────────────────────────────────────────────────────
// ─── 執行緒（Thread）────────────────────────────────────────────────────────
// ─────────────────────────────────────────────────────────────────────────────

// 基底實作
int Bsp_Pthread_Create(Bsp_Thread_t *thread_t, const char *name,
                       unsigned int stack_size, unsigned int priority,
                       void *cb, void *arg)
{
    int ret = rtos_create_thread((beken_thread_t *)thread_t,
                                 (uint8_t)priority, name,
                                 (beken_thread_function_t)cb,
                                 (uint32_t)stack_size, arg);
    return (ret == kNoErr) ? 0 : -1;
}

// PSRAM 棧變體：BK 無獨立 PSRAM 棧 → 委派基底
int Bsp_Pthread_Create_Psram(Bsp_Thread_t *thread_t, const char *name,
                              unsigned int stack_size, unsigned int priority,
                              void *cb, void *arg)
{
    return Bsp_Pthread_Create(thread_t, name, stack_size, priority, cb, arg);
}

// 綁核變體：BK 單核，忽略 id_num → 委派基底
int Bsp_Pthread_Create_Pinned_To_Core(Bsp_Thread_t *thread_t, const char *name,
                                      unsigned int stack_size, unsigned int priority,
                                      void *cb, void *arg, unsigned char id_num)
{
    (void)id_num;
    return Bsp_Pthread_Create(thread_t, name, stack_size, priority, cb, arg);
}

// PSRAM + 綁核變體：BK 單核無 PSRAM → 委派基底
int Bsp_Pthread_Create_Psram_Pinned_To_Core(Bsp_Thread_t *thread_t, const char *name,
                                             unsigned int stack_size, unsigned int priority,
                                             void *cb, void *arg, unsigned char id_num)
{
    (void)id_num;
    return Bsp_Pthread_Create(thread_t, name, stack_size, priority, cb, arg);
}

// _Ex 統一入口：忽略 core / region → 委派基底
int Bsp_Pthread_Create_Ex(Bsp_Thread_t *thread_t, const char *name,
                           unsigned int stack_size, unsigned int priority,
                           void *cb, void *arg,
                           int core, Bsp_Mem_Region_e region)
{
    (void)core;
    (void)region;
    return Bsp_Pthread_Create(thread_t, name, stack_size, priority, cb, arg);
}

// 刪除（基底）
int Bsp_Pthread_Delete(Bsp_Thread_t *thread_t)
{
    rtos_delete_thread((beken_thread_t *)thread_t);
    return 0;
}

// 刪除 PSRAM 執行緒：同基底
int Bsp_Pthread_Psram_Delete(Bsp_Thread_t *thread_t)
{
    return Bsp_Pthread_Delete(thread_t);
}

// ─────────────────────────────────────────────────────────────────────────────
// ─── 互斥鎖（Mutex）────────────────────────────────────────────────────────
// ─────────────────────────────────────────────────────────────────────────────
//
// 簽名：uint32_t（與 header 一致，0 = 成功，非零 = 失敗）
// Lock 特殊規則：timeout_ms == 0xFFFFFFFF → 永久等（rtos_lock_mutex no-timeout）
//                否則 → rtos_lock_mutex_timeout
//
// bsp_system.h 同時定義了 pthread_mutex 宏（Bsp_Mutex_Init / Bsp_Mutex_Destroy / ...）
// 這些宏實際展開為下列函式，名稱一對一對齊。

// 基底建立
uint32_t Bsp_Mutex_Init(Bsp_Mutex_t *mutex)
{
    return (uint32_t)rtos_init_mutex((beken_mutex_t *)mutex);
}

// PSRAM 變體：委派基底
uint32_t Bsp_Mutex_Init_Psram(Bsp_Mutex_t *mutex)
{
    return Bsp_Mutex_Init(mutex);
}

// _Ex 統一入口：忽略 region → 委派基底
uint32_t Bsp_Mutex_Init_Ex(Bsp_Mutex_t *mutex, Bsp_Mem_Region_e region)
{
    (void)region;
    return Bsp_Mutex_Init(mutex);
}

// 基底銷毀（pthread 宏：Bsp_Mutex_Destroy）
uint32_t Bsp_Mutex_Destroy(Bsp_Mutex_t *mutex)
{
    return (uint32_t)rtos_deinit_mutex((beken_mutex_t *)mutex);
}

// PSRAM 銷毀：委派基底
uint32_t Bsp_Mutex_Destroy_Psram(Bsp_Mutex_t *mutex)
{
    return Bsp_Mutex_Destroy(mutex);
}

// 基底上鎖（timeout 0xFFFFFFFF = 永遠等待）
uint32_t Bsp_Mutex_Lock(Bsp_Mutex_t *mutex, uint32_t timeout_ms)
{
    if (timeout_ms == 0xFFFFFFFFU)
    {
        return (uint32_t)rtos_lock_mutex((beken_mutex_t *)mutex);
    }
    return (uint32_t)rtos_lock_mutex_timeout((beken_mutex_t *)mutex, timeout_ms);
}

// PSRAM 上鎖：委派基底
uint32_t Bsp_Mutex_Lock_Psram(Bsp_Mutex_t *mutex, uint32_t timeout_ms)
{
    return Bsp_Mutex_Lock(mutex, timeout_ms);
}

// 基底解鎖（pthread 宏：Bsp_Mutex_Unlock）
uint32_t Bsp_Mutex_Unlock(Bsp_Mutex_t *mutex)
{
    return (uint32_t)rtos_unlock_mutex((beken_mutex_t *)mutex);
}

// PSRAM 解鎖：委派基底
uint32_t Bsp_Mutex_Unlock_Psram(Bsp_Mutex_t *mutex)
{
    return Bsp_Mutex_Unlock(mutex);
}

// ─────────────────────────────────────────────────────────────────────────────
// ─── 信號量（Semaphore）────────────────────────────────────────────────────
// ─────────────────────────────────────────────────────────────────────────────
//
// 命名鐵則（來自 header，非 brief 示例代碼）：
//   give → Bsp_Set_Semaphore     (header line 270)
//   take → Bsp_Get_Semaphore     (header line 276)
//   NOT Bsp_Semaphore_Set / Bsp_Semaphore_Get（brief 示例名稱錯誤）

// 基底建立
uint32_t Bsp_Semaphore_Init(Bsp_Sema_t *sema, int maxcount)
{
    return (uint32_t)rtos_init_semaphore((beken_semaphore_t *)sema, maxcount);
}

// PSRAM 變體：委派基底
uint32_t Bsp_Semaphore_Init_Psram(Bsp_Sema_t *sema, int maxcount)
{
    return Bsp_Semaphore_Init(sema, maxcount);
}

// _Ex 統一入口：忽略 region → 委派基底
uint32_t Bsp_Semaphore_Init_Ex(Bsp_Sema_t *sema, int maxcount, Bsp_Mem_Region_e region)
{
    (void)region;
    return Bsp_Semaphore_Init(sema, maxcount);
}

// 基底銷毀
uint32_t Bsp_Semaphore_Deinit(Bsp_Sema_t *sema)
{
    return (uint32_t)rtos_deinit_semaphore((beken_semaphore_t *)sema);
}

// PSRAM 銷毀：委派基底
uint32_t Bsp_Semaphore_Deinit_Psram(Bsp_Sema_t *sema)
{
    return Bsp_Semaphore_Deinit(sema);
}

// Give（基底）
uint32_t Bsp_Set_Semaphore(Bsp_Sema_t *sema)
{
    return (uint32_t)rtos_set_semaphore((beken_semaphore_t *)sema);
}

// Give PSRAM：委派基底
uint32_t Bsp_Set_Semaphore_Psram(Bsp_Sema_t *sema)
{
    return Bsp_Set_Semaphore(sema);
}

// Take（基底）
uint32_t Bsp_Get_Semaphore(Bsp_Sema_t *sema, uint32_t timeout_ms)
{
    return (uint32_t)rtos_get_semaphore((beken_semaphore_t *)sema, timeout_ms);
}

// Take PSRAM：委派基底
uint32_t Bsp_Get_Semaphore_Psram(Bsp_Sema_t *sema, uint32_t timeout_ms)
{
    return Bsp_Get_Semaphore(sema, timeout_ms);
}

// ─────────────────────────────────────────────────────────────────────────────
// ─── 消息隊列（Message Queue）──────────────────────────────────────────────
// ─────────────────────────────────────────────────────────────────────────────
//
// 設計：用 bsp_queue_wrap_t 包裝底層 beken_queue_t，同時記錄 item_size。
// 外部看到的 Bsp_Queue_t* 實際指向 bsp_queue_wrap_t*（malloc/free 在 Create/Delete）。
// Wait 的 msg_size OUT 參數從 wrap->item_size 填入，確保不留懸空。
//
// 注意：*Bsp_Queue 是 void* 類型，我們把它指向 bsp_queue_wrap_t。
//       呼叫端把 Bsp_Queue_t* 當不透明控制代碼（只在 Bsp_* API 之間傳遞），
//       因此 wrap 封裝對呼叫端完全透明。

// 基底建立（含 wrap malloc）
uint32_t Bsp_Msg_Queue_Create(Bsp_Queue_t *Bsp_Queue, uint16_t queue_len,
                              uint32_t msg_size)
{
    bsp_queue_wrap_t *wrap =
        (bsp_queue_wrap_t *)os_malloc(sizeof(bsp_queue_wrap_t));
    if (wrap == NULL)
    {
        return (uint32_t)kGeneralErr;
    }
    wrap->bk_queue  = NULL;
    wrap->item_size = msg_size;

    int ret = rtos_init_queue(&wrap->bk_queue, "bsp_q",
                              (uint32_t)msg_size, (uint32_t)queue_len);
    if (ret != kNoErr)
    {
        os_free(wrap);
        return (uint32_t)ret;
    }
    *Bsp_Queue = (Bsp_Queue_t)wrap;
    return (uint32_t)kNoErr;
}

// PSRAM 建立：委派基底（BK 無獨立 PSRAM 隊列）
uint32_t Bsp_Msg_Queue_Create_Psram(Bsp_Queue_t *Bsp_Queue, uint16_t queue_len,
                                    uint32_t msg_size)
{
    return Bsp_Msg_Queue_Create(Bsp_Queue, queue_len, msg_size);
}

// _Ex 統一入口：忽略 region → 委派基底
uint32_t Bsp_Msg_Queue_Create_Ex(Bsp_Queue_t *Bsp_Queue, uint16_t queue_len,
                                  uint32_t msg_size, Bsp_Mem_Region_e region)
{
    (void)region;
    return Bsp_Msg_Queue_Create(Bsp_Queue, queue_len, msg_size);
}

// 基底刪除（含 wrap free）
uint32_t Bsp_Msg_Queue_Delete(Bsp_Queue_t *Bsp_Queue)
{
    if (Bsp_Queue == NULL || *Bsp_Queue == NULL)
    {
        return (uint32_t)kGeneralErr;
    }
    bsp_queue_wrap_t *wrap = (bsp_queue_wrap_t *)*Bsp_Queue;
    int ret = rtos_deinit_queue(&wrap->bk_queue);
    os_free(wrap);
    *Bsp_Queue = NULL;
    return (uint32_t)ret;
}

// PSRAM 刪除：委派基底
uint32_t Bsp_Msg_Queue_Delete_Psram(Bsp_Queue_t *Bsp_Queue)
{
    return Bsp_Msg_Queue_Delete(Bsp_Queue);
}

// 基底發送（rtos_push_to_queue 在 ISR 語境自動切換 FromISR 版本，見 rtos_pub.c）
uint32_t Bsp_Msg_Queue_Send(Bsp_Queue_t *Bsp_Queue, void *msg,
                             uint32_t msg_size, uint32_t timeout_ms)
{
    if (Bsp_Queue == NULL || *Bsp_Queue == NULL)
    {
        return (uint32_t)kGeneralErr;
    }
    bsp_queue_wrap_t *wrap = (bsp_queue_wrap_t *)*Bsp_Queue;
    (void)msg_size;  /* 隊列為固定長度，msg_size 冗餘 */
    return (uint32_t)rtos_push_to_queue(&wrap->bk_queue, msg, timeout_ms);
}

// PSRAM 發送：委派基底
uint32_t Bsp_Msg_Queue_Send_Psram(Bsp_Queue_t *Bsp_Queue, void *msg,
                                   uint32_t msg_size, uint32_t timeout_ms)
{
    return Bsp_Msg_Queue_Send(Bsp_Queue, msg, msg_size, timeout_ms);
}

// ISR 發送：rtos_push_to_queue 已偵測 ISR 語境並使用 FromISR 版本（rtos_pub.c L487）
// 此處使用 timeout_ms = 0（不等待），符合 ISR 語境非阻塞需求。
uint32_t Bsp_Msg_Queue_Send_From_Isr(Bsp_Queue_t *Bsp_Queue, void *msg)
{
    if (Bsp_Queue == NULL || *Bsp_Queue == NULL)
    {
        return (uint32_t)kGeneralErr;
    }
    bsp_queue_wrap_t *wrap = (bsp_queue_wrap_t *)*Bsp_Queue;
    return (uint32_t)rtos_push_to_queue(&wrap->bk_queue, msg, 0);
}

// 基底等待接收（含 msg_size OUT 填充）
uint32_t Bsp_Msg_Queue_Wait(Bsp_Queue_t *Bsp_Queue, void *msg,
                             uint32_t *msg_size, uint32_t timeout_ms)
{
    if (Bsp_Queue == NULL || *Bsp_Queue == NULL)
    {
        return (uint32_t)kGeneralErr;
    }
    bsp_queue_wrap_t *wrap = (bsp_queue_wrap_t *)*Bsp_Queue;
    int ret = rtos_pop_from_queue(&wrap->bk_queue, msg, timeout_ms);
    /* 填入固定元素大小；若呼叫端傳 NULL 則跳過 */
    if (msg_size != NULL)
    {
        *msg_size = wrap->item_size;
    }
    return (uint32_t)ret;
}

// PSRAM 等待接收：委派基底
uint32_t Bsp_Msg_Queue_Wait_Psram(Bsp_Queue_t *Bsp_Queue, void *msg,
                                   uint32_t *msg_size, uint32_t timeout_ms)
{
    return Bsp_Msg_Queue_Wait(Bsp_Queue, msg, msg_size, timeout_ms);
}

// 取得隊列中訊息數量（非 stub）
// beken_queue_t 底層是 FreeRTOS QueueHandle_t（rtos_pub.c L475 xQueueCreate 驗證）。
// 直接呼叫 uxQueueMessagesWaiting 取得真實計數。
// Phase 8 注意：需確認 "queue.h" include 路徑在 armino 工程中可解析。
uint32_t Bsp_Msg_Queue_Get_Msg_Num(Bsp_Queue_t *Bsp_Queue)
{
    if (Bsp_Queue == NULL || *Bsp_Queue == NULL)
    {
        return 0u;
    }
    bsp_queue_wrap_t *wrap = (bsp_queue_wrap_t *)*Bsp_Queue;
    /* bk_queue 底層是 QueueHandle_t（xQueueCreate 回傳值） */
    return (uint32_t)uxQueueMessagesWaiting((QueueHandle_t)wrap->bk_queue);
}

// PSRAM 隊列訊息數量：委派基底
uint32_t Bsp_Msg_Queue_Get_Msg_Num_Psram(Bsp_Queue_t *Bsp_Queue)
{
    return Bsp_Msg_Queue_Get_Msg_Num(Bsp_Queue);
}

// 隊列是否為空（建構在 Get_Msg_Num 之上）
uint32_t Bsp_Msg_Queue_Is_Empty(Bsp_Queue_t *Bsp_Queue)
{
    return (Bsp_Msg_Queue_Get_Msg_Num(Bsp_Queue) == 0u) ? 1u : 0u;
}

// ─────────────────────────────────────────────────────────────────────────────
// ─── 事件組（Event Group）─────────────────────────────────────────────────
// ─────────────────────────────────────────────────────────────────────────────
// NOTE：bsp_system.h 宣告了 Bsp_Event_* 系列（Create/Delete/Send/Wait/Clear），
//       但 header 的 Bsp_Event_t 是 unsigned int（非指標），且 BK7258 目前
//       在 rino 參考中未見對應 rtos_event_group_* 包裝。
//       Task 1.2 範圍以 header 中明確歸屬「thread/mutex/sema/queue」的符號為主；
//       Bsp_Event_* 留給後續 Task（Phase 2 臨界區/事件組）或由使用端確認需求後補齊。
//       若 Phase 8 link 缺 Bsp_Event_* 符號，請在此檔追加實作。

// ═══════════════════════════════════════════════════════════════════════════════
// Task 1.3：定時器（Timer）原語
// ═══════════════════════════════════════════════════════════════════════════════
//
// 設計摘要：
//   BK7258 有兩套獨立定時器 API：
//     - 週期（periodic）：beken_timer_t + rtos_{init,start,stop,deinit,reload}_timer
//     - 單次（oneshot）： beken2_timer_t + rtos_{init,start,stop,deinit}_oneshot_timer
//                         + rtos_oneshot_reload_timer
//   呼叫端每次都傳入 is_period（unsigned char）以區分兩類。
//   使用輕量 wrapper struct bsp_timer_wrap_t 統一兩類控制代碼（void*），
//   並新增 running 旗標以支援 Bsp_Timer_Is_Running 查詢。
//
// 配置器選擇：
//   wrapper 是內部元資料（非使用者業務資料），直接用原生 os_malloc / os_free，
//   與 Task 1.2 bsp_queue_wrap_t 慣例一致；不使用 header 注入的 Bsp_Mem_* 集合。
//
// 回調型別：
//   週期：timer_handler_t  — 等效於 void (*)(void *arg)
//   單次：timer_2handler_t — 等效於 void (*)(void *arg)（rino 實測簽名一致）
//   靜態轉接函式分別提取 wrapper→cb/arg，確保對呼叫端透明。
//
// rtos_init_oneshot_timer 額外第 5 個參數：
//   rino hal_system_bk7258.c line 254 傳入 NULL，對應 BK SDK 的 context 欄位，
//   此處同樣傳 NULL。
//
// Is_Init：控制代碼（wrapper 指標）非空即已初始化。
// Is_Running：讀 wrap->running；NULL 控制代碼回傳 false。
//
// BK API 來源（rino common_components/chip_bk7258/hal_system_bk7258.c 驗證）：
//   rtos_init_timer / rtos_start_timer / rtos_stop_timer
//   rtos_deinit_timer / rtos_reload_timer
//   rtos_init_oneshot_timer / rtos_start_oneshot_timer / rtos_stop_oneshot_timer
//   rtos_deinit_oneshot_timer / rtos_oneshot_reload_timer
//
// Phase 8 注意：
//   beken_timer_t / beken2_timer_t 定義在 armino os.h 或 rtos_pub.h；
//   timer_handler_t / timer_2handler_t 函式指標型別亦同。
//   若實際 include 路徑偏差，只需調整 #include，函式體不需改動。

// ─── 定時器回調轉接（靜態，對呼叫端不可見）──────────────────────────────────

// 週期定時器 wrapper 結構（前置宣告以供回調使用）
typedef struct bsp_timer_wrap_s bsp_timer_wrap_t;

struct bsp_timer_wrap_s
{
    unsigned char is_period;   /* 1 = 週期；0 = 單次 */
    int           running;     /* 1 = 運行中；0 = 停止（Is_Running 查詢用） */
    void         *cb;          /* 呼叫端回調（實際型別 Timer_Callback_f） */
    void         *arg;         /* 回調透傳參數 */
    union
    {
        beken_timer_t  p;      /* 週期定時器底層控制代碼 */
        beken2_timer_t o;      /* 單次定時器底層控制代碼 */
    } t;
};

static void _bsp_timer_period_cb(void *arg)
{
    bsp_timer_wrap_t *w = (bsp_timer_wrap_t *)arg;
    if (w != NULL && w->cb != NULL)
    {
        ((Timer_Callback_f)w->cb)(w->arg);
    }
}

static void _bsp_timer_oneshot_cb(void *arg)
{
    bsp_timer_wrap_t *w = (bsp_timer_wrap_t *)arg;
    if (w != NULL && w->cb != NULL)
    {
        /* 單次觸發後自動轉為停止狀態 */
        w->running = 0;
        ((Timer_Callback_f)w->cb)(w->arg);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Bsp_Timer_Create
// @name  Bsp_Timer_Create
// @brief 建立週期或單次定時器，回傳不透明控制代碼。
// @param is_period  1 = 週期定時器；0 = 單次定時器
// @param interval   定時週期（毫秒）
// @param cb         觸發回調（Timer_Callback_f，以 void* 傳入）
// @param arg        回調透傳參數
// @retval void*     成功回傳 wrapper 指標；失敗回傳 NULL
// ─────────────────────────────────────────────────────────────────────────────
void *Bsp_Timer_Create(unsigned char is_period, unsigned int interval,
                       void *cb, void *arg)
{
    bsp_timer_wrap_t *w = (bsp_timer_wrap_t *)os_malloc(sizeof(bsp_timer_wrap_t));
    if (w == NULL)
    {
        return NULL;
    }
    memset(w, 0, sizeof(bsp_timer_wrap_t));
    w->is_period = is_period;
    w->running   = 0;
    w->cb        = cb;
    w->arg       = arg;

    int ret;
    if (is_period)
    {
        ret = rtos_init_timer(&w->t.p, (uint32_t)interval,
                              (timer_handler_t)_bsp_timer_period_cb, w);
    }
    else
    {
        ret = rtos_init_oneshot_timer(&w->t.o, (uint32_t)interval,
                                      (timer_2handler_t)_bsp_timer_oneshot_cb,
                                      w, NULL);
    }
    if (ret != kNoErr)
    {
        os_free(w);
        return NULL;
    }
    return (void *)w;
}

// ─────────────────────────────────────────────────────────────────────────────
// Bsp_Timer_Start
// @name  Bsp_Timer_Start
// @brief 啟動已建立的定時器，並標記 running = 1。
// @param is_period  週期/單次選擇（與建立時一致）
// @param ptimer     Bsp_Timer_Create 回傳的控制代碼
// @retval unsigned int  0（kNoErr）= 成功；非零 = 失敗
// ─────────────────────────────────────────────────────────────────────────────
unsigned int Bsp_Timer_Start(unsigned char is_period, void *ptimer)
{
    if (ptimer == NULL)
    {
        return (unsigned int)kGeneralErr;
    }
    bsp_timer_wrap_t *w = (bsp_timer_wrap_t *)ptimer;
    int ret;
    if (is_period)
    {
        ret = rtos_start_timer(&w->t.p);
    }
    else
    {
        ret = rtos_start_oneshot_timer(&w->t.o);
    }
    if (ret == kNoErr)
    {
        w->running = 1;
    }
    return (unsigned int)ret;
}

// ─────────────────────────────────────────────────────────────────────────────
// Bsp_Timer_Stop
// @name  Bsp_Timer_Stop
// @brief 停止運行中的定時器，並標記 running = 0。
// @param is_period  週期/單次選擇
// @param ptimer     控制代碼
// @retval unsigned int  0 = 成功；非零 = 失敗
// ─────────────────────────────────────────────────────────────────────────────
unsigned int Bsp_Timer_Stop(unsigned char is_period, void *ptimer)
{
    if (ptimer == NULL)
    {
        return (unsigned int)kGeneralErr;
    }
    bsp_timer_wrap_t *w = (bsp_timer_wrap_t *)ptimer;
    int ret;
    if (is_period)
    {
        ret = rtos_stop_timer(&w->t.p);
    }
    else
    {
        ret = rtos_stop_oneshot_timer(&w->t.o);
    }
    if (ret == kNoErr)
    {
        w->running = 0;
    }
    return (unsigned int)ret;
}

// ─────────────────────────────────────────────────────────────────────────────
// Bsp_Timer_Delete
// @name  Bsp_Timer_Delete
// @brief 反初始化定時器並釋放 wrapper 記憶體。
// @param is_period  週期/單次選擇
// @param ptimer     控制代碼
// @retval unsigned int  0 = 成功；非零 = 失敗
// ─────────────────────────────────────────────────────────────────────────────
unsigned int Bsp_Timer_Delete(unsigned char is_period, void *ptimer)
{
    if (ptimer == NULL)
    {
        return (unsigned int)kGeneralErr;
    }
    bsp_timer_wrap_t *w = (bsp_timer_wrap_t *)ptimer;
    int ret;
    if (is_period)
    {
        ret = rtos_deinit_timer(&w->t.p);
    }
    else
    {
        ret = rtos_deinit_oneshot_timer(&w->t.o);
    }
    os_free(w);
    return (unsigned int)ret;
}

// ─────────────────────────────────────────────────────────────────────────────
// Bsp_Timer_Is_Init
// @name  Bsp_Timer_Is_Init
// @brief 查詢定時器是否已初始化（控制代碼非空即為已初始化）。
// @param is_period  週期/單次選擇（此處忽略，僅憑控制代碼判斷）
// @param ptimer     控制代碼
// @retval bool  true = 已初始化；false = 未初始化
// ─────────────────────────────────────────────────────────────────────────────
bool Bsp_Timer_Is_Init(unsigned char is_period, void *ptimer)
{
    (void)is_period;
    return (ptimer != NULL);
}

// ─────────────────────────────────────────────────────────────────────────────
// Bsp_Timer_Is_Running
// @name  Bsp_Timer_Is_Running
// @brief 查詢定時器是否正在運行（讀 wrapper 的 running 旗標）。
// @param is_period  週期/單次選擇（此處忽略，旗標統一管理）
// @param ptimer     控制代碼
// @retval bool  true = 運行中；false = 停止或控制代碼為空
// ─────────────────────────────────────────────────────────────────────────────
bool Bsp_Timer_Is_Running(unsigned char is_period, void *ptimer)
{
    (void)is_period;
    if (ptimer == NULL)
    {
        return false;
    }
    return (((bsp_timer_wrap_t *)ptimer)->running != 0);
}

// ─────────────────────────────────────────────────────────────────────────────
// Bsp_Timer_Reload
// @name  Bsp_Timer_Reload
// @brief 重置定時器計數（週期：rtos_reload_timer；單次：rtos_oneshot_reload_timer）。
// @param is_period  週期/單次選擇
// @param ptimer     控制代碼
// @retval unsigned int  0 = 成功；非零 = 失敗
// ─────────────────────────────────────────────────────────────────────────────
unsigned int Bsp_Timer_Reload(unsigned char is_period, void *ptimer)
{
    if (ptimer == NULL)
    {
        return (unsigned int)kGeneralErr;
    }
    bsp_timer_wrap_t *w = (bsp_timer_wrap_t *)ptimer;
    int ret;
    if (is_period)
    {
        ret = rtos_reload_timer(&w->t.p);
    }
    else
    {
        ret = rtos_oneshot_reload_timer(&w->t.o);
    }
    return (unsigned int)ret;
}
#endif /* BSP_SYSTEM_BK7258_ALLOCATOR_ONLY */
