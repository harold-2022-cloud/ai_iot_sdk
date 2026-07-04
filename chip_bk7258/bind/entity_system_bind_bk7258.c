// entity_system_bind_bk7258.c
// BK7258 chip backend：把平台 (Bsp_*) 實作綁定到 entity_iot 的各域 Cbs 接口
// （Log / Countdown Timer / cJSON hooks / Network）。
// 結構鏡像 chip_esp32s3/bind/entity_system_bind_esp32s3.c；唯一差異：
//   (1) 不定義 IOT_MEM_USE_PSRAM → cJSON hooks 走 Bsp_Mem_Malloc/Bsp_Mem_Free
//       （BK7258 無 PSRAM；#ifdef 分支保留以維持視覺對稱，BK 自然走 #else）
//   (2) Entity_System_Chip_Common_Init() 加冪等守衛（防 Bind + 通用 init 雙呼叫）

#include "entity_iot_func.h"       /* Entity_System_Chip_Common_Init 宣告 */

#include "bsp_log.h"               /* Bsp_Vprint */
#include "bsp_system.h"            /* Bsp_Get_Run_Time_Ms, Bsp_Mem_Malloc/Free */
#include "bsp_timer_countdown.h"   /* Bsp_System_Timer_Countdown_Ms/Countdown/Remain/Expired（neutral） */
#include "bsp_network.h"           /* Bsp_Tcp_ / Bsp_Udp_ / Bsp_Network_Socket_Close (neutral) */

#include "entity_log.h"            /* Entity_Log_Info_t, Entity_Log_Info_Init */
#include "entity_timer_countdown.h"/* Entity_Timer_Func_t, Entity_Timer_Func_Init */
#include "entity_network.h"        /* Entity_Network_Func_t, Entity_Network_Func_Init */
#include "cJSON.h"                 /* cJSON_Hooks, cJSON_InitHooks */

static void _Log_Info_Init(void)
{
    Entity_Log_Info_t info =
    {
        .Log_Level = ENTITY_LOG_LEVEL_DEBUG,
        .Print     = Bsp_Vprint,   /* BK 自有實作（Task 2.3） */
    };
    Entity_Log_Info_Init(&info);
}

static void _Countdown_Timer_Func_Init(void)
{
    Entity_Timer_Func_t cbs =
    {
        .Get_System_Run_Time_Ms    = Bsp_Get_Run_Time_Ms,         /* BK 自有（Task 1.1） */
        .System_Timer_Countdown_Ms = Bsp_System_Timer_Countdown_Ms,/* neutral：platform_os/bsp_timer_countdown.c */
        .System_Timer_Countdown    = Bsp_System_Timer_Countdown,
        .System_Timer_Remain       = Bsp_System_Timer_Remain,
        .System_Timer_Expired      = Bsp_System_Timer_Expired,
    };
    Entity_Timer_Func_Init(&cbs);
}

static void _Cjson_Mem_Func_Init(void)
{
    /* BK7258 無 PSRAM，故 IOT_MEM_USE_PSRAM 未定義，走 #else 分支。
     * #ifdef 結構保留以便移植到帶 PSRAM 的 BK 系列時只加 #define 即可。 */
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
        .Tcp_Connect              = Bsp_Tcp_Connect,              /* neutral：platform_os/bsp_network.c */
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
// 冪等守衛：防止 Bind 路徑與通用 init 路徑雙重呼叫（esp32 版無此守衛）。
void Entity_System_Chip_Common_Init(void)
{
    static int s_inited = 0;
    if (s_inited) { return; }
    s_inited = 1;
    _Log_Info_Init();
    _Countdown_Timer_Func_Init();
    _Cjson_Mem_Func_Init();
    _Network_Func_Init();
}
