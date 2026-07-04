//entity_iot_func.c
// 全局函數指針變量由芯片後端靜態初始化（ESP32-S3：sdk/chip_esp32s3/bind/entity_hal_binding_esp32s3.c）。
// 此處的 _Init() 函數保留為空 stub，兼容外部調用點。
#include "entity_iot_func.h"

void Entity_Mem_Func_Init(Entity_Mem_Func_t *func)          { (void)func; }
void Entity_System_Func_Init(Entity_System_Func_t *func)    { (void)func; }
void Entity_Msg_Queue_Func_Init(Entity_Msg_Queue_Func_t *func) { (void)func; }
void Entity_Pthread_Func_Init(Entity_Pthread_Func_t *func)  { (void)func; }
void Entity_System_Timer_Func_Init(Entity_Systme_Timer_Func_t *func) { (void)func; }
void Entity_Semphore_Func_Init(Entity_Semaphore_Func_t *func) { (void)func; }
void Entity_Mutex_Func_Init(Entity_Mutex_Func_t *func)      { (void)func; }
void Entity_Critical_Func_Init(Entity_Critical_Func_t *func){ (void)func; }

bool Entity_Iot_Func_All_Bound(const char **missing_name)
{
    struct { void *p; const char *name; } req[] =
    {
        { (void *)Entity_Mem_Malloc,       "Entity_Mem_Malloc" },
        { (void *)Entity_Mem_Free,         "Entity_Mem_Free" },
        { (void *)Entity_Get_Memory_Snapshot, "Entity_Get_Memory_Snapshot" },
        { (void *)Entity_Get_Run_Time_Ms,  "Entity_Get_Run_Time_Ms" },
        { (void *)Entity_Sleep_Ms,         "Entity_Sleep_Ms" },
        { (void *)Entity_Msg_Queue_Create, "Entity_Msg_Queue_Create" },
        { (void *)Entity_Msg_Queue_Send,   "Entity_Msg_Queue_Send" },
        { (void *)Entity_Msg_Queue_Wait,   "Entity_Msg_Queue_Wait" },
        { (void *)Entity_Pthread_Create,   "Entity_Pthread_Create" },
        { (void *)Entity_Timer_Create,     "Entity_Timer_Create" },
        { (void *)Entity_Semaphore_Create, "Entity_Semaphore_Create" },
        { (void *)Entity_Semaphore_Get,    "Entity_Semaphore_Get" },
        { (void *)Entity_Mutex_Create,     "Entity_Mutex_Create" },
        { (void *)Entity_Mutex_Lock,       "Entity_Mutex_Lock" },
        { (void *)Entity_Critical_Enter,   "Entity_Critical_Enter" },
    };
    for (unsigned i = 0; i < sizeof(req) / sizeof(req[0]); ++i)
    {
        if (req[i].p == NULL)
        {
            if (missing_name) { *missing_name = req[i].name; }
            return false;
        }
    }
    if (missing_name) { *missing_name = NULL; }
    return true;
}
