//entity_hal_binding_esp32s3.c
// ESP32-S3 chip backend 的 HAL 綁定：直接初始化所有 entity_iot_func 函數指針。
// （P6.2a：自 alarm/xiaozhi 產品側上移至 sdk/chip_esp32s3，兩產品原為逐 byte 相同。）
// 移植新芯片時新增另一份 sdk/chip_<xxx>/bind/entity_hal_binding_<xxx>.c，不改 entity_iot_sdk core。
#include "entity_iot_func.h"

#include "bsp_system.h"
#include "bsp_power.h"

#include "esp_heap_caps.h"        /* _Psram_Calloc_Prefer / _Critical 的 heap_caps */
#include "freertos/FreeRTOS.h"    /* _Critical_* 的 portMUX */

// ── PSRAM 優先分配 ────────────────────────────────────────────────────────────

static void *_Psram_Calloc_Prefer(size_t num, size_t size)
{
    void *ptr = heap_caps_calloc(num, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (ptr == NULL)
    {
        ptr = heap_caps_calloc(num, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    return ptr;
}


// ── 臨界區（FreeRTOS portMUX） ────────────────────────────────────────────────

static uint32_t _Critical_Create(void **pcs)
{
    portMUX_TYPE *mux = (portMUX_TYPE *)_Psram_Calloc_Prefer(1, sizeof(portMUX_TYPE));
    if (mux == NULL)
        return (uint32_t)-1;
    portMUX_TYPE init = portMUX_INITIALIZER_UNLOCKED;
    *mux = init;
    *pcs = mux;
    return 0;
}

static uint32_t _Critical_Delete(void **pcs)
{
    if (pcs == NULL || *pcs == NULL)
        return (uint32_t)-1;
    heap_caps_free(*pcs);
    *pcs = NULL;
    return 0;
}

static void _Critical_Enter(void **pcs)
{
    taskENTER_CRITICAL((portMUX_TYPE *)*pcs);
}

static void _Critical_Exit(void **pcs)
{
    taskEXIT_CRITICAL((portMUX_TYPE *)*pcs);
}

// ── 線程（PSRAM 棧） ──────────────────────────────────────────────────────────

static int _Pthread_Smart_Create(Bsp_Thread_t *thread_t, const char *name,
                                 unsigned int stack_size, unsigned int priority,
                                 void *cb, void *arg)
{
    return Bsp_Pthread_Create_Psram(thread_t, name, stack_size, priority, cb, arg);
}

// ── 全局函數指針定義（靜態初始化，啟動前即可用） ──────────────────────────────

Entity_Mem_Malloc_f   Entity_Mem_Malloc   = Bsp_Psram_Malloc;
Entity_Mem_Free_f     Entity_Mem_Free     = Bsp_Psram_Free;
Entity_Mem_Calloc_f   Entity_Mem_Calloc   = Bsp_Psram_Calloc;
Entity_Mem_Realloc_f  Entity_Mem_Realloc  = Bsp_Psram_Realloc;

static unsigned int Entity_Get_Time_Stamp_Compat(void)
{
    return (unsigned int)Bsp_Get_Time_Stamp();
}

// 單調運行時間（ms）：ESP 平台用 esp_timer（自啟動起，不受 NTP/牆鐘跳變影響）
static uint64_t _Uptime_Ms(void)
{
    return (uint64_t)(Bsp_Get_Run_Time_Us() / 1000LL);
}

Entity_Get_Run_Time_Ms_f    Entity_Get_Run_Time_Ms    = Bsp_Get_Run_Time_Ms;
Entity_Get_Time_Stamp_Ms_f  Entity_Get_Time_Stamp_Ms  = Bsp_Get_Time_Stamp_Ms;
Entity_Get_Time_Stamp_f     Entity_Get_Time_Stamp     = Entity_Get_Time_Stamp_Compat;
Entity_Set_Time_Stamp_f     Entity_Set_Time_Stamp     = Bsp_Set_Time_Stamp;
Entity_Get_Zone_Offset_f    Entity_Get_Zone_Offset    = Bsp_Get_Zone_Offset;
Entity_Set_Zone_Offset_f    Entity_Set_Zone_Offset    = Bsp_Set_Zone_Offset;
Entity_Sleep_Ms_f           Entity_Sleep_Ms           = Bsp_Sleep_Ms;
Entity_System_Reset_f       Entity_System_Reset       = Bsp_System_Reset;
Entity_Debug_Heap_Info_f    Entity_Debug_Heap_Info    = Bsp_Debug_Heap_Info;
Entity_Debug_Sram_Heap_Info_f  Entity_Debug_Sram_Heap_Info  = Bsp_Debug_Sram_Heap_Info;
Entity_Debug_Psram_Heap_Info_f Entity_Debug_Psram_Heap_Info = Bsp_Debug_Psram_Heap_Info;
Entity_Get_Device_Id_f      Entity_Get_Device_Id      = Bsp_Get_Device_Id;
Entity_Get_Uptime_Ms_f      Entity_Get_Uptime_Ms      = _Uptime_Ms;
Entity_Rand_f               Entity_Rand               = Bsp_Random;

Entity_Msg_Queue_Create_f     Entity_Msg_Queue_Create     = (Entity_Msg_Queue_Create_f)Bsp_Msg_Queue_Create_Psram;
Entity_Msg_Queue_Delete_f     Entity_Msg_Queue_Delete     = (Entity_Msg_Queue_Delete_f)Bsp_Msg_Queue_Delete_Psram;
Entity_Msg_Queue_Send_f       Entity_Msg_Queue_Send       = (Entity_Msg_Queue_Send_f)Bsp_Msg_Queue_Send_Psram;
Entity_Msg_Queue_Wait_f       Entity_Msg_Queue_Wait       = (Entity_Msg_Queue_Wait_f)Bsp_Msg_Queue_Wait_Psram;
Entity_Msg_Queue_Get_Msg_Num_f Entity_Msg_Queue_Get_Msg_Num = (Entity_Msg_Queue_Get_Msg_Num_f)Bsp_Msg_Queue_Get_Msg_Num_Psram;

Entity_Pthread_Create_f Entity_Pthread_Create = (Entity_Pthread_Create_f)_Pthread_Smart_Create;
Entity_Pthread_Delete_f Entity_Pthread_Delete = Bsp_Pthread_Delete;

Entity_System_Timer_Create_f     Entity_Timer_Create     = Bsp_Timer_Create;
Entity_System_Timer_Start_f      Entity_Timer_Start      = Bsp_Timer_Start;
Entity_System_Timer_Stop_f       Entity_Timer_Stop       = Bsp_Timer_Stop;
Entity_System_Timer_Delete_f     Entity_Timer_Delete     = Bsp_Timer_Delete;
Entity_System_Timer_Is_Init_f    Entity_Timer_Is_Init    = Bsp_Timer_Is_Init;
Entity_System_Timer_Is_Running_f Entity_Timer_Is_Running = Bsp_Timer_Is_Running;
Entity_System_Timer_Reload_f     Entity_Timer_Reload     = Bsp_Timer_Reload;

Entity_Semaphore_Create_f Entity_Semaphore_Create = (Entity_Semaphore_Create_f)Bsp_Semaphore_Init_Psram;
Entity_Semaphore_Delete_f Entity_Semaphore_Delete = (Entity_Semaphore_Delete_f)Bsp_Semaphore_Deinit_Psram;
Entity_Semaphore_Set_f    Entity_Semaphore_Set    = (Entity_Semaphore_Set_f)Bsp_Set_Semaphore_Psram;
Entity_Semaphore_Get_f    Entity_Semaphore_Get    = (Entity_Semaphore_Get_f)Bsp_Get_Semaphore_Psram;

Entity_Mutex_Create_f  Entity_Mutex_Create  = (Entity_Mutex_Create_f)Bsp_Mutex_Init_Psram;
Entity_Mutex_Delete_f  Entity_Mutex_Delete  = (Entity_Mutex_Delete_f)Bsp_Mutex_Destroy_Psram;
Entity_Mutex_Lock_f    Entity_Mutex_Lock    = (Entity_Mutex_Lock_f)Bsp_Mutex_Lock_Psram;
Entity_Mutex_Unlock_f  Entity_Mutex_Unlock  = (Entity_Mutex_Unlock_f)Bsp_Mutex_Unlock_Psram;

Entity_Critical_Create_f Entity_Critical_Create = (Entity_Critical_Create_f)_Critical_Create;
Entity_Critical_Delete_f Entity_Critical_Delete = (Entity_Critical_Delete_f)_Critical_Delete;
Entity_Critical_Enter_f  Entity_Critical_Enter  = (Entity_Critical_Enter_f)_Critical_Enter;
Entity_Critical_Exit_f   Entity_Critical_Exit   = (Entity_Critical_Exit_f)_Critical_Exit;
