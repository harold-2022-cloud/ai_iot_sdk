//bsp_system.h
#pragma once


#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif


/* 平台 Mutex 適配：將 POSIX pthread_mutex_* 重定向到 FreeRTOS Bsp_Mutex_*
 * ⚠️  include 順序約束：
 *     本檔案必須在 <pthread.h> 之前 include，否則宏無法覆蓋已展開的函式呼叫。
 *     正確做法：僅透過 bsp_include.h 統一引入，禁止直接 #include <pthread.h>。
 * 效果：mi_mqtt 等第三方代碼的 pthread_mutex 呼叫全部走 FreeRTOS xSemaphoreTake/Give，
 *       系統內沒有真正的 POSIX mutex，不存在兩種鎖混用的問題。*/
#ifdef pthread_mutex_init
#  undef pthread_mutex_init
#endif
#ifdef pthread_mutex_destroy
#  undef pthread_mutex_destroy
#endif
#ifdef pthread_mutex_lock
#  undef pthread_mutex_lock
#endif
#ifdef pthread_mutex_unlock
#  undef pthread_mutex_unlock
#endif
#define pthread_mutex_init(a,b)     Bsp_Mutex_Init(a)
#define pthread_mutex_destroy(n)    Bsp_Mutex_Destroy(n)
#define pthread_mutex_lock(n)       Bsp_Mutex_Lock(n, 0xffffffffU)
#define pthread_mutex_unlock(n)     Bsp_Mutex_Unlock(n)



typedef void (*Timer_Callback_f)(void *data);

typedef void* Bsp_Thread_t;
typedef void*   Bsp_Queue_t;
typedef unsigned int Bsp_Event_t;
typedef void*  Bsp_Mutex_t;
typedef void*  Bsp_Sema_t;

/* ── 统一入口(_Ex):收敛 _Psram/_Pinned 雙胞胎,新代碼優先用這組 ──────────────
 * region 選任務棧/物件記憶體區;_Ex 內部 dispatch 到既有對應函式,行為不變。
 * 注:PSRAM 物件後續操作(Lock/Take/Give/Send/Wait/Deinit)仍須用對應 _Psram 函式。*/
typedef enum { BSP_MEM_INTERNAL = 0, BSP_MEM_PSRAM = 1 } Bsp_Mem_Region_e;
#define BSP_CORE_NO_AFFINITY  (-1)

//统一线程创建(core=-1 不绑核;region 选内部/PSRAM 栈)
int      Bsp_Pthread_Create_Ex(Bsp_Thread_t *thread_t, const char *name, unsigned int stack_size,
                               unsigned int priority, void *cb, void *arg,
                               int core, Bsp_Mem_Region_e region);
//统一消息队列创建
uint32_t Bsp_Msg_Queue_Create_Ex(Bsp_Queue_t *Bsp_Queue, uint16_t queue_len, uint32_t msg_size,
                                 Bsp_Mem_Region_e region);
//统一信号量创建
uint32_t Bsp_Semaphore_Init_Ex(Bsp_Sema_t *sema, int maxcount, Bsp_Mem_Region_e region);
//统一互斥锁创建
uint32_t Bsp_Mutex_Init_Ex(Bsp_Mutex_t *mutex, Bsp_Mem_Region_e region);

//阻塞微秒延时
void Bsp_Block_Delay_Us(uint32_t nus);

//阻塞毫秒延时
void Bsp_Block_Delay_Ms(uint32_t nus);

//非阻塞毫秒延时
void Bsp_Sleep_Ms(uint32_t nms);

//获取系统运行时间ms
uint32_t Bsp_Get_Run_Time_Ms(void);

//获取系统运行时间us(单调,自启动累加;绝对墙钟用 Bsp_Get_Time_Stamp_Ms)
int64_t Bsp_Get_Run_Time_Us(void);

//获取系统时间戳ms
uint64_t Bsp_Get_Time_Stamp_Ms(void);

//设置时间 
int Bsp_Set_Time_Stamp(unsigned int stamp);

//获取系统时间戳
uint64_t Bsp_Get_Time_Stamp(void);

//获取时区偏移量，单位秒
int32_t Bsp_Get_Zone_Offset(void);

//设置时区偏移量，单位秒
void Bsp_Set_Zone_Offset(int32_t value);

//设置 POSIX 时区规则（用于支持夏令时）
void Bsp_Set_Timezone_Rule(const char *timezone_id, const char *posix_tz, int32_t fallback_offset_sec);

//获取当前应用的时区 ID
const char *Bsp_Get_Timezone_Id(void);

//安全模块初始化
void Bsp_Cipher_Init(void);

//AT初始化
void Bsp_At_Cmd_Init(void);

//内存申请 
void *Bsp_Mem_Malloc(unsigned int size);

//内存释放
void Bsp_Mem_Free(void *addr);

//内存申请并初始化为0
void *Bsp_Mem_Calloc(size_t num, size_t size);

//内存重新申请
void *Bsp_Mem_Realloc(void *ptr, size_t size);

//外部内存申请 
void *Bsp_Psram_Malloc(unsigned int size);

//外部内存申请并清0 
void *Bsp_Psram_Zalloc(unsigned int size);

//外部内存申请并初始化为0
void *Bsp_Psram_Calloc(size_t num, size_t size);

//内存重新申请
void *Bsp_Psram_Realloc(void *ptr, size_t size);

//外部内存释放
void Bsp_Psram_Free(void *addr);

//获取随机数
uint32_t Bsp_Random(void);

//创建定时器
void* Bsp_Timer_Create(unsigned char is_period, unsigned int interval, void *cb,void *arg);

//启动定时器
unsigned int Bsp_Timer_Start(unsigned char is_period, void* ptimer);

//停止定时器
unsigned int Bsp_Timer_Stop(unsigned char is_period, void* ptimer);

//删除定时器
unsigned int Bsp_Timer_Delete(unsigned char is_period, void* ptimer);

//定时器是否初始化
bool Bsp_Timer_Is_Init(unsigned char is_period, void* ptimer);

//定时器是否在运行
bool Bsp_Timer_Is_Running(unsigned char is_period, void* ptimer);

//定时器重新计数
unsigned int Bsp_Timer_Reload(unsigned char is_period, void* ptimer);

//创建线程 
int Bsp_Pthread_Create(Bsp_Thread_t *thread_t, const char *name, unsigned int stack_size, unsigned int priority, void *cb,void *arg);

//创建线程,任务栈在PSRAM中
int Bsp_Pthread_Create_Psram(Bsp_Thread_t *thread_t, const char *name, unsigned int stack_size, unsigned int priority, void *cb, void *arg);

//创建线程到指定CPU
int Bsp_Pthread_Create_Pinned_To_Core(Bsp_Thread_t *thread_t, const char *name, unsigned int stack_size, unsigned int priority, void *cb, void *arg, unsigned char id_num);

//创建线程到指定CPU,任务栈在PSRAM中(不含NVS/Flash操作任务适用)
int Bsp_Pthread_Create_Psram_Pinned_To_Core(Bsp_Thread_t *thread_t, const char *name, unsigned int stack_size, unsigned int priority, void *cb, void *arg, unsigned char id_num);

//创建线程 
int Bsp_Pthread_Delete(Bsp_Thread_t *thread_t);

//删除PSRAM线程
int Bsp_Pthread_Psram_Delete(Bsp_Thread_t *thread_t);

//创建消息队列
uint32_t Bsp_Msg_Queue_Create(Bsp_Queue_t *Bsp_Queue, uint16_t queue_len, uint32_t msg_size);

//创建PSRAM消息队列
uint32_t Bsp_Msg_Queue_Create_Psram(Bsp_Queue_t *Bsp_Queue, uint16_t queue_len, uint32_t msg_size);

//删除消息队列
uint32_t Bsp_Msg_Queue_Delete(Bsp_Queue_t *Bsp_Queue);

//删除PSRAM消息队列
uint32_t Bsp_Msg_Queue_Delete_Psram(Bsp_Queue_t *Bsp_Queue);

//发送消息
uint32_t Bsp_Msg_Queue_Send(Bsp_Queue_t *Bsp_Queue, void *msg, uint32_t msg_size, uint32_t timeout_ms);

//发送PSRAM消息队列消息
uint32_t Bsp_Msg_Queue_Send_Psram(Bsp_Queue_t *Bsp_Queue, void *msg, uint32_t msg_size, uint32_t timeout_ms);

//中断函数中发送消息
uint32_t Bsp_Msg_Queue_Send_From_Isr(Bsp_Queue_t *Bsp_Queue, void *msg);

//接收消息
uint32_t Bsp_Msg_Queue_Wait(Bsp_Queue_t *Bsp_Queue, void *msg, uint32_t *msg_size, uint32_t timeout_ms);

//接收PSRAM消息队列消息
uint32_t Bsp_Msg_Queue_Wait_Psram(Bsp_Queue_t *Bsp_Queue, void *msg, uint32_t *msg_size, uint32_t timeout_ms);

//消息队列的消息数量
uint32_t Bsp_Msg_Queue_Get_Msg_Num(Bsp_Queue_t *Bsp_Queue);

//PSRAM消息队列的消息数量
uint32_t Bsp_Msg_Queue_Get_Msg_Num_Psram(Bsp_Queue_t *Bsp_Queue);

//消息队列是否为空
uint32_t Bsp_Msg_Queue_Is_Empty(Bsp_Queue_t *Bsp_Queue);

//创建事件组
uint32_t Bsp_Event_Create(Bsp_Event_t *Bsp_Event);

//删除事件组
uint32_t Bsp_Event_Delete(Bsp_Event_t Bsp_Event);

//发送事件
uint32_t Bsp_Event_Send(Bsp_Event_t Bsp_Event, uint32_t event_bits);

//等待事件
uint32_t Bsp_Event_Wait(Bsp_Event_t Bsp_Event, uint32_t mask, uint32_t *event_bits, uint32_t timeout, uint32_t flag);

//清除事件
uint32_t Bsp_Event_Clear(Bsp_Event_t Bsp_Event, uint32_t event_bits);

//创建互斥锁
uint32_t Bsp_Mutex_Init(Bsp_Mutex_t *mutex);

//创建PSRAM互斥锁
uint32_t Bsp_Mutex_Init_Psram(Bsp_Mutex_t *mutex);

//删除互斥锁
uint32_t Bsp_Mutex_Destroy(Bsp_Mutex_t *mutex);

//删除PSRAM互斥锁
uint32_t Bsp_Mutex_Destroy_Psram(Bsp_Mutex_t *mutex);

//上锁
uint32_t Bsp_Mutex_Lock(Bsp_Mutex_t *mutex, uint32_t timeout_ms);

//PSRAM互斥锁上锁
uint32_t Bsp_Mutex_Lock_Psram(Bsp_Mutex_t *mutex, uint32_t timeout_ms);

//释放锁
uint32_t Bsp_Mutex_Unlock(Bsp_Mutex_t *mutex);

//释放PSRAM互斥锁
uint32_t Bsp_Mutex_Unlock_Psram(Bsp_Mutex_t *mutex);

//创建信号量
uint32_t Bsp_Semaphore_Init(Bsp_Sema_t *sema, int maxcount);

//创建PSRAM信号量
uint32_t Bsp_Semaphore_Init_Psram(Bsp_Sema_t *sema, int maxcount);

//销毁信号量
uint32_t Bsp_Semaphore_Deinit(Bsp_Sema_t *sema);

//销毁PSRAM信号量
uint32_t Bsp_Semaphore_Deinit_Psram(Bsp_Sema_t *sema);

//发送信号量
uint32_t Bsp_Set_Semaphore(Bsp_Sema_t *sema);

//发送PSRAM信号量
uint32_t Bsp_Set_Semaphore_Psram(Bsp_Sema_t *sema);

//获取信号量
uint32_t Bsp_Get_Semaphore(Bsp_Sema_t *sema, uint32_t timeout_ms);

//获取PSRAM信号量
uint32_t Bsp_Get_Semaphore_Psram(Bsp_Sema_t *sema, uint32_t timeout_ms);

//输出系统内存信息
void Bsp_Debug_Heap_Info(void);

//打印本地时间 
void Bsp_Print_Local_Time(void);

//输出系统内部堆栈信息
void Bsp_Debug_Sram_Heap_Info(void);

//输出系统外部堆栈信息
void Bsp_Debug_Psram_Heap_Info(void);

//获取芯片的设备ID
unsigned int Bsp_Get_Device_Id(void);

#ifdef __cplusplus
}
#endif
