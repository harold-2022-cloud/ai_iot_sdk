// entity_hal_binding_bk7258.c
// BK7258 chip backend 的 HAL 綁定：定義全部 32 個 entity_iot_func 函數指針，
// 並由 Entity_Chip_Bk7258_Bind() 顯式賦值（armino 不用 force-link）。
//
// 與 esp32 版（chip_esp32s3/bind/entity_hal_binding_esp32s3.c）結構鏡像，
// 兩處全域差異：
//   (1) 顯式 bind 取代靜態初始化（BK 不用 WHOLE_ARCHIVE，app 主動呼叫 Bind）。
//   (2) BK 目標無 PSRAM、單核 → 記憶體用 Bsp_Mem_*（header-set 配置器，
//       Task 1.1），queue/sema/mutex 用「非 _Psram」的 Bsp_*（Task 1.2）。
//
// 本檔自帶全部 gap 配接器（core 會呼叫故不可 NULL），全部以本檔 local static
// 實作，不修改 bsp_system_bk7258.c。
//
// 移植新芯片時新增另一份 chip_<xxx>/bind/...，不改 entity_iot_sdk core。

#include "entity_iot_func.h"   /* 32 個函數指針 typedef + extern 宣告 */
#include "bsp_system.h"        /* Bsp_Mem/Pthread/Timer/Mutex/Sema/Queue/time 原型 */
#include "bsp_sync_bk7258.h"   /* Bsp_Critical_* 原型 */
#include "chip_bk7258.h"       /* Entity_Chip_Bk7258_Bind 宣告 */

#include <stdint.h>

// Task 6.2（chip_bk7258_bind.c）：賦值 Entity_Get_Memory_Snapshot / Entity_Net_Set_Option。
// 本檔只 extern-呼叫，All_Bound 的 snapshot 指針由它補上。
extern void Entity_Chip_Bk7258_Bind_Special(void);

// BK reboot：bk_reboot 為 armino 系統 API（components/system.h，Phase 8 才實際存在）。
// 本檔不依賴 armino 頭，前向宣告之；Phase 8 若簽名/頭路徑有別，僅需調整此宣告。
// 注：Bsp_System_Reset 屬週邊層而非 OS HAL，BK 不提供，故直接呼叫 bk_reboot。
extern void bk_reboot(void);

// ── gap 配接器（core 直接呼叫，NULL 會崩） ─────────────────────────────────────

// Entity_Get_Time_Stamp（core 呼叫 18×）：BK 的 Bsp_Get_Time_Stamp 回傳 uint64_t，
// 對齊 unsigned int 簽名（鏡像 esp 的 Entity_Get_Time_Stamp_Compat）。
static unsigned int _Get_Time_Stamp_Compat(void)
{
    return (unsigned int)Bsp_Get_Time_Stamp();
}

// Entity_Get_Uptime_Ms（core 呼叫 3×）：單調運行時間 ms（自啟動起，不受 NTP/牆鐘跳變影響）。
// 鏡像 esp 的 _Uptime_Ms：Bsp_Get_Run_Time_Us 回傳 int64_t。
static uint64_t _Uptime_Ms(void)
{
    return (uint64_t)(Bsp_Get_Run_Time_Us() / 1000LL);
}

// Entity_System_Reset（core 呼叫 5×）：BK 重啟。
static void _System_Reset(void)
{
    bk_reboot();
}

// Entity_Set/Get_Zone_Offset（set 呼叫 1×、get 0×，仍綁定）：
// process-local 時區偏移存儲，符合契約語義（與 esp 的 Bsp_*_Zone_Offset 等價）。
static int32_t s_zone_offset = 0;

static void _Set_Zone_Offset(int32_t value)
{
    s_zone_offset = value;
}

static int32_t _Get_Zone_Offset(void)
{
    return s_zone_offset;
}

// Entity_Debug_Sram_Heap_Info（3×）/ Entity_Debug_Heap_Info（0×）/
// Entity_Debug_Psram_Heap_Info（0×）：BK debug heap log，no-op/basic。
// 這些是除錯日誌輔助，非 load-bearing；no-op 可接受。
static void _Debug_Heap_Info(void)      {}
static void _Debug_Sram_Heap_Info(void) {}
static void _Debug_Psram_Heap_Info(void) {}

// ── 線程配接器（BK 無 PSRAM → 用普通 Bsp_Pthread_Create） ─────────────────────
// 鏡像 esp 的 _Pthread_Smart_Create，但用非 _Psram 版本。
static int _Pthread_Smart_Create(Bsp_Thread_t *thread_t, const char *name,
                                 unsigned int stack_size, unsigned int priority,
                                 void *cb, void *arg)
{
    return Bsp_Pthread_Create(thread_t, name, stack_size, priority, cb, arg);
}

// ── 全域函數指針定義（本檔為 BK build 唯一定義處，初值 NULL，由 Bind 賦值） ────

// 記憶體（4）— MATCHED-SET：四者必須同綁 Bsp_Mem_*（header-set 配置器），
// 確保配置與釋放使用同一方案；絕不與外來配置器混用。
Entity_Mem_Malloc_f   Entity_Mem_Malloc   = NULL;
Entity_Mem_Free_f     Entity_Mem_Free     = NULL;
Entity_Mem_Calloc_f   Entity_Mem_Calloc   = NULL;
Entity_Mem_Realloc_f  Entity_Mem_Realloc  = NULL;

// 時間/雜項
Entity_Get_Run_Time_Ms_f    Entity_Get_Run_Time_Ms    = NULL;
Entity_Get_Time_Stamp_Ms_f  Entity_Get_Time_Stamp_Ms  = NULL;
Entity_Get_Time_Stamp_f     Entity_Get_Time_Stamp     = NULL;
Entity_Set_Time_Stamp_f     Entity_Set_Time_Stamp     = NULL;
Entity_Get_Zone_Offset_f    Entity_Get_Zone_Offset    = NULL;
Entity_Set_Zone_Offset_f    Entity_Set_Zone_Offset    = NULL;
Entity_Sleep_Ms_f           Entity_Sleep_Ms           = NULL;
Entity_System_Reset_f       Entity_System_Reset       = NULL;
Entity_Debug_Heap_Info_f       Entity_Debug_Heap_Info       = NULL;
Entity_Debug_Sram_Heap_Info_f  Entity_Debug_Sram_Heap_Info  = NULL;
Entity_Debug_Psram_Heap_Info_f Entity_Debug_Psram_Heap_Info = NULL;
Entity_Get_Device_Id_f      Entity_Get_Device_Id      = NULL;
Entity_Get_Uptime_Ms_f      Entity_Get_Uptime_Ms      = NULL;
Entity_Rand_f               Entity_Rand               = NULL;

// 消息隊列（5）
Entity_Msg_Queue_Create_f      Entity_Msg_Queue_Create      = NULL;
Entity_Msg_Queue_Delete_f      Entity_Msg_Queue_Delete      = NULL;
Entity_Msg_Queue_Send_f        Entity_Msg_Queue_Send        = NULL;
Entity_Msg_Queue_Wait_f        Entity_Msg_Queue_Wait        = NULL;
Entity_Msg_Queue_Get_Msg_Num_f Entity_Msg_Queue_Get_Msg_Num = NULL;

// 線程（2）
Entity_Pthread_Create_f Entity_Pthread_Create = NULL;
Entity_Pthread_Delete_f Entity_Pthread_Delete = NULL;

// 定時器（7）
Entity_System_Timer_Create_f     Entity_Timer_Create     = NULL;
Entity_System_Timer_Start_f      Entity_Timer_Start      = NULL;
Entity_System_Timer_Stop_f       Entity_Timer_Stop       = NULL;
Entity_System_Timer_Delete_f     Entity_Timer_Delete     = NULL;
Entity_System_Timer_Is_Init_f    Entity_Timer_Is_Init    = NULL;
Entity_System_Timer_Is_Running_f Entity_Timer_Is_Running = NULL;
Entity_System_Timer_Reload_f     Entity_Timer_Reload     = NULL;

// 信號量（4）
Entity_Semaphore_Create_f Entity_Semaphore_Create = NULL;
Entity_Semaphore_Delete_f Entity_Semaphore_Delete = NULL;
Entity_Semaphore_Set_f    Entity_Semaphore_Set    = NULL;
Entity_Semaphore_Get_f    Entity_Semaphore_Get    = NULL;

// 互斥鎖（4）
Entity_Mutex_Create_f  Entity_Mutex_Create  = NULL;
Entity_Mutex_Delete_f  Entity_Mutex_Delete  = NULL;
Entity_Mutex_Lock_f    Entity_Mutex_Lock    = NULL;
Entity_Mutex_Unlock_f  Entity_Mutex_Unlock  = NULL;

// 臨界區（4）
Entity_Critical_Create_f Entity_Critical_Create = NULL;
Entity_Critical_Delete_f Entity_Critical_Delete = NULL;
Entity_Critical_Enter_f  Entity_Critical_Enter  = NULL;
Entity_Critical_Exit_f   Entity_Critical_Exit   = NULL;

// ── 顯式綁定入口 ──────────────────────────────────────────────────────────────
//
// armino app 早期呼叫；賦值全部指針後呼叫 Bind_Special（Task 6.2）補上
// snapshot + net-option 指針。
void Entity_Chip_Bk7258_Bind(void)
{
    // 記憶體（4）— MATCHED-SET：全綁 Bsp_Mem_*（NOT Bsp_Psram_*；BK 無 PSRAM）。
    Entity_Mem_Malloc  = Bsp_Mem_Malloc;
    Entity_Mem_Free    = Bsp_Mem_Free;
    Entity_Mem_Calloc  = Bsp_Mem_Calloc;
    Entity_Mem_Realloc = Bsp_Mem_Realloc;

    // 時間/雜項
    Entity_Get_Run_Time_Ms   = Bsp_Get_Run_Time_Ms;
    Entity_Get_Time_Stamp_Ms = Bsp_Get_Time_Stamp_Ms;
    Entity_Get_Time_Stamp    = _Get_Time_Stamp_Compat;   // gap：BK 無 unsigned-int 版
    Entity_Set_Time_Stamp    = Bsp_Set_Time_Stamp;
    Entity_Get_Zone_Offset   = _Get_Zone_Offset;         // gap：process-local 存儲
    Entity_Set_Zone_Offset   = _Set_Zone_Offset;         // gap：process-local 存儲
    Entity_Sleep_Ms          = Bsp_Sleep_Ms;
    Entity_System_Reset      = _System_Reset;            // gap：bk_reboot
    Entity_Debug_Heap_Info       = _Debug_Heap_Info;        // gap：no-op
    Entity_Debug_Sram_Heap_Info  = _Debug_Sram_Heap_Info;   // gap：no-op
    Entity_Debug_Psram_Heap_Info = _Debug_Psram_Heap_Info;  // gap：no-op
    Entity_Get_Device_Id     = Bsp_Get_Device_Id;
    Entity_Get_Uptime_Ms     = _Uptime_Ms;               // gap：Bsp_Get_Run_Time_Us/1000
    Entity_Rand              = Bsp_Random;

    // 消息隊列（5）— 非 _Psram（BK 無 PSRAM）。Cast 對齊 Entity_Msg_Queue_*_f（如 esp）。
    Entity_Msg_Queue_Create      = (Entity_Msg_Queue_Create_f)Bsp_Msg_Queue_Create;
    Entity_Msg_Queue_Delete      = (Entity_Msg_Queue_Delete_f)Bsp_Msg_Queue_Delete;
    Entity_Msg_Queue_Send        = (Entity_Msg_Queue_Send_f)Bsp_Msg_Queue_Send;
    Entity_Msg_Queue_Wait        = (Entity_Msg_Queue_Wait_f)Bsp_Msg_Queue_Wait;
    Entity_Msg_Queue_Get_Msg_Num = (Entity_Msg_Queue_Get_Msg_Num_f)Bsp_Msg_Queue_Get_Msg_Num;

    // 線程（2）— 非 _Psram；Create 走 _Pthread_Smart_Create 配接器（如 esp）。
    Entity_Pthread_Create = (Entity_Pthread_Create_f)_Pthread_Smart_Create;
    Entity_Pthread_Delete = Bsp_Pthread_Delete;

    // 定時器（7）
    Entity_Timer_Create     = Bsp_Timer_Create;
    Entity_Timer_Start      = Bsp_Timer_Start;
    Entity_Timer_Stop       = Bsp_Timer_Stop;
    Entity_Timer_Delete     = Bsp_Timer_Delete;
    Entity_Timer_Is_Init    = Bsp_Timer_Is_Init;
    Entity_Timer_Is_Running = Bsp_Timer_Is_Running;
    Entity_Timer_Reload     = Bsp_Timer_Reload;

    // 信號量（4）— 非 _Psram。Cast 如 esp。
    Entity_Semaphore_Create = (Entity_Semaphore_Create_f)Bsp_Semaphore_Init;
    Entity_Semaphore_Delete = (Entity_Semaphore_Delete_f)Bsp_Semaphore_Deinit;
    Entity_Semaphore_Set    = (Entity_Semaphore_Set_f)Bsp_Set_Semaphore;
    Entity_Semaphore_Get    = (Entity_Semaphore_Get_f)Bsp_Get_Semaphore;

    // 互斥鎖（4）— 非 _Psram。Cast 如 esp。
    Entity_Mutex_Create = (Entity_Mutex_Create_f)Bsp_Mutex_Init;
    Entity_Mutex_Delete = (Entity_Mutex_Delete_f)Bsp_Mutex_Destroy;
    Entity_Mutex_Lock   = (Entity_Mutex_Lock_f)Bsp_Mutex_Lock;
    Entity_Mutex_Unlock = (Entity_Mutex_Unlock_f)Bsp_Mutex_Unlock;

    // 臨界區（4）— Bsp_Critical_*（Task 2.2）。Cast 如 esp。
    Entity_Critical_Create = (Entity_Critical_Create_f)Bsp_Critical_Create;
    Entity_Critical_Delete = (Entity_Critical_Delete_f)Bsp_Critical_Delete;
    Entity_Critical_Enter  = (Entity_Critical_Enter_f)Bsp_Critical_Enter;
    Entity_Critical_Exit   = (Entity_Critical_Exit_f)Bsp_Critical_Exit;

    // Task 6.2：snapshot + net-option 指針（Entity_Get_Memory_Snapshot 等）。
    Entity_Chip_Bk7258_Bind_Special();
}
