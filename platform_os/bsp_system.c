//bsp_system.c
#include <stdio.h>

#include "bsp_system.h"

#include "bsp_log.h"

#include <sys/time.h>

#include "esp_heap_caps.h"
#include "esp_rom_sys.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/idf_additions.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/timers.h"

extern void *calloc(size_t nmemb, size_t size);

#define BSP_TIMEZONE_ID_LEN        48
#define BSP_TIMEZONE_POSIX_LEN     80
#define BSP_MIN_VALID_UNIX_TS      1700000000ULL

static int32_t Zone_Offset = 0;
static int32_t Zone_Fallback_Offset = 0;
static bool Zone_Rule_Active = false;
static char Zone_Id[BSP_TIMEZONE_ID_LEN] = "UTC";
static char Zone_Posix[BSP_TIMEZONE_POSIX_LEN] = "UTC0";

typedef struct
{
    QueueHandle_t handle;
    StaticQueue_t *queue_buffer;
    uint8_t *storage;
} Bsp_Psram_Queue_Ctx_t;

typedef struct
{
    SemaphoreHandle_t handle;
    StaticSemaphore_t *buffer;
} Bsp_Psram_Sync_Ctx_t;

static void *Bsp_Psram_Calloc_Prefer(size_t num, size_t size)
{
    void *ptr = heap_caps_calloc(num, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (ptr == NULL)
    {
        ptr = heap_caps_calloc(num, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    return ptr;
}

static void Bsp_Psram_Sync_Free(Bsp_Psram_Sync_Ctx_t *sync)
{
    if (sync == NULL)
    {
        return;
    }
    if (sync->handle != NULL)
    {
        vSemaphoreDelete(sync->handle);
    }
    heap_caps_free(sync->buffer);
    heap_caps_free(sync);
}

static int Bsp_Time_Apply_Tz_Value(const char *tz_value)
{
    if ((tz_value == NULL) || (tz_value[0] == '\0'))
    {
        return -1;
    }

    if (setenv("TZ", tz_value, 1) != 0)
    {
        Log_Error("set TZ failed, TZ=%s\r\n", tz_value);
        return -1;
    }

    tzset();
    return 0;
}

static int32_t Bsp_Time_Calculate_Effective_Offset(int32_t fallback_offset)
{
    time_t now = time(NULL);
    struct tm utc_tm;
    time_t utc_as_local;
    double diff;

    if ((uint64_t)now < BSP_MIN_VALID_UNIX_TS)
    {
        return fallback_offset;
    }

    if (gmtime_r(&now, &utc_tm) == NULL)
    {
        return fallback_offset;
    }

    utc_tm.tm_isdst = -1;
    utc_as_local = mktime(&utc_tm);
    if (utc_as_local == (time_t)-1)
    {
        return fallback_offset;
    }

    diff = difftime(now, utc_as_local);
    if ((diff > (double)INT32_MAX) || (diff < (double)INT32_MIN))
    {
        return fallback_offset;
    }

    return (int32_t)diff;
}

static void Bsp_Time_Apply_Fixed_Offset(int32_t zone_offset)
{
    char tz_value[32] = {0};
    int32_t abs_offset = (zone_offset >= 0) ? zone_offset : -zone_offset;
    int hour = abs_offset / 3600;
    int minute = (abs_offset % 3600) / 60;
    int second = abs_offset % 60;
    char sign = (zone_offset >= 0) ? '-' : '+';

    if (zone_offset == 0)
    {
        strncpy(tz_value, "UTC0", sizeof(tz_value) - 1);
    }
    else
    {
        snprintf(tz_value,
                 sizeof(tz_value),
                 "UTC%c%02d:%02d:%02d",
                 sign,
                 hour,
                 minute,
                 second);
    }

    if (Bsp_Time_Apply_Tz_Value(tz_value) != 0)
    {
        Log_Error("apply fixed timezone failed, zone_offset=%ld\r\n", (long)zone_offset);
        return;
    }

    Zone_Rule_Active = false;
    Zone_Offset = zone_offset;
    Zone_Fallback_Offset = zone_offset;
    strncpy(Zone_Id, tz_value, sizeof(Zone_Id) - 1);
    Zone_Id[sizeof(Zone_Id) - 1] = '\0';
    strncpy(Zone_Posix, tz_value, sizeof(Zone_Posix) - 1);
    Zone_Posix[sizeof(Zone_Posix) - 1] = '\0';
    Log_Info("timezone applied, zone_offset=%ld, TZ=%s\r\n", (long)zone_offset, tz_value);
}


/**
*@名称        Bsp_Block_Delay_Us
*@功能        阻塞微秒延时
*@参数        uint32_t nus
*@返回值   void
*@使用说明  
*/
void Bsp_Block_Delay_Us(uint32_t nus)
{
    esp_rom_delay_us(nus);
}

/**
*@名称        Bsp_Block_Delay_Ms
*@功能        阻塞毫秒延时
*@参数        uint32_t nms
*@返回值   void
*@使用说明  
*/
void Bsp_Block_Delay_Ms(uint32_t nms)
{
    Bsp_Block_Delay_Us(nms*1000);
}

/**
*@名称        Bsp_Sleep_Ms
*@功能        非阻塞毫秒延时
*@参数        uint32_t nms
*@返回值   void
*@使用说明  
*/
void Bsp_Sleep_Ms(uint32_t nms)
{
    vTaskDelay(nms/portTICK_PERIOD_MS);
}

/**
*@名称        Bsp_Get_Run_Time_Ms
*@功能        获取系统运行时间ms
*@参数        void
*@返回值   uint32_t
*@使用说明  
*/
uint32_t Bsp_Get_Run_Time_Ms(void)
{
    /* 单调运行时间（自启动起，ms）——用 esp_timer 而非 gettimeofday，
     * 免疫 NTP/墙钟跳变；调用方均为相对计时（去抖/超时/staleness/diff）。
     * 绝对墙钟请用 Bsp_Get_Time_Stamp_Ms。 */
    return (uint32_t)(esp_timer_get_time() / 1000LL);
}

/**
*@名称        Bsp_Get_Run_Time_Us
*@功能        获取系统运行时间us(单调)
*@参数        void
*@返回值   int64_t
*@使用说明  微秒级耗时测量;绝对墙钟请用 Bsp_Get_Time_Stamp_Ms
*/
int64_t Bsp_Get_Run_Time_Us(void)
{
    return esp_timer_get_time();
}

/**
*@名称        Bsp_Get_Time_Stamp_Ms
*@功能        获取系统时间戳ms
*@参数        void
*@返回值   uint64_t
*@使用说明  
*/
uint64_t Bsp_Get_Time_Stamp_Ms(void)
{
    struct timeval tv_now = {0, 0};
    gettimeofday(&tv_now, NULL);
    uint64_t time_ms = (uint64_t)tv_now.tv_sec * 1000L + (uint64_t)tv_now.tv_usec/1000L;
    return time_ms;
}



/**
*@名称        Bsp_At_Cmd_Init
*@功能        获取系统时间戳
*@参数        void
*@返回值   uint64_t
*@使用说明  
*/
uint64_t Bsp_Get_Time_Stamp(void)
{
    time_t now = time(NULL);
    return now;
}


/**
*@名称        Bsp_Set_Time_Stamp
*@功能        设置时间 
*@参数        unsigned int stamp
*@返回值   int
*@使用说明  
*/
int Bsp_Set_Time_Stamp(unsigned int stamp)
{
    Log_Debug("ts=%d\n", stamp);
    struct timeval set_time = {0};
    set_time.tv_sec = stamp;
    set_time.tv_usec = 0;
    settimeofday(&set_time,NULL);
    Log_Debug("current time stamp :%u\n", Bsp_Get_Time_Stamp());
    struct tm soc_tm;
    localtime_r((time_t *)&set_time.tv_sec, &soc_tm);
    Log_Debug("================%04d-%02d-%02d %02d:%02d:%02d====================\r\n", soc_tm.tm_year+1900, soc_tm.tm_mon+1, soc_tm.tm_mday,
               soc_tm.tm_hour, soc_tm.tm_min, soc_tm.tm_sec);
    return 0;
}


/**
*@名称        Bsp_Get_Zone_Offset
*@功能        获取时区偏移量，单位秒
*@参数        void
*@返回值   int32_t
*@使用说明  
*/
int32_t Bsp_Get_Zone_Offset(void)
{
    if (Zone_Rule_Active)
    {
        Zone_Offset = Bsp_Time_Calculate_Effective_Offset(Zone_Fallback_Offset);
    }

    return Zone_Offset;
}

/**
*@名称        Bsp_Set_Zone_Offset
*@功能        设置时区偏移量，单位秒
*@参数        int32_t value
*@返回值   void
*@使用说明  
*/
void Bsp_Set_Zone_Offset(int32_t value)
{
    Bsp_Time_Apply_Fixed_Offset(value);
}

/**
*@名称        Bsp_Set_Timezone_Rule
*@功能        设置POSIX时区规则，支持夏令时
*@参数        timezone_id IANA时区ID
*@参数        posix_tz POSIX TZ规则
*@参数        fallback_offset_sec 时间未同步时使用的标准偏移
*@返回值   void
*@使用说明
*/
void Bsp_Set_Timezone_Rule(const char *timezone_id, const char *posix_tz, int32_t fallback_offset_sec)
{
    if ((posix_tz == NULL) || (posix_tz[0] == '\0'))
    {
        Bsp_Set_Zone_Offset(fallback_offset_sec);
        return;
    }

    if (Bsp_Time_Apply_Tz_Value(posix_tz) != 0)
    {
        Log_Error("apply timezone rule failed, id=%s, TZ=%s\r\n",
                  (timezone_id != NULL) ? timezone_id : "null",
                  posix_tz);
        Bsp_Set_Zone_Offset(fallback_offset_sec);
        return;
    }

    Zone_Rule_Active = true;
    Zone_Fallback_Offset = fallback_offset_sec;
    Zone_Offset = Bsp_Time_Calculate_Effective_Offset(fallback_offset_sec);

    if ((timezone_id != NULL) && (timezone_id[0] != '\0'))
    {
        strncpy(Zone_Id, timezone_id, sizeof(Zone_Id) - 1);
        Zone_Id[sizeof(Zone_Id) - 1] = '\0';
    }
    else
    {
        strncpy(Zone_Id, posix_tz, sizeof(Zone_Id) - 1);
        Zone_Id[sizeof(Zone_Id) - 1] = '\0';
    }

    strncpy(Zone_Posix, posix_tz, sizeof(Zone_Posix) - 1);
    Zone_Posix[sizeof(Zone_Posix) - 1] = '\0';

    Log_Info("timezone rule applied, id=%s, offset=%ld, TZ=%s\r\n",
             Zone_Id,
             (long)Zone_Offset,
             Zone_Posix);
}

const char *Bsp_Get_Timezone_Id(void)
{
    return Zone_Id;
}

/**
*@名称        Bsp_Print_Local_Time
*@功能        打印本地时间 
*@参数        unsigned int stamp
*@返回值   int
*@使用说明  
*/
void Bsp_Print_Local_Time(void)
{
    struct timeval now_time = {0, 0};
    gettimeofday(&now_time, 0);
    struct tm soc_tm;
    localtime_r((time_t *)&now_time.tv_sec, &soc_tm);
    Log_Debug("================%04d-%02d-%02d %02d:%02d:%02d====================\r\n", soc_tm.tm_year+1900, soc_tm.tm_mon+1, soc_tm.tm_mday,
               soc_tm.tm_hour, soc_tm.tm_min, soc_tm.tm_sec);
}


/**
*@名称        Bsp_Cipher_Init
*@功能        安全模块初始化
*@参数        void
*@返回值   void
*@使用说明  TRNG KDF HASH AES RSA EFUSE CRC32
*/
void Bsp_Cipher_Init(void)
{
    
}

/**
*@名称        Bsp_At_Cmd_Init
*@功能        AT初始化
*@参数        void
*@返回值   void
*@使用说明  
*/
void Bsp_At_Cmd_Init(void)
{

}

/**
*@名称        Bsp_Mem_Malloc
*@功能        内存申请 
*@参数        unsigned int size
*@返回值   void *
*@使用说明  
*/
void *Bsp_Mem_Malloc(unsigned int size)
{
    return malloc(size);
}

/**
*@名称        Bsp_Mem_Free
*@功能        内存释放
*@参数        void * addr
*@返回值   void 
*@使用说明  
*/
void Bsp_Mem_Free(void *addr)
{
    if(addr)
    {
        free(addr);
        addr = 0;
    }
}

/**
*@名称        Bsp_Mem_Calloc
*@功能        内存申请并初始化为0
*@参数        size_t num, size_t size
*@返回值   void *
*@使用说明  
*/
void *Bsp_Mem_Calloc(size_t num, size_t size)
{
    return calloc(num, size);
}

/**
*@名称        Bsp_Mem_Realloc
*@功能        内存重新申请
*@参数        void *ptr, size_t size
*@返回值   void *
*@使用说明  
*/
void *Bsp_Mem_Realloc(void *ptr, size_t size)
{
    if(ptr)
    {
        return realloc(ptr, size);
    }
    else
    {
        return calloc(1, size);
    }
}

/**
*@名称        Bsp_Psram_Malloc
*@功能        外部内存申请 
*@参数        unsigned int size
*@返回值   void *
*@使用说明  
*/
void *Bsp_Psram_Malloc(unsigned int size)
{
    return heap_caps_malloc(size, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
}

/**
*@名称        Bsp_Psram_Zalloc
*@功能        外部内存申请并清0 
*@参数        unsigned int size
*@返回值   void *
*@使用说明  
*/
void *Bsp_Psram_Zalloc(unsigned int size)
{
    return heap_caps_calloc(1, size, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
}

/**
*@名称        Bsp_Psram_Calloc
*@功能        外部内存申请并初始化为0 
*@参数        size_t num, size_t size
*@返回值   void *
*@使用说明  
*/
void *Bsp_Psram_Calloc(size_t num, size_t size)
{
    return heap_caps_calloc(num, size, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
}

/**
*@名称        Bsp_Psram_Realloc
*@功能        内存重新申请
*@参数        void *ptr, size_t size
*@返回值   void *
*@使用说明  
*/
void *Bsp_Psram_Realloc(void *ptr, size_t size)
{
    return heap_caps_realloc(ptr, size, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
}


/**
*@名称        Bsp_Psram_Free
*@功能        外部内存释放
*@参数        void * addr
*@返回值   void 
*@使用说明  
*/
void Bsp_Psram_Free(void *addr)
{
    if(addr)
    {
        heap_caps_free(addr);
        addr = 0;
    }
}

/**
*@名称        Bsp_Random
*@功能        获取随机数
*@参数        void
*@返回值   uint32_t
*@使用说明  
*/
uint32_t Bsp_Random(void)
{
    return (uint32_t)rand();
}


typedef void (*_Entity_Timer_Cb_t)(void *arg, void *reserved);

typedef struct
{
    _Entity_Timer_Cb_t user_cb;
    void *user_arg;
} _Timer_Ctx_t;

static void _timer_trampoline(TimerHandle_t xTimer)
{
    _Timer_Ctx_t *ctx = (_Timer_Ctx_t *)pvTimerGetTimerID(xTimer);
    if (ctx && ctx->user_cb)
    {
        ctx->user_cb(ctx->user_arg, NULL);
    }
}

/**
*@名称        Bsp_Timer_Create
*@功能        创建定时器
*@参数        unsigned char is_period, unsigned int interval,void *cb,void *arg
*@返回值   void*
*@使用说明  单次定时器
*/
void* Bsp_Timer_Create(unsigned char is_period, unsigned int interval, void *cb, void *arg)
{
    _Timer_Ctx_t *ctx = (_Timer_Ctx_t *)heap_caps_malloc(sizeof(_Timer_Ctx_t),
                                                          MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (ctx == NULL)
    {
        Log_Error("Bsp_Timer_Create: ctx alloc 失败\r\n");
        return NULL;
    }
    ctx->user_cb  = (_Entity_Timer_Cb_t)cb;
    ctx->user_arg = arg;

    TimerHandle_t timer = xTimerCreate("entity_tmr", pdMS_TO_TICKS(interval), is_period,
                                        ctx, _timer_trampoline);
    if (timer == NULL)
    {
        Log_Error("Bsp_Timer_Create: xTimerCreate 失败\r\n");
        heap_caps_free(ctx);
        return NULL;
    }
    return timer;
}

/**
*@名称        Bsp_Timer_Start
*@功能        启动定时器
*@参数        unsigned char is_period, void* ptimer
*@返回值   unsigned int 
*@使用说明  论最大值为0xffffffff/1000000 约 4294 秒
*/
unsigned int Bsp_Timer_Start(unsigned char is_period, void* ptimer)
{
    BaseType_t xReturn = pdFAIL;
    xReturn = xTimerStart((TimerHandle_t)ptimer, 0);
    return xReturn==pdPASS ? 0 : -1;
}

/**
*@名称        Bsp_Timer_Stop
*@功能        停止定时器
*@参数        unsigned char is_period, void* ptimer
*@返回值    unsigned int
*@使用说明  
*/
unsigned int Bsp_Timer_Stop(unsigned char is_period, void* ptimer)
{
    BaseType_t xReturn = pdFAIL;
    xReturn = xTimerStop((TimerHandle_t)ptimer, 0);
    return xReturn==pdPASS ? 0 : -1;
}

/**
*@名称        Bsp_Timer_Delete
*@功能        删除定时器
*@参数        unsigned char is_period, void* ptimer
*@返回值   unsigned char
*@使用说明
*/
unsigned int Bsp_Timer_Delete(unsigned char is_period, void* ptimer)
{
    if (ptimer == NULL)
    {
        return -1;
    }
    TimerHandle_t timer = (TimerHandle_t)ptimer;
    _Timer_Ctx_t *ctx   = (_Timer_Ctx_t *)pvTimerGetTimerID(timer);

    xTimerStop(timer, pdMS_TO_TICKS(200));
    BaseType_t xReturn = xTimerDelete(timer, pdMS_TO_TICKS(200));

    heap_caps_free(ctx);
    return xReturn == pdPASS ? 0 : -1;
}

/**
*@名称        Bsp_Timer_Is_Init
*@功能        定时器是否初始化
*@参数        unsigned char is_period, void* ptimer
*@返回值   bool
*@使用说明  
*/
bool Bsp_Timer_Is_Init(unsigned char is_period, void* ptimer)
{
    return ptimer ? 1: 0;
}


/**
*@名称        Bsp_Timer_Is_Running
*@功能        定时器是否在运行
*@参数        unsigned char is_period, void* ptimer
*@返回值   bool
*@使用说明  
*/
bool Bsp_Timer_Is_Running(unsigned char is_period, void* ptimer)
{
    BaseType_t xReturn = pdFAIL;
    xReturn = xTimerIsTimerActive((TimerHandle_t)ptimer);
    return xReturn==pdPASS ? 1 : 0;
}


/**
*@名称        Bsp_Timer_Reload
*@功能        定时器重新计数
*@参数        unsigned char is_period, void* ptimer
*@返回值   unsigned int
*@使用说明  
*/
unsigned int Bsp_Timer_Reload(unsigned char is_period, void* ptimer)
{
    BaseType_t xReturn = pdFAIL;
    xReturn = xTimerReset((TimerHandle_t)ptimer, 0);
    return xReturn==pdPASS ? 0 : -1;
}

/**
*@名称        Bsp_Pthread_Create
*@功能        创建线程 
*@参数        Bsp_Thread_t *thread_t, char *name, unsigned int stack_size, unsigned int priority, void *cb,void *arg
*@返回值   int
*@使用说明  
*/
int Bsp_Pthread_Create(Bsp_Thread_t *thread_t, const char *name, unsigned int stack_size, unsigned int priority, void *cb, void *arg)
{
    BaseType_t xReturn = pdFAIL;
    xReturn = xTaskCreate((TaskFunction_t)cb, name, stack_size, arg, priority, (TaskHandle_t*)thread_t);
    if (xReturn != pdPASS) 
    {
        Log_Error("rtos_create_thread fail, ret:%d\r\n", xReturn);
        return -1;
    }
    Log_Debug("Bsp_Pthread_Create %s success\r\n", name);
    return 0;
}

/**
*@名称        Bsp_Pthread_Create_Psram
*@功能        创建线程,任务栈在PSRAM中
*@参数        Bsp_Thread_t *thread_t, char *name, unsigned int stack_size, unsigned int priority, void *cb,void *arg
*@返回值   int
*@使用说明  
*/
int Bsp_Pthread_Create_Psram(Bsp_Thread_t *thread_t, const char *name, unsigned int stack_size, unsigned int priority, void *cb, void *arg)
{
    BaseType_t xReturn = pdFAIL;
    xReturn = xTaskCreatePinnedToCoreWithCaps((TaskFunction_t)cb, name, stack_size, arg, priority, (TaskHandle_t*)thread_t, 0, MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if (xReturn != pdPASS) 
    {
        Log_Error("rtos_create_thread fail, ret:%d\r\n", xReturn);
        return -1;
    }
    Log_Debug("Bsp_Pthread_Create_Psram %s success\r\n", name);
    return 0;
}

/**
*@名称        Bsp_Pthread_Create_Pinned_To_Core
*@功能        创建线程到指定CPU
*@参数        Bsp_Thread_t *thread_t, char *name, unsigned int stack_size, unsigned int priority, void *cb,void *arg, unsigned char id_num
*@返回值   int
*@使用说明  
*/
int Bsp_Pthread_Create_Pinned_To_Core(Bsp_Thread_t *thread_t, const char *name, unsigned int stack_size, unsigned int priority, void *cb, void *arg, unsigned char id_num)
{
    BaseType_t xReturn = pdFAIL;
    xReturn = xTaskCreatePinnedToCore(cb, name, stack_size, arg, priority, (TaskHandle_t *)thread_t, id_num);
    if (xReturn != pdPASS) 
    {
        Log_Error("xTaskCreatePinnedToCore fail\r\n");
        return -1;
    }
    Log_Debug("xTaskCreatePinnedToCore success\r\n");
    return 0;
}

/**
*@名称        Bsp_Pthread_Create_Psram_Pinned_To_Core
*@功能        创建线程到指定CPU，任务栈分配在 PSRAM（不涉及 NVS/Flash 的任务适用）
*@参数        Bsp_Thread_t *thread_t, char *name, unsigned int stack_size, unsigned int priority, void *cb,void *arg, unsigned char id_num
*@返回值   int
*@使用说明  PSRAM 栈不可用于 NVS/Flash 写操作任务（Cache 关闭期间会崩溃）
*           Audio_Dac_Task / Audio_Decoder_Task 等不触发 Flash 操作的任务可安全使用
*/
int Bsp_Pthread_Create_Psram_Pinned_To_Core(Bsp_Thread_t *thread_t, const char *name, unsigned int stack_size, unsigned int priority, void *cb, void *arg, unsigned char id_num)
{
    BaseType_t xReturn = pdFAIL;
    xReturn = xTaskCreatePinnedToCoreWithCaps((TaskFunction_t)cb, name, stack_size, arg, priority, (TaskHandle_t*)thread_t, id_num, MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if (xReturn != pdPASS)
    {
        Log_Error("xTaskCreatePinnedToCoreWithCaps(PSRAM) fail\r\n");
        return -1;
    }
    Log_Debug("Bsp_Pthread_Create_Psram_Pinned_To_Core %s success\r\n", name);
    return 0;
}

/* ── 统一入口(_Ex):dispatch 到既有雙胞胎,行為逐位元不變 ──────────────────── */

/**
*@名称        Bsp_Pthread_Create_Ex
*@功能        统一线程创建入口(core=-1 不绑核;region 选栈内存区)
*@使用说明    PSRAM 栈不可用于 NVS/Flash 写任务(Cache 关闭期会崩)
*/
int Bsp_Pthread_Create_Ex(Bsp_Thread_t *thread_t, const char *name, unsigned int stack_size,
                          unsigned int priority, void *cb, void *arg,
                          int core, Bsp_Mem_Region_e region)
{
    if (region == BSP_MEM_PSRAM)
    {
        if (core == BSP_CORE_NO_AFFINITY)
        {
            return Bsp_Pthread_Create_Psram(thread_t, name, stack_size, priority, cb, arg);
        }
        return Bsp_Pthread_Create_Psram_Pinned_To_Core(thread_t, name, stack_size, priority, cb, arg, (unsigned char)core);
    }
    if (core == BSP_CORE_NO_AFFINITY)
    {
        return Bsp_Pthread_Create(thread_t, name, stack_size, priority, cb, arg);
    }
    return Bsp_Pthread_Create_Pinned_To_Core(thread_t, name, stack_size, priority, cb, arg, (unsigned char)core);
}

/**
*@名称        Bsp_Msg_Queue_Create_Ex
*@功能        统一消息队列创建入口(region 选队列存储区)
*/
uint32_t Bsp_Msg_Queue_Create_Ex(Bsp_Queue_t *Bsp_Queue, uint16_t queue_len, uint32_t msg_size,
                                 Bsp_Mem_Region_e region)
{
    return (region == BSP_MEM_PSRAM)
        ? Bsp_Msg_Queue_Create_Psram(Bsp_Queue, queue_len, msg_size)
        : Bsp_Msg_Queue_Create(Bsp_Queue, queue_len, msg_size);
}

/**
*@名称        Bsp_Semaphore_Init_Ex
*@功能        统一信号量创建入口(region 选对象存储区)
*/
uint32_t Bsp_Semaphore_Init_Ex(Bsp_Sema_t *sema, int maxcount, Bsp_Mem_Region_e region)
{
    return (region == BSP_MEM_PSRAM)
        ? Bsp_Semaphore_Init_Psram(sema, maxcount)
        : Bsp_Semaphore_Init(sema, maxcount);
}

/**
*@名称        Bsp_Mutex_Init_Ex
*@功能        统一互斥锁创建入口(region 选对象存储区)
*/
uint32_t Bsp_Mutex_Init_Ex(Bsp_Mutex_t *mutex, Bsp_Mem_Region_e region)
{
    return (region == BSP_MEM_PSRAM)
        ? Bsp_Mutex_Init_Psram(mutex)
        : Bsp_Mutex_Init(mutex);
}

/**
*@名称        Bsp_Pthread_Delete
*@功能        删除线程 
*@参数        Bsp_Thread_t *thread_t
*@返回值   int
*@使用说明  
*/
int Bsp_Pthread_Delete(Bsp_Thread_t *thread_t)
{
    vTaskDelete(*((TaskHandle_t*)thread_t));
    return 0;
}

/**
*@名称        Bsp_Pthread_Psram_Delete
*@功能        删除PSRAM线程 
*@参数        Bsp_Thread_t *thread_t
*@返回值   int
*@使用说明  
*/
int Bsp_Pthread_Psram_Delete(Bsp_Thread_t *thread_t)
{
    vTaskDeleteWithCaps(*((TaskHandle_t*)thread_t));
    return 0;
}

/**
*@名称        Bsp_Msg_Queue_Create
*@功能        创建消息队列
*@参数        Bsp_Queue_t *Bsp_Queue,uint16_t queue_len, uint32_t msg_size
*@返回值   uint32_t
*@使用说明  0-成功
*/
uint32_t Bsp_Msg_Queue_Create(Bsp_Queue_t *Bsp_Queue, uint16_t queue_len, uint32_t msg_size)
{
    QueueHandle_t *pqueue = (QueueHandle_t *)Bsp_Queue;
    *pqueue = xQueueCreate(queue_len, msg_size);
    return *pqueue ? 0 : -1;
}

/**
*@名称        Bsp_Msg_Queue_Create_Psram
*@功能        创建PSRAM消息队列
*@参数        Bsp_Queue_t *Bsp_Queue,uint16_t queue_len, uint32_t msg_size
*@返回值   uint32_t
*@使用说明  0-成功
*/
uint32_t Bsp_Msg_Queue_Create_Psram(Bsp_Queue_t *Bsp_Queue, uint16_t queue_len, uint32_t msg_size)
{
    if (Bsp_Queue == NULL || queue_len == 0 || msg_size == 0)
    {
        return -1;
    }

    size_t storage_size = (size_t)queue_len * (size_t)msg_size;
    if (storage_size / msg_size != queue_len)
    {
        return -1;
    }

    Bsp_Psram_Queue_Ctx_t *ctx = (Bsp_Psram_Queue_Ctx_t *)Bsp_Psram_Calloc_Prefer(1, sizeof(Bsp_Psram_Queue_Ctx_t));
    if (ctx == NULL)
    {
        return -1;
    }

    ctx->queue_buffer = (StaticQueue_t *)Bsp_Psram_Calloc_Prefer(1, sizeof(StaticQueue_t));
    ctx->storage = (uint8_t *)Bsp_Psram_Calloc_Prefer(1, storage_size);
    if (ctx->queue_buffer == NULL || ctx->storage == NULL)
    {
        heap_caps_free(ctx->storage);
        heap_caps_free(ctx->queue_buffer);
        heap_caps_free(ctx);
        return -1;
    }

    ctx->handle = xQueueCreateStatic(queue_len, msg_size, ctx->storage, ctx->queue_buffer);
    if (ctx->handle == NULL)
    {
        heap_caps_free(ctx->storage);
        heap_caps_free(ctx->queue_buffer);
        heap_caps_free(ctx);
        return -1;
    }

    *Bsp_Queue = ctx;
    return 0;
}

/**
*@名称        Bsp_Msg_Queue_Delete
*@功能        删除消息队列
*@参数        Bsp_Queue_t *Bsp_Queue
*@返回值   uint32_t
*@使用说明  
*/
uint32_t Bsp_Msg_Queue_Delete(Bsp_Queue_t *Bsp_Queue)
{
    QueueHandle_t *pqueue = (QueueHandle_t *)Bsp_Queue;
    if(*pqueue ==  NULL)
        return -1;
    vQueueDelete(*pqueue);
    return 0;
}

/**
*@名称        Bsp_Msg_Queue_Delete_Psram
*@功能        删除PSRAM消息队列
*@参数        Bsp_Queue_t *Bsp_Queue
*@返回值   uint32_t
*@使用说明
*/
uint32_t Bsp_Msg_Queue_Delete_Psram(Bsp_Queue_t *Bsp_Queue)
{
    if (Bsp_Queue == NULL || *Bsp_Queue == NULL)
    {
        return -1;
    }

    Bsp_Psram_Queue_Ctx_t *ctx = (Bsp_Psram_Queue_Ctx_t *)*Bsp_Queue;
    if (ctx->handle != NULL)
    {
        vQueueDelete(ctx->handle);
    }
    heap_caps_free(ctx->storage);
    heap_caps_free(ctx->queue_buffer);
    heap_caps_free(ctx);
    *Bsp_Queue = NULL;
    return 0;
}

/**
*@名称        Bsp_Msg_Queue_Send
*@功能        发送消息
*@参数        Bsp_Queue_t *Bsp_Queue, void *msg, uint32_t msg_size, uint32_t timeout_ms
*@返回值   uint32_t
*@使用说明  
*/
uint32_t Bsp_Msg_Queue_Send(Bsp_Queue_t *Bsp_Queue, void *msg, uint32_t msg_size, uint32_t timeout_ms)
{
    BaseType_t xReturn = pdFAIL;
    QueueHandle_t *pqueue = (QueueHandle_t *)Bsp_Queue;
    if(*pqueue ==  NULL)
        return -1;
    xReturn = xQueueSend(*pqueue,  msg, timeout_ms);
    return xReturn==pdPASS ? 0 : -1;
}

uint32_t Bsp_Msg_Queue_Send_Psram(Bsp_Queue_t *Bsp_Queue, void *msg, uint32_t msg_size, uint32_t timeout_ms)
{
    (void)msg_size;
    if (Bsp_Queue == NULL || *Bsp_Queue == NULL)
    {
        return -1;
    }

    Bsp_Psram_Queue_Ctx_t *ctx = (Bsp_Psram_Queue_Ctx_t *)*Bsp_Queue;
    return xQueueSend(ctx->handle, msg, timeout_ms) == pdPASS ? 0 : -1;
}


/**
*@名称        Bsp_Msg_Queue_Send_From_Isr
*@功能        中断函数中发送消息
*@参数        Bsp_Queue_t *Bsp_Queue, void *msg
*@返回值   uint32_t
*@使用说明  0-成功
*/
uint32_t Bsp_Msg_Queue_Send_From_Isr(Bsp_Queue_t *Bsp_Queue, void *msg)
{
    BaseType_t xReturn = pdFAIL;
    BaseType_t high_task_wakeup = pdFALSE;
    QueueHandle_t *pqueue = (QueueHandle_t *)Bsp_Queue;
    if(*pqueue ==  NULL)
        return -1;
    xReturn = xQueueSendFromISR(*pqueue, msg, &high_task_wakeup);
    if ((xReturn == pdPASS) && (high_task_wakeup == pdTRUE))
    {
        portYIELD_FROM_ISR();
    }
    return xReturn == pdPASS ? 0 : -1;
}
  
    

/**
*@名称        Bsp_Msg_Queue_Wait
*@功能        接收消息
*@参数        Bsp_Queue_t *Bsp_Queue, void *msg, uint32_t *msg_size, uint32_t timeout_ms
*@返回值   uint32_t
*@使用说明  
*/
uint32_t Bsp_Msg_Queue_Wait(Bsp_Queue_t *Bsp_Queue, void *msg, uint32_t *msg_size, uint32_t timeout_ms)
{
    BaseType_t xReturn = pdFAIL;
    QueueHandle_t *pqueue = (QueueHandle_t *)Bsp_Queue;
    if(*pqueue ==  NULL)
        return -1;
    xReturn = xQueueReceive(*pqueue,  msg, timeout_ms);
    return xReturn==pdPASS ? 0 : -1;
}

uint32_t Bsp_Msg_Queue_Wait_Psram(Bsp_Queue_t *Bsp_Queue, void *msg, uint32_t *msg_size, uint32_t timeout_ms)
{
    (void)msg_size;
    if (Bsp_Queue == NULL || *Bsp_Queue == NULL)
    {
        return -1;
    }

    Bsp_Psram_Queue_Ctx_t *ctx = (Bsp_Psram_Queue_Ctx_t *)*Bsp_Queue;
    return xQueueReceive(ctx->handle, msg, timeout_ms) == pdPASS ? 0 : -1;
}

/**
*@名称        Bsp_Msg_Queue_Get_Msg_Num
*@功能        消息队列的消息数量
*@参数        Bsp_Queue_t *Bsp_Queue
*@返回值   uint32_t 
*@使用说明  
*/
uint32_t Bsp_Msg_Queue_Get_Msg_Num(Bsp_Queue_t *Bsp_Queue)
{
    QueueHandle_t *pqueue = (QueueHandle_t *)Bsp_Queue;
    if(*pqueue ==  NULL)
        return -1;
    return uxQueueMessagesWaiting(*pqueue);
}

uint32_t Bsp_Msg_Queue_Get_Msg_Num_Psram(Bsp_Queue_t *Bsp_Queue)
{
    if (Bsp_Queue == NULL || *Bsp_Queue == NULL)
    {
        return -1;
    }

    Bsp_Psram_Queue_Ctx_t *ctx = (Bsp_Psram_Queue_Ctx_t *)*Bsp_Queue;
    return uxQueueMessagesWaiting(ctx->handle);
}

/**
*@名称        Bsp_Msg_Queue_Is_Empty
*@功能        消息队列是否为空
*@参数        Bsp_Queue_t *Bsp_Queue
*@返回值   uint32_t 
*@使用说明  
*/
uint32_t Bsp_Msg_Queue_Is_Empty(Bsp_Queue_t *Bsp_Queue)
{
    QueueHandle_t *pqueue = (QueueHandle_t *)Bsp_Queue;
    if(*pqueue ==  NULL)
        return -1;
    return !Bsp_Msg_Queue_Get_Msg_Num((Bsp_Queue_t *)pqueue);
}

/**
*@名称        Bsp_Event_Create
*@功能        创建事件组
*@参数        Bsp_Event_t *Bsp_Event
*@返回值   uint32_t 0-成功
*@使用说明 
*/
uint32_t Bsp_Event_Create(Bsp_Event_t *Bsp_Event)
{
    EventGroupHandle_t pevent = xEventGroupCreate();
    *((EventGroupHandle_t *)Bsp_Event) = pevent;
    return pevent ? 0 : -1;
}

/**
*@名称        Bsp_Event_Delete
*@功能        删除事件组
*@参数        Bsp_Event_t Bsp_Event
*@返回值   uint32_t 0-成功
*@使用说明  
*/
uint32_t Bsp_Event_Delete(Bsp_Event_t Bsp_Event)
{
    return 0;//aich_event_delete((uint32_t)Bsp_Event);
}

/**
*@名称        Bsp_Event_Send
*@功能        发送事件
*@参数        Bsp_Event_t Bsp_Event, uint32_t event_bits
*@返回值   uint32_t 0-成功
*@使用说明  bits 0-23.
*/
uint32_t Bsp_Event_Send(Bsp_Event_t Bsp_Event, uint32_t event_bits)
{
    return 0;//aich_event_send((uint32_t)Bsp_Event, event_bits);
}

/**
*@名称        Bsp_Event_Wait
*@功能        等待事件
*@参数        Bsp_Event_t Bsp_Event, uint32_t mask, uint32_t *event_bits, uint32_t timeout, uint32_t flag
*@返回值   uint32_t 0-成功
*@使用说明  bits 0-23.
*/
uint32_t Bsp_Event_Wait(Bsp_Event_t Bsp_Event, uint32_t mask, uint32_t *event_bits, uint32_t timeout, uint32_t flag)
{
    return 0;//aich_event_wait((uint32_t)Bsp_Event, mask, event_bits, timeout, flag);
}

/**
*@名称        Bsp_Event_Clear
*@功能        清除事件
*@参数        Bsp_Event_t Bsp_Event, uint32_t event_bits
*@返回值   uint32_t 0-成功
*@使用说明  bits 0-23.
*/
uint32_t Bsp_Event_Clear(Bsp_Event_t Bsp_Event, uint32_t event_bits)
{
    return 0;//aich_event_clear((uint32_t)Bsp_Event, event_bits);
}

/**
*@名称        Bsp_Mutex_Init
*@功能        创建互斥锁
*@参数        Bsp_Mutex_t *bsp_mutex
*@返回值   uint32_t 0-成功
*@使用说明 
*/
uint32_t Bsp_Mutex_Init(Bsp_Mutex_t *bsp_mutex)
{
    if(!bsp_mutex)
    {
        return -1;
    } 
    SemaphoreHandle_t *pmutex = (SemaphoreHandle_t *)bsp_mutex;
    *pmutex = xSemaphoreCreateMutex();
    return *pmutex ? 0 : -1;
}

uint32_t Bsp_Mutex_Init_Psram(Bsp_Mutex_t *bsp_mutex)
{
    if (bsp_mutex == NULL)
    {
        return -1;
    }

    Bsp_Psram_Sync_Ctx_t *ctx = (Bsp_Psram_Sync_Ctx_t *)Bsp_Psram_Calloc_Prefer(1, sizeof(Bsp_Psram_Sync_Ctx_t));
    if (ctx == NULL)
    {
        return -1;
    }

    ctx->buffer = (StaticSemaphore_t *)Bsp_Psram_Calloc_Prefer(1, sizeof(StaticSemaphore_t));
    if (ctx->buffer == NULL)
    {
        heap_caps_free(ctx);
        return -1;
    }

    ctx->handle = xSemaphoreCreateMutexStatic(ctx->buffer);
    if (ctx->handle == NULL)
    {
        Bsp_Psram_Sync_Free(ctx);
        return -1;
    }

    *bsp_mutex = ctx;
    return 0;
}

/**
*@名称        Bsp_Mutex_Destroy
*@功能        删除互斥锁
*@参数        Bsp_Mutex_t *bsp_mutex
*@返回值   uint32_t 0-成功
*@使用说明  
*/
uint32_t Bsp_Mutex_Destroy(Bsp_Mutex_t *bsp_mutex)
{
    SemaphoreHandle_t *pmutex = (SemaphoreHandle_t *)bsp_mutex;
    if(!pmutex || *pmutex == 0)
    {
        return -1;
    } 
    vSemaphoreDelete(*pmutex);
    return 0;
}

uint32_t Bsp_Mutex_Destroy_Psram(Bsp_Mutex_t *bsp_mutex)
{
    if (bsp_mutex == NULL || *bsp_mutex == NULL)
    {
        return -1;
    }

    Bsp_Psram_Sync_Free((Bsp_Psram_Sync_Ctx_t *)*bsp_mutex);
    *bsp_mutex = NULL;
    return 0;
}

/**
*@名称        Bsp_Mutex_Lock
*@功能        上锁
*@参数        Bsp_Mutex_t *bsp_mutex, uint32_t timeout_ms
*@返回值   uint32_t 0-成功
*@使用说明  
*/
uint32_t Bsp_Mutex_Lock(Bsp_Mutex_t *bsp_mutex, uint32_t timeout_ms)
{
    SemaphoreHandle_t *pmutex = (SemaphoreHandle_t *)bsp_mutex;
    if(!pmutex || *pmutex == 0)
    {
        return -1;
    }
    /* Bug 修正：timeout_ms 直接傳 xSemaphoreTake 會被當 ticks 使用，
     * 需透過 pdMS_TO_TICKS 轉換；0xffffffff 代表永遠等待，對應 portMAX_DELAY */
    TickType_t ticks = (timeout_ms == 0xffffffffU) ? portMAX_DELAY
                                                   : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTake(*pmutex, ticks) == pdPASS ? 0 : -1;
}

uint32_t Bsp_Mutex_Lock_Psram(Bsp_Mutex_t *bsp_mutex, uint32_t timeout_ms)
{
    if (bsp_mutex == NULL || *bsp_mutex == NULL)
    {
        return -1;
    }
    /* Bug 修正：同 Bsp_Mutex_Lock，timeout_ms → ticks */
    TickType_t ticks = (timeout_ms == 0xffffffffU) ? portMAX_DELAY
                                                   : pdMS_TO_TICKS(timeout_ms);
    Bsp_Psram_Sync_Ctx_t *ctx = (Bsp_Psram_Sync_Ctx_t *)*bsp_mutex;
    return xSemaphoreTake(ctx->handle, ticks) == pdPASS ? 0 : -1;
}

/**
*@名称        Bsp_Mutex_Unlock
*@功能        释放锁
*@参数        Bsp_Mutex_t *bsp_mutex
*@返回值   uint32_t 0-成功
*@使用说明  
*/
uint32_t Bsp_Mutex_Unlock(Bsp_Mutex_t *bsp_mutex)
{
    SemaphoreHandle_t *pmutex = (SemaphoreHandle_t *)bsp_mutex;
    if(!pmutex || *pmutex == 0)
    {
        return -1;
    } 
    return xSemaphoreGive(*pmutex)==pdPASS ? 0 : -1;
}

uint32_t Bsp_Mutex_Unlock_Psram(Bsp_Mutex_t *bsp_mutex)
{
    if (bsp_mutex == NULL || *bsp_mutex == NULL)
    {
        return -1;
    }

    Bsp_Psram_Sync_Ctx_t *ctx = (Bsp_Psram_Sync_Ctx_t *)*bsp_mutex;
    return xSemaphoreGive(ctx->handle) == pdPASS ? 0 : -1;
}


/**
*@名称        Bsp_Semaphore_Init
*@功能        创建信号量
*@参数        Bsp_Sema_t *bsp_sema, int maxcount
*@返回值   uint32_t 0-成功
*@使用说明 
*/
uint32_t Bsp_Semaphore_Init(Bsp_Sema_t *bsp_sema, int maxcount)
{
    if(!bsp_sema)
    {
        return -1;
    } 
    SemaphoreHandle_t *psema = (SemaphoreHandle_t *)bsp_sema;
    SemaphoreHandle_t xSemaphore = NULL;
    if(maxcount == 1)
    {
        // 二进制信号量 - 使用现代API
        xSemaphore = xSemaphoreCreateBinary();
    }
    else
    {
        // 计数信号量
        xSemaphore = xSemaphoreCreateCounting(maxcount, 0);
    }
    *psema = xSemaphore;
    return *psema ? 0 : -1;
}

uint32_t Bsp_Semaphore_Init_Psram(Bsp_Sema_t *bsp_sema, int maxcount)
{
    if (bsp_sema == NULL || maxcount <= 0)
    {
        return -1;
    }

    Bsp_Psram_Sync_Ctx_t *ctx = (Bsp_Psram_Sync_Ctx_t *)Bsp_Psram_Calloc_Prefer(1, sizeof(Bsp_Psram_Sync_Ctx_t));
    if (ctx == NULL)
    {
        return -1;
    }

    ctx->buffer = (StaticSemaphore_t *)Bsp_Psram_Calloc_Prefer(1, sizeof(StaticSemaphore_t));
    if (ctx->buffer == NULL)
    {
        heap_caps_free(ctx);
        return -1;
    }

    if (maxcount == 1)
    {
        ctx->handle = xSemaphoreCreateBinaryStatic(ctx->buffer);
    }
    else
    {
        ctx->handle = xSemaphoreCreateCountingStatic(maxcount, 0, ctx->buffer);
    }

    if (ctx->handle == NULL)
    {
        Bsp_Psram_Sync_Free(ctx);
        return -1;
    }

    *bsp_sema = ctx;
    return 0;
}


/**
*@名称        Bsp_Semaphore_Deinit
*@功能        销毁信号量
*@参数        Bsp_Sema_t *bsp_sema
*@返回值   uint32_t 0-成功
*@使用说明 
*/
uint32_t Bsp_Semaphore_Deinit(Bsp_Sema_t *bsp_sema)
{
    SemaphoreHandle_t *psema = (SemaphoreHandle_t *)bsp_sema;
    if(!psema || *psema == 0)
    {
        return -1;
    } 
    vSemaphoreDelete(*psema);
    return 0;
}

uint32_t Bsp_Semaphore_Deinit_Psram(Bsp_Sema_t *bsp_sema)
{
    if (bsp_sema == NULL || *bsp_sema == NULL)
    {
        return -1;
    }

    Bsp_Psram_Sync_Free((Bsp_Psram_Sync_Ctx_t *)*bsp_sema);
    *bsp_sema = NULL;
    return 0;
}

/**
*@名称        Bsp_Set_Semaphore
*@功能        发送信号量
*@参数        Bsp_Sema_t *bsp_sema
*@返回值   uint32_t 0-成功
*@使用说明 
*/
uint32_t Bsp_Set_Semaphore(Bsp_Sema_t *bsp_sema)
{
    SemaphoreHandle_t *psema = (SemaphoreHandle_t *)bsp_sema;
    if(!psema || *psema == 0)
    {
        return -1;
    } 
    return xSemaphoreGive(*psema)==pdPASS ? 0 : -1;
}

uint32_t Bsp_Set_Semaphore_Psram(Bsp_Sema_t *bsp_sema)
{
    if (bsp_sema == NULL || *bsp_sema == NULL)
    {
        return -1;
    }

    Bsp_Psram_Sync_Ctx_t *ctx = (Bsp_Psram_Sync_Ctx_t *)*bsp_sema;
    return xSemaphoreGive(ctx->handle) == pdPASS ? 0 : -1;
}
/**
*@名称        Bsp_Get_Semaphore
*@功能        获取信号量
*@参数        Bsp_Sema_t *bsp_sema,uint32_t timeout_ms
*@返回值   uint32_t 0-成功
*@使用说明 
*/
uint32_t Bsp_Get_Semaphore(Bsp_Sema_t *bsp_sema, uint32_t timeout_ms)
{
    SemaphoreHandle_t *psema = (SemaphoreHandle_t *)bsp_sema;
    if(!psema || *psema == 0)
    {
        return -1;
    }
    /* Bug 修正：timeout_ms → ticks；0xffffffff = portMAX_DELAY */
    TickType_t ticks = (timeout_ms == 0xffffffffU) ? portMAX_DELAY
                                                   : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTake(*psema, ticks) == pdPASS ? 0 : -1;
}

uint32_t Bsp_Get_Semaphore_Psram(Bsp_Sema_t *bsp_sema, uint32_t timeout_ms)
{
    if (bsp_sema == NULL || *bsp_sema == NULL)
    {
        return -1;
    }
    /* Bug 修正：timeout_ms → ticks；0xffffffff = portMAX_DELAY */
    TickType_t ticks = (timeout_ms == 0xffffffffU) ? portMAX_DELAY
                                                   : pdMS_TO_TICKS(timeout_ms);
    Bsp_Psram_Sync_Ctx_t *ctx = (Bsp_Psram_Sync_Ctx_t *)*bsp_sema;
    return xSemaphoreTake(ctx->handle, ticks) == pdPASS ? 0 : -1;
}

/**
*@名称        Bsp_Debug_Heap_Info
*@功能        输出系统内部堆栈信息
*@参数        void
*@返回值   void
*@使用说明 
*/
void Bsp_Debug_Sram_Heap_Info(void)
{
    uint32_t free_iram_size = esp_get_free_internal_heap_size();
    Log_Debug("IRAM Free memory: %" PRIu32 " bytes", free_iram_size);
}

/**
*@名称        Bsp_Debug_Psram_Heap_Info
*@功能        输出系统外部堆栈信息
*@参数        void
*@返回值   void
*@使用说明 
*/
void Bsp_Debug_Psram_Heap_Info(void)
{
    uint32_t free_total_heap_size = esp_get_free_heap_size();
    uint32_t free_iram_size = esp_get_free_internal_heap_size();
    uint32_t free_psram_size = free_total_heap_size - free_iram_size;
    Log_Debug("PSRAM Free memory: %" PRIu32 " bytes", free_psram_size);
}

/**
*@名称        Bsp_Debug_Heap_Info
*@功能        输出系统内存信息
*@参数        void
*@返回值   void
*@使用说明 
*/
void Bsp_Debug_Heap_Info(void)
{
    Bsp_Print_Local_Time();
#if 0
    uint32_t free_total_heap_size = esp_get_free_heap_size();
    uint32_t free_iram_size = esp_get_free_internal_heap_size();
    uint32_t free_psram_size = free_total_heap_size - free_iram_size;
    Log_Info("================Dynamic memory================\r\n");
    Log_Debug("DRAM Free memory: %" PRIu32 " bytes", free_iram_size);
    Log_Debug("PSRAM Free memory: %" PRIu32 " bytes", free_psram_size);
    Log_Info("==============================================\r\n");
#else
    uint32_t free_dram      = esp_get_free_internal_heap_size();      // 或 heap_caps_get_free_size(MALLOC_CAP_INTERNAL)
    uint32_t free_psram     = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    uint32_t free_iram      = heap_caps_get_free_size(MALLOC_CAP_IRAM_8BIT);
    uint32_t largest_dram   = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
    uint32_t free_dma       = heap_caps_get_free_size(MALLOC_CAP_DMA);
    uint32_t largest_dma    = heap_caps_get_largest_free_block(MALLOC_CAP_DMA);
    uint32_t largest_psram  = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);

    Log_Info("================Dynamic memory / 动态内存================");
    Log_Info("Heap EN: IRAM free=%" PRIu32 ", DRAM free=%" PRIu32 " largest=%" PRIu32 ", DMA free=%" PRIu32 " largest=%" PRIu32 ", PSRAM free=%" PRIu32 " largest=%" PRIu32 " bytes",
             free_iram, free_dram, largest_dram, free_dma, largest_dma, free_psram, largest_psram);
    Log_Info("Heap CN: IRAM空闲=%" PRIu32 "，内部DRAM空闲=%" PRIu32 " 最大块=%" PRIu32 "，DMA空闲=%" PRIu32 " 最大块=%" PRIu32 "，PSRAM空闲=%" PRIu32 " 最大块=%" PRIu32 " 字节",
             free_iram, free_dram, largest_dram, free_dma, largest_dma, free_psram, largest_psram);
    Log_Info("========================================================");
#endif

}

/**
*@名称        Bsp_Get_Device_Id
*@功能        获取芯片的设备ID
*@参数        void
*@返回值   unsigned int
*@使用说明 
*/
unsigned int Bsp_Get_Device_Id(void)
{
    return 0;
}
