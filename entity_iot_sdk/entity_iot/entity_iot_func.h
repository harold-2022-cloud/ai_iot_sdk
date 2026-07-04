//entity_iot_func.h
#pragma once

//提供给SDK用的函数接口，平台需要对接

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

// 可移植 PSRAM BSS 段宏：ESP 平台映射到 EXT_RAM_BSS_ATTR，其他平台為空屬性
#ifdef ESP_PLATFORM
#  include "esp_attr.h"
#  define ENTITY_PSRAM_BSS  EXT_RAM_BSS_ATTR
#else
#  define ENTITY_PSRAM_BSS
#endif

typedef void*   Entity_Thread_t;
typedef void*   Entity_Queue_t;
typedef void*   Entity_System_Timer_t;
typedef void*   Entity_Mutex_t;
typedef void*   Entity_Sema_t;
typedef void*   Entity_Critical_t;   // 臨界區句柄（禁止調度的自旋鎖，可在 ISR 中使用）

// Entity_Mutex_Lock / Entity_Semaphore_Get 的 timeout_ms 特殊值：永久阻塞等待
#define ENTITY_WAIT_FOREVER   0xFFFFFFFFU


//设置IOT的内存管理函数
typedef void *(*Entity_Mem_Malloc_f)(unsigned int size);
typedef void (*Entity_Mem_Free_f)(void *addr);
typedef void *(*Entity_Mem_Calloc_f)(size_t num, size_t size);
typedef void *(*Entity_Mem_Realloc_f)(void *ptr, size_t size);
typedef struct 
{
    Entity_Mem_Malloc_f Malloc;
    Entity_Mem_Free_f Free;
    Entity_Mem_Calloc_f Calloc;
    Entity_Mem_Realloc_f Realloc;
}Entity_Mem_Func_t;
void Entity_Mem_Func_Init(Entity_Mem_Func_t *func);

//设置IOT的系统时间相关函数
typedef uint32_t (*Entity_Get_Run_Time_Ms_f)(void);
typedef uint64_t (*Entity_Get_Time_Stamp_Ms_f)(void);
typedef unsigned int (*Entity_Get_Time_Stamp_f)(void);
typedef int (*Entity_Set_Time_Stamp_f)(unsigned int stamp);
typedef int32_t (*Entity_Get_Zone_Offset_f)(void);
typedef void (*Entity_Set_Zone_Offset_f)(int32_t value);
typedef void (*Entity_Sleep_Ms_f)(uint32_t nms);
typedef void (*Entity_System_Reset_f)(void);
typedef void (*Entity_Debug_Heap_Info_f)(void);
typedef void (*Entity_Debug_Sram_Heap_Info_f)(void);
typedef void (*Entity_Debug_Psram_Heap_Info_f)(void);
typedef unsigned int (*Entity_Get_Device_Id_f)(void);
typedef uint64_t (*Entity_Get_Uptime_Ms_f)(void);   //单调运行时间 ms（不受 NTP/墙钟跳变影响，用于 RTT/超时等相对计时）
typedef uint32_t (*Entity_Rand_f)(void);            //随机数
typedef struct
{
    Entity_Get_Run_Time_Ms_f Get_Run_Time_Ms;
    Entity_Get_Time_Stamp_Ms_f Get_Time_Stamp_Ms;
    Entity_Get_Time_Stamp_f Get_Time_Stamp;
    Entity_Set_Time_Stamp_f Set_Time_Stamp;
    Entity_Get_Zone_Offset_f Get_Zone_Offset;
    Entity_Set_Zone_Offset_f Set_Zone_Offset;
    Entity_Sleep_Ms_f Sleep_Ms;
    Entity_System_Reset_f System_Reset;
    Entity_Debug_Heap_Info_f Debug_Heap_Info;
    Entity_Debug_Sram_Heap_Info_f Debug_Sram_Heap_Info;
    Entity_Debug_Psram_Heap_Info_f Debug_Psram_Heap_Info;
    Entity_Get_Device_Id_f Get_Device_Id;
}Entity_System_Func_t;
void Entity_System_Func_Init(Entity_System_Func_t *func);

//设置IOT的消息队列相关函数
typedef uint32_t (*Entity_Msg_Queue_Create_f)(Entity_Queue_t *queue, uint16_t queue_len, uint32_t msg_size);
typedef uint32_t (*Entity_Msg_Queue_Delete_f)(Entity_Queue_t *queue);
typedef uint32_t (*Entity_Msg_Queue_Send_f)(Entity_Queue_t *queue, void *msg, uint32_t msg_size, uint32_t timeout_ms);
typedef uint32_t (*Entity_Msg_Queue_Wait_f)(Entity_Queue_t *queue, void *msg, uint32_t *msg_size, uint32_t timeout_ms);
typedef uint32_t (*Entity_Msg_Queue_Get_Msg_Num_f)(Entity_Queue_t *queue);
typedef struct 
{
    Entity_Msg_Queue_Create_f Msg_Queue_Create;
    Entity_Msg_Queue_Delete_f Msg_Queue_Delete;
    Entity_Msg_Queue_Send_f Msg_Queue_Send;
    Entity_Msg_Queue_Wait_f Msg_Queue_Wait;
    Entity_Msg_Queue_Get_Msg_Num_f Msg_Queue_Get_Msg_Num;
}Entity_Msg_Queue_Func_t;
void Entity_Msg_Queue_Func_Init(Entity_Msg_Queue_Func_t *func);

//设置IOT的线程函数
typedef int (*Entity_Pthread_Create_f)(Entity_Thread_t* thread_t, const char *name, unsigned int stack_size, unsigned int priority, void *cb,void *arg);
typedef int (*Entity_Pthread_Delete_f)(Entity_Thread_t* thread_t);
typedef struct 
{
    Entity_Pthread_Create_f Pthread_Create;
    Entity_Pthread_Delete_f Pthread_Delete;
}Entity_Pthread_Func_t;
void Entity_Pthread_Func_Init(Entity_Pthread_Func_t *func);

//设置IOT的定时器函数
typedef enum
{
    ENTITY_TIMER_ONESHOT_TYPE,//单次定时器
    ENTITY_TIMER_PERIOD_TYPE,//周期定时器
}Entity_System_Timer_Type_e;
typedef void (*Entity_System_Timer_Callback_f)(void *data);
typedef Entity_System_Timer_t (*Entity_System_Timer_Create_f)(unsigned char is_period, unsigned int interval, void *cb,void *arg);
typedef unsigned int (*Entity_System_Timer_Start_f)(unsigned char is_period, Entity_System_Timer_t ptimer);
typedef unsigned int (*Entity_System_Timer_Stop_f)(unsigned char is_period, Entity_System_Timer_t ptimer);
typedef unsigned int (*Entity_System_Timer_Delete_f)(unsigned char is_period, Entity_System_Timer_t ptimer);
typedef bool (*Entity_System_Timer_Is_Init_f)(unsigned char is_period, Entity_System_Timer_t ptimer);
typedef bool (*Entity_System_Timer_Is_Running_f)(unsigned char is_period, Entity_System_Timer_t ptimer);
typedef unsigned int (*Entity_System_Timer_Reload_f)(unsigned char is_period, Entity_System_Timer_t ptimer);
typedef struct 
{
    Entity_System_Timer_Create_f Timer_Create;
    Entity_System_Timer_Start_f Timer_Start;
    Entity_System_Timer_Stop_f Timer_Stop;
    Entity_System_Timer_Delete_f Timer_Delete;
    Entity_System_Timer_Is_Init_f Timer_Is_Init;
    Entity_System_Timer_Is_Running_f Timer_Is_Running;
    Entity_System_Timer_Reload_f Timer_Reload;
}Entity_Systme_Timer_Func_t;
void Entity_System_Timer_Func_Init(Entity_Systme_Timer_Func_t *func);

//设置IOT的信号量函数
typedef uint32_t (*Entity_Semaphore_Create_f)(Entity_Sema_t* psema, int maxcount);
typedef uint32_t (*Entity_Semaphore_Delete_f)(Entity_Sema_t* psema);
typedef uint32_t (*Entity_Semaphore_Set_f)(Entity_Sema_t* psema);
typedef uint32_t (*Entity_Semaphore_Get_f)(Entity_Sema_t* psema, uint32_t timeout_ms);
typedef struct 
{
    Entity_Semaphore_Create_f Semaphore_Create;
    Entity_Semaphore_Delete_f Semaphore_Delete;
    Entity_Semaphore_Set_f Semaphore_Set;
    Entity_Semaphore_Get_f Semaphore_Get;
}Entity_Semaphore_Func_t;
void Entity_Semphore_Func_Init(Entity_Semaphore_Func_t *func);


//互斥锁
typedef uint32_t (*Entity_Mutex_Create_f)(Entity_Mutex_t* pmutex);
typedef uint32_t (*Entity_Mutex_Delete_f)(Entity_Mutex_t* pmutex);
typedef uint32_t (*Entity_Mutex_Lock_f)(Entity_Mutex_t* pmutex, uint32_t timeout_ms);
typedef uint32_t (*Entity_Mutex_Unlock_f)(Entity_Mutex_t* pmutex);
typedef struct 
{
    Entity_Mutex_Create_f Mutex_Create;
    Entity_Mutex_Delete_f Mutex_Delete;
    Entity_Mutex_Lock_f Mutex_Lock;
    Entity_Mutex_Unlock_f Mutex_Unlock;
}Entity_Mutex_Func_t;
void Entity_Mutex_Func_Init(Entity_Mutex_Func_t *func);


extern Entity_Mem_Malloc_f Entity_Mem_Malloc;
extern Entity_Mem_Free_f Entity_Mem_Free;
extern Entity_Mem_Calloc_f Entity_Mem_Calloc;
extern Entity_Mem_Realloc_f Entity_Mem_Realloc;

extern Entity_Get_Run_Time_Ms_f Entity_Get_Run_Time_Ms;
extern Entity_Get_Time_Stamp_Ms_f Entity_Get_Time_Stamp_Ms;
extern Entity_Get_Time_Stamp_f Entity_Get_Time_Stamp;
extern Entity_Set_Time_Stamp_f Entity_Set_Time_Stamp;
extern Entity_Get_Zone_Offset_f Entity_Get_Zone_Offset;
extern Entity_Set_Zone_Offset_f Entity_Set_Zone_Offset;
extern Entity_Sleep_Ms_f Entity_Sleep_Ms;
extern Entity_System_Reset_f Entity_System_Reset; 
extern Entity_Debug_Heap_Info_f Entity_Debug_Heap_Info;
extern Entity_Debug_Sram_Heap_Info_f Entity_Debug_Sram_Heap_Info;
extern Entity_Debug_Psram_Heap_Info_f Entity_Debug_Psram_Heap_Info;
extern Entity_Get_Device_Id_f Entity_Get_Device_Id;
extern Entity_Get_Uptime_Ms_f Entity_Get_Uptime_Ms;
extern Entity_Rand_f Entity_Rand;

/**
 * @name    Entity_Memory_Snapshot_t / Entity_Get_Memory_Snapshot
 * @brief   内存快照 HAL 缝：一次性读取 internal/DMA/external(PSRAM) 的
 *          free 与 largest free block，供上层避免直接调用 heap_caps_*。
 *          非 ESP 芯片无 DMA/PSRAM 时对应字段填 0。
 * @param   out  输出快照结构体指针，由调用方提供，非 NULL
 */
typedef struct
{
    uint32_t internal_free;     uint32_t internal_largest;
    uint32_t dma_free;          uint32_t dma_largest;
    uint32_t external_free;     uint32_t external_largest;  /* PSRAM；无则填 0 */
} Entity_Memory_Snapshot_t;
typedef void (*Entity_Get_Memory_Snapshot_f)(Entity_Memory_Snapshot_t *out);
extern Entity_Get_Memory_Snapshot_f Entity_Get_Memory_Snapshot;

extern Entity_Msg_Queue_Create_f Entity_Msg_Queue_Create;
extern Entity_Msg_Queue_Delete_f Entity_Msg_Queue_Delete;
extern Entity_Msg_Queue_Send_f Entity_Msg_Queue_Send;
extern Entity_Msg_Queue_Wait_f Entity_Msg_Queue_Wait;
extern Entity_Msg_Queue_Get_Msg_Num_f Entity_Msg_Queue_Get_Msg_Num;

extern Entity_Pthread_Create_f Entity_Pthread_Create;
extern Entity_Pthread_Delete_f Entity_Pthread_Delete;

extern Entity_System_Timer_Create_f Entity_Timer_Create;
extern Entity_System_Timer_Start_f Entity_Timer_Start;
extern Entity_System_Timer_Stop_f Entity_Timer_Stop;
extern Entity_System_Timer_Delete_f Entity_Timer_Delete;
extern Entity_System_Timer_Is_Init_f Entity_Timer_Is_Init;
extern Entity_System_Timer_Is_Running_f Entity_Timer_Is_Running;
extern Entity_System_Timer_Reload_f Entity_Timer_Reload;

extern Entity_Semaphore_Create_f Entity_Semaphore_Create;
extern Entity_Semaphore_Delete_f Entity_Semaphore_Delete;
extern Entity_Semaphore_Set_f Entity_Semaphore_Set;
extern Entity_Semaphore_Get_f Entity_Semaphore_Get;

extern Entity_Mutex_Create_f Entity_Mutex_Create;
extern Entity_Mutex_Delete_f Entity_Mutex_Delete;
extern Entity_Mutex_Lock_f Entity_Mutex_Lock;
extern Entity_Mutex_Unlock_f Entity_Mutex_Unlock;

//臨界區（spinlock，禁止搶佔；保護極短的共享資源，可在 ISR 中呼叫）
typedef uint32_t (*Entity_Critical_Create_f)(Entity_Critical_t *pcs);
typedef uint32_t (*Entity_Critical_Delete_f)(Entity_Critical_t *pcs);
typedef void     (*Entity_Critical_Enter_f)(Entity_Critical_t *pcs);
typedef void     (*Entity_Critical_Exit_f)(Entity_Critical_t *pcs);
typedef struct
{
    Entity_Critical_Create_f Critical_Create;
    Entity_Critical_Delete_f Critical_Delete;
    Entity_Critical_Enter_f  Critical_Enter;
    Entity_Critical_Exit_f   Critical_Exit;
} Entity_Critical_Func_t;
void Entity_Critical_Func_Init(Entity_Critical_Func_t *func);

extern Entity_Critical_Create_f Entity_Critical_Create;
extern Entity_Critical_Delete_f Entity_Critical_Delete;
extern Entity_Critical_Enter_f  Entity_Critical_Enter;
extern Entity_Critical_Exit_f   Entity_Critical_Exit;

/**
 * @name    Entity_Iot_Func_All_Bound
 * @brief   开机 HAL 绑定完整性自检。
 *          检查全部「必需」函数指针是否已绑定；用于启动时提前发现漏注册，
 *          将运行时空指针崩溃前移到启动即报。
 * @param   missing_name  输出参数：若有指针未绑定，指向首个未绑定指针的名称字符串；
 *                        全部已绑定时置为 NULL；调用方传 NULL 时忽略。
 * @retval  true   全部必需指针均已绑定
 * @retval  false  至少一个必需指针未绑定，*missing_name 已填入其名称
 */
bool Entity_Iot_Func_All_Bound(const char **missing_name);

// 由芯片後端提供：把平台 (Bsp_*) 實作綁定到 entity_iot 各域 Cbs（Log/Timer/cJSON/Network）。
// 定義於 sdk/chip_esp32s3/bind/entity_system_bind_esp32s3.c（換芯片時各自提供同名實作）。
// 產品 aggregator（Entity_System_All_Func_Import_Interface_Init）呼叫它；芯片中立名，產品呼叫點不隨芯片改變。
void Entity_System_Chip_Common_Init(void);




















