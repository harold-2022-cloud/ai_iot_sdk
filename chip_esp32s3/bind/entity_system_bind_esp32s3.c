//entity_system_bind_esp32s3.c
// ESP32-S3 chip backend：把平台 (Bsp_*) 實作綁定到 entity_iot 的各域 Cbs 接口
// （Log / Countdown Timer / cJSON hooks / Network）。
// P6.2b 自產品側 entity_system_import_interface.c 下沉至此；產品策略性的 Dev_Status hook 留在產品側。
// 移植新芯片時新增 sdk/chip_<xxx>/bind/entity_system_bind_<xxx>.c，提供同名 Entity_System_Chip_Common_Init()。
#include "entity_iot_func.h"   /* Entity_System_Chip_Common_Init 宣告 */

#include "bsp_log.h"
#include "bsp_system.h"
#include "bsp_power.h"
#include "bsp_timer_countdown.h"
#include "bsp_network.h"

#include "entity_log.h"
#include "entity_timer_countdown.h"
#include "entity_network.h"
#include "cJSON.h"

#define IOT_MEM_USE_PSRAM

static void _Log_Info_Init(void)
{
    Entity_Log_Info_t info =
    {
        .Log_Level = ENTITY_LOG_LEVEL_DEBUG,
        .Print     = Bsp_Vprint,
    };
    Entity_Log_Info_Init(&info);
}

static void _Countdown_Timer_Func_Init(void)
{
    Entity_Timer_Func_t cbs =
    {
        .Get_System_Run_Time_Ms    = Bsp_Get_Run_Time_Ms,
        .System_Timer_Countdown_Ms = Bsp_System_Timer_Countdown_Ms,
        .System_Timer_Countdown    = Bsp_System_Timer_Countdown,
        .System_Timer_Remain       = Bsp_System_Timer_Remain,
        .System_Timer_Expired      = Bsp_System_Timer_Expired,
    };
    Entity_Timer_Func_Init(&cbs);
}

static void _Cjson_Mem_Func_Init(void)
{
#ifdef IOT_MEM_USE_PSRAM
    cJSON_Hooks hooks = { .malloc_fn = Bsp_Psram_Malloc, .free_fn = Bsp_Psram_Free };
#else
    cJSON_Hooks hooks = { .malloc_fn = Bsp_Mem_Malloc,   .free_fn = Bsp_Mem_Free };
#endif
    cJSON_InitHooks(&hooks);
}

static void _Network_Func_Init(void)
{
    Entity_Network_Func_t func =
    {
        .Tcp_Connect              = Bsp_Tcp_Connect,
        .Tcp_Disconnect           = Bsp_Tcp_Disconnect,
        .Tcp_Write                = Bsp_Tcp_Write,
        .Tcp_Read                 = Bsp_Tcp_Read,
        .Udp_Broadcast_Init       = Bsp_Udp_Broadcast_Init,
        .Udp_Broadcast_Send_Bytes = Bsp_Udp_Broadcast_Send_Bytes,
        .Udp_Server_Init          = Bsp_Udp_Server_Init,
        .Udp_Server_Recv_Bytes    = Bsp_Udp_Server_Recv_Bytes,
        .Udp_Server_Send_Bytes    = Bsp_Udp_Server_Send_Bytes,
        .Network_Socket_Close     = Bsp_Network_Socket_Close,
    };
    Entity_Network_Func_Init(&func);
}

// 把平台 (Bsp_*) 實作綁定到各域 Cbs。產品 aggregator 於系統初始化時呼叫。
void Entity_System_Chip_Common_Init(void)
{
    _Log_Info_Init();
    _Countdown_Timer_Func_Init();
    _Cjson_Mem_Func_Init();
    _Network_Func_Init();
}
