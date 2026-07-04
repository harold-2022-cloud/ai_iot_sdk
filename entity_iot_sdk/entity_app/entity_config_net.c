//entity_config_net.c
#include "entity_config_net.h"

#include "entity_dev_info.h"
#include "entity_wifi.h"
#include "entity_log.h"
#include "entity_ble_gatt.h"
#include "entity_iot_func.h"
#include "entity_msg_queue.h"

#include <stddef.h>

static Wifi_Account_Param_t Wifi_Account_Param;
static unsigned char Entity_Net_Mode;//当前网络模式
static void *Config_Timer_Handle=0;
#define CONFIG_TIME_PERIOD        60000     //时基定时器周期60S
#define CONFIG_NET_TIMEOUT_TIMER_TYPE   ENTITY_TIMER_ONESHOT_TYPE

typedef struct
{
    size_t internal_free;
    size_t internal_largest;
    size_t dma_free;
    size_t dma_largest;
    size_t psram_free;
    size_t psram_largest;
} Entity_Net_Mode_Heap_Snapshot_t;

typedef struct
{
    unsigned char has_prev;
    Entity_Net_Mode_Heap_Snapshot_t prev;
} Entity_Net_Mode_Heap_Log_Ctx_t;

static int Entity_Net_Mode_Heap_Delta(size_t now, size_t prev)
{
    return (now >= prev) ? (int)(now - prev) : -(int)(prev - now);
}

static void Entity_Net_Mode_Heap_Snapshot(Entity_Net_Mode_Heap_Snapshot_t *snapshot)
{
    Entity_Memory_Snapshot_t snap = {0};

    Entity_Get_Memory_Snapshot(&snap);
    snapshot->internal_free    = (size_t)snap.internal_free;
    snapshot->internal_largest = (size_t)snap.internal_largest;
    snapshot->dma_free         = (size_t)snap.dma_free;
    snapshot->dma_largest      = (size_t)snap.dma_largest;
    snapshot->psram_free       = (size_t)snap.external_free;     /* HAL 的 external = PSRAM */
    snapshot->psram_largest    = (size_t)snap.external_largest;
}

static void Entity_Net_Mode_Log_Heap_Stage(const char *stage, Entity_Net_Mode_Heap_Log_Ctx_t *ctx)
{
    Entity_Net_Mode_Heap_Snapshot_t now;
    unsigned char has_prev = (ctx != NULL) && ctx->has_prev;

    Entity_Net_Mode_Heap_Snapshot(&now);
    ENTITY_LOGI("[ENTITY_NET_MODE_HEAP] stage=%s "
                "internal_free=%u internal_largest=%u internal_d_free=%d internal_d_largest=%d "
                "dma_free=%u dma_largest=%u dma_d_free=%d dma_d_largest=%d "
                "psram_free=%u psram_largest=%u psram_d_free=%d psram_d_largest=%d\r\n",
                stage ? stage : "unknown",
                (unsigned int)now.internal_free,
                (unsigned int)now.internal_largest,
                has_prev ? Entity_Net_Mode_Heap_Delta(now.internal_free, ctx->prev.internal_free) : 0,
                has_prev ? Entity_Net_Mode_Heap_Delta(now.internal_largest, ctx->prev.internal_largest) : 0,
                (unsigned int)now.dma_free,
                (unsigned int)now.dma_largest,
                has_prev ? Entity_Net_Mode_Heap_Delta(now.dma_free, ctx->prev.dma_free) : 0,
                has_prev ? Entity_Net_Mode_Heap_Delta(now.dma_largest, ctx->prev.dma_largest) : 0,
                (unsigned int)now.psram_free,
                (unsigned int)now.psram_largest,
                has_prev ? Entity_Net_Mode_Heap_Delta(now.psram_free, ctx->prev.psram_free) : 0,
                has_prev ? Entity_Net_Mode_Heap_Delta(now.psram_largest, ctx->prev.psram_largest) : 0);

    if (ctx != NULL)
    {
        ctx->prev = now;
        ctx->has_prev = 1;
    }
}

/**
*@名称 		Config_Net_Timer_Callback
*@功能 		配网超时定时器回调
*@参数 		void *arg 
*@返回值 	void
*@使用说明  配网获取WIFI信息后，但未连上MQTT，超时1分钟后保留配置并回到可重试配网态
*/
void Config_Net_Timer_Callback(void *arg1, void *arg2)
{
    int wifi_disconnect_ret = 0;
    int ble_disconnect_ret = 0;
    int ble_adv_data_ret = 0;

    (void)arg1;
    (void)arg2;

    ENTITY_LOGW("配网超时阶段入口：60秒内未完成联网，切回可重试BLE配网态\r\n");

    Entity_Config_Net_Timer_Stop();
    Entity_Wifi_Sta_Auto_Reconnect_Enable(0);
    wifi_disconnect_ret = Entity_Wifi_Sta_Disconnect();

    Wifi_Account_Param.Flag_Wifi_Need_Reconnect = 0;
    Wifi_Account_Param.Flag_Wifi_Info_Vaild = 0;
    Wifi_Account_Param.Flag_Config_Net_Success = 0;
    Wifi_Account_Param.Flag_Have_Got_Ip = 0;
    Wifi_Account_Param.Flag_Have_Get_Config_Net_Info = 0;
    Wifi_Account_Param.Flag_Config_Net_Progress = 1;
    Wifi_Account_Param.Entity_Config_Net_Flag &= ~ENTITY_NOT_IN_CONFIG_STATUS_MASK;
    Wifi_Account_Param.Entity_Config_Net_Flag &= ~ENTITY_WIFI_CONNECTED_MASK;
    Wifi_Account_Param.Entity_Config_Net_Flag |= ENTITY_REQUST_CONNECT_MASK;

    ble_disconnect_ret = Entity_Ble_Disconnect();
    ble_adv_data_ret = Entity_Ble_Set_Adv_Scan_Rsp_Data();
    Entity_Ble_Adv_Start();

    Entity_Net_Mode = ENTITY_NET_MODE_BLE_CONFIG;
    Entity_Set_Dev_Status(DEV_PROVISION_FAILD_STATE);

    ENTITY_LOGW("配网超时阶段完成：已保留用户配置，等待BLE重新配网 wifi_ret=%d ble_disc_ret=%d ble_adv_ret=%d\r\n",
              wifi_disconnect_ret,
              ble_disconnect_ret,
              ble_adv_data_ret);
}

/**
*@名称        Entity_Config_Net_Timer_Create
*@功能        创建配网超时定时器
*@参数        void 
*@返回值   int
*@使用说明  
*/
int Entity_Config_Net_Timer_Create(void)
{
    int ret = 0;
    if(Config_Timer_Handle == 0)
    {
        //创建单次定时器
        Config_Timer_Handle = Entity_Timer_Create(CONFIG_NET_TIMEOUT_TIMER_TYPE, CONFIG_TIME_PERIOD, Config_Net_Timer_Callback, NULL);
        if(Config_Timer_Handle == NULL)
            ENTITY_LOGE("%s, config net timer create faild\r\n", __func__);
        else
            ENTITY_LOGD("%s, config net  timer create success\r\n", __func__);
    }
    return ret;
}
/**
*@名称        Entity_Config_Net_Timer_Start
*@功能        启动配网定时器
*@参数        void 
*@返回值   void
*@使用说明  
*/
void Entity_Config_Net_Timer_Start(void)
{
    if(Config_Timer_Handle == 0)
    {
        if(Entity_Config_Net_Timer_Create())
            return;
    }  
    Entity_Timer_Start(CONFIG_NET_TIMEOUT_TIMER_TYPE, Config_Timer_Handle);
}

/**
*@名称        Entity_Config_Net_Timer_Stop
*@功能        停止配网定时器
*@参数        void 
*@返回值   void
*@使用说明  
*/
void Entity_Config_Net_Timer_Stop(void)
{
    if(Config_Timer_Handle)
        Entity_Timer_Stop(CONFIG_NET_TIMEOUT_TIMER_TYPE, Config_Timer_Handle);
}

/**
*@名称        Entity_Config_Net_Timer_Delete
*@功能        删除配网定时器
*@参数        void 
*@返回值   void
*@使用说明  
*/
void Entity_Config_Net_Timer_Delete(void)
{
    if(Config_Timer_Handle)
        Entity_Timer_Delete(CONFIG_NET_TIMEOUT_TIMER_TYPE, Config_Timer_Handle);
}

/**
*@名称 		Entity_Set_Net_Mode
*@功能 		设置当前网络模式
*@参数 		Entity_Net_Mode_e mode
*@返回值 	void
*@使用说明	
*/
void Entity_Set_Net_Mode(Entity_Net_Mode_e mode)
{
    unsigned char same_ble_config_mode = 0;
    Entity_Net_Mode_Heap_Log_Ctx_t heap_log = {0};

    ENTITY_LOGI("%s set mode:%d, current mode:%d\r\n", __func__, mode, Entity_Net_Mode);
    Entity_Net_Mode_Log_Heap_Stage("set_mode_entry", &heap_log);
    if(Entity_Net_Mode == mode)
    {
        if(mode != ENTITY_NET_MODE_BLE_CONFIG)
        {
            ENTITY_LOGI("have in mode\r\n");
            Entity_Net_Mode_Log_Heap_Stage("set_mode_same_skip", &heap_log);
            return ;
        }
        same_ble_config_mode = 1;
        ENTITY_LOGI("BLE配网重入阶段入口：当前已在配网模式，准备刷新BLE连接与广播\r\n");
    }
    Entity_Dev_Config_Net_Info_t *config_net_info = Entity_Get_Dev_Config_Net_Info();
    switch(mode)
    {
        case ENTITY_NET_MODE_BLE_CONFIG:
        {
            int wifi_disconnect_ret = 0;
            int ble_disconnect_ret = 0;
            int ble_adv_data_ret = 0;
            int ble_init_ret = 0;

            Entity_Net_Mode = mode;
            ENTITY_LOGI("BLE配网入口：关闭旧WiFi自动重连并清空运行态账号 reenter=%d\r\n",
                      same_ble_config_mode);
            Entity_Net_Mode_Log_Heap_Stage("ble_config_before_ble_ensure_init", &heap_log);
            ble_init_ret = Entity_Ble_Ensure_Init();
            Entity_Net_Mode_Log_Heap_Stage("ble_config_after_ble_ensure_init", &heap_log);
            if(ble_init_ret != 0)
            {
                ENTITY_LOGE("BLE配网失败：BLE初始化失败 ret=%d\r\n", ble_init_ret);
                break;
            }
            Entity_Net_Mode_Log_Heap_Stage("ble_config_before_timer_stop", &heap_log);
            Entity_Config_Net_Timer_Stop();
            Entity_Net_Mode_Log_Heap_Stage("ble_config_after_timer_stop", &heap_log);
            Entity_Net_Mode_Log_Heap_Stage("ble_config_before_wifi_autoreconnect_off", &heap_log);
            Entity_Wifi_Sta_Auto_Reconnect_Enable(0);
            Entity_Net_Mode_Log_Heap_Stage("ble_config_after_wifi_autoreconnect_off", &heap_log);
            Wifi_Account_Param.Flag_Wifi_Need_Reconnect = 0;
            Wifi_Account_Param.Flag_Wifi_Info_Vaild = 0;
            Wifi_Account_Param.Flag_Config_Net_Success = 0;
            Wifi_Account_Param.Flag_Have_Got_Ip = 0;
            Entity_Net_Mode_Log_Heap_Stage("ble_config_before_wifi_config_empty", &heap_log);
            Entity_Wifi_Config_Net_Info("", "");
            Entity_Net_Mode_Log_Heap_Stage("ble_config_after_wifi_config_empty", &heap_log);

            //蓝牙扫描应答数据中的FLAG标志加载
            Wifi_Account_Param.Entity_Config_Net_Flag &= ~ENTITY_NOT_IN_CONFIG_STATUS_MASK;//在配网状态
            if(config_net_info->Flag_Bind)
                Wifi_Account_Param.Entity_Config_Net_Flag |= ENTITY_HAVE_BIND_MASK;
            else
                Wifi_Account_Param.Entity_Config_Net_Flag &= ~ENTITY_HAVE_BIND_MASK;
            Wifi_Account_Param.Entity_Config_Net_Flag &= ~ENTITY_WIFI_CONNECTED_MASK;//清WIFI连网成功标志
            Wifi_Account_Param.Entity_Config_Net_Flag |= ENTITY_REQUST_CONNECT_MASK;//请求蓝牙连接标志
            Wifi_Account_Param.Flag_Have_Get_Config_Net_Info = 0;
            Entity_Net_Mode_Log_Heap_Stage("ble_config_before_wifi_disconnect", &heap_log);
            wifi_disconnect_ret = Entity_Wifi_Sta_Disconnect();//WIFI断开STA连接
            Entity_Net_Mode_Log_Heap_Stage("ble_config_after_wifi_disconnect", &heap_log);
            Entity_Net_Mode_Log_Heap_Stage("ble_config_before_ble_disconnect", &heap_log);
            ble_disconnect_ret = Entity_Ble_Disconnect();//蓝牙主动断开当前连接，确保重新配网时手机可重新发现
            Entity_Net_Mode_Log_Heap_Stage("ble_config_after_ble_disconnect", &heap_log);
            Entity_Net_Mode_Log_Heap_Stage("ble_config_before_ble_adv_stop", &heap_log);
            Entity_Ble_Adv_Stop();//配网已完成，停止蓝牙广播   
            Entity_Net_Mode_Log_Heap_Stage("ble_config_after_ble_adv_stop", &heap_log);
            Entity_Net_Mode_Log_Heap_Stage("ble_config_before_ble_adv_data", &heap_log);
            ble_adv_data_ret = Entity_Ble_Set_Adv_Scan_Rsp_Data();//设置广播应答数据
            Entity_Net_Mode_Log_Heap_Stage("ble_config_after_ble_adv_data", &heap_log);
            Entity_Net_Mode_Log_Heap_Stage("ble_config_before_ble_adv_start", &heap_log);
            Entity_Ble_Adv_Start();//启动蓝牙广播
            Entity_Net_Mode_Log_Heap_Stage("ble_config_after_ble_adv_start", &heap_log);
            uint8_t cache_ready = Entity_Wifi_Is_Scan_Cache_Ready();
            unsigned char cache_num = Entity_Wifi_Get_Scan_Result_Num();
            if(!cache_ready || !cache_num)
            {
                ENTITY_LOGI("[WiFi缓存] 入口: 缓存不可用, 启动补充扫描, ready:%d AP数量:%d\r\n",
                          cache_ready,
                          cache_num);
                Entity_Net_Mode_Log_Heap_Stage("ble_config_before_wifi_scan_cache_start", &heap_log);
                Entity_Wifi_Scan_Cache_Start();//缓存未就绪时才启动扫描
                Entity_Net_Mode_Log_Heap_Stage("ble_config_after_wifi_scan_cache_start", &heap_log);
            }
            else
            {
                ENTITY_LOGI("[WiFi缓存] 完成: 缓存已就绪, 跳过重复扫描, AP数量:%d\r\n", cache_num);
            }
            Wifi_Account_Param.Flag_Config_Net_Progress = 1;//设置进入配网标志
            Entity_Net_Mode_Log_Heap_Stage("ble_config_before_set_dev_status", &heap_log);
            Entity_Set_Dev_Status(DEV_UNPROVISION_STATE);
            Entity_Net_Mode_Log_Heap_Stage("ble_config_after_set_dev_status", &heap_log);
            ENTITY_LOGI("BLE配网完成：已断开旧连接并启动广播 wifi_ret=%d ble_disc_ret=%d ble_adv_ret=%d\r\n",
                      wifi_disconnect_ret,
                      ble_disconnect_ret,
                      ble_adv_data_ret);
            break;
        }
        
        case ENTITY_NET_MODE_CONFIG_TRY_CONNECT:
        {
            ENTITY_LOGI("配网试连入口：恢复WiFi自动重连并连接新账号\r\n");
            Entity_Net_Mode_Log_Heap_Stage("config_try_before_wifi_autoreconnect_on", &heap_log);
            Entity_Wifi_Sta_Auto_Reconnect_Enable(1);
            Entity_Net_Mode_Log_Heap_Stage("config_try_after_wifi_autoreconnect_on", &heap_log);
            Wifi_Account_Param.Flag_Have_Get_Config_Net_Info = 1;
            Entity_Net_Mode_Log_Heap_Stage("config_try_before_wifi_connect", &heap_log);
            Entity_Wifi_Sta_Conncet();//连接WIFI
            Entity_Net_Mode_Log_Heap_Stage("config_try_after_wifi_connect", &heap_log);
            Entity_Net_Mode_Log_Heap_Stage("config_try_before_timer_start", &heap_log);
            Entity_Config_Net_Timer_Start();//
            Entity_Net_Mode_Log_Heap_Stage("config_try_after_timer_start", &heap_log);
            break;
        }
        
        case ENTITY_NET_MODE_STA_START:
        {
            Wifi_Account_Param.Entity_Config_Net_Flag |= ENTITY_NOT_IN_CONFIG_STATUS_MASK;//不在配网状态
            if(config_net_info->Flag_Bind)
                Wifi_Account_Param.Entity_Config_Net_Flag |= ENTITY_HAVE_BIND_MASK;
            else
                Wifi_Account_Param.Entity_Config_Net_Flag &= ~ENTITY_HAVE_BIND_MASK;
            Wifi_Account_Param.Entity_Config_Net_Flag &= ~ENTITY_WIFI_CONNECTED_MASK;//清WIFI连网成功标志
            Wifi_Account_Param.Entity_Config_Net_Flag &= ~ENTITY_REQUST_CONNECT_MASK;//请求蓝牙连接标志
            Entity_Net_Mode_Log_Heap_Stage("sta_start_before_ble_disconnect", &heap_log);
            Entity_Ble_Disconnect();//蓝牙主动断开当前连接
            Entity_Net_Mode_Log_Heap_Stage("sta_start_after_ble_disconnect", &heap_log);
            Entity_Net_Mode_Log_Heap_Stage("sta_start_before_ble_adv_stop", &heap_log);
            Entity_Ble_Adv_Stop();//配网已完成，停止蓝牙广播    
            Entity_Net_Mode_Log_Heap_Stage("sta_start_after_ble_adv_stop", &heap_log);
            Entity_Net_Mode_Log_Heap_Stage("sta_start_before_wifi_autoreconnect_on", &heap_log);
            Entity_Wifi_Sta_Auto_Reconnect_Enable(1);
            Entity_Net_Mode_Log_Heap_Stage("sta_start_after_wifi_autoreconnect_on", &heap_log);
            Entity_Net_Mode_Log_Heap_Stage("sta_start_before_wifi_connect", &heap_log);
            Entity_Wifi_Sta_Conncet();
            Entity_Net_Mode_Log_Heap_Stage("sta_start_after_wifi_connect", &heap_log);
            Entity_Net_Mode_Log_Heap_Stage("sta_start_before_set_dev_status", &heap_log);
            Entity_Set_Dev_Status(DEV_WIFI_CONNECTING_STATE);
            Entity_Net_Mode_Log_Heap_Stage("sta_start_after_set_dev_status", &heap_log);
            break;
        }
        
        case ENTITY_NET_MODE_STA_STOP:
        {
            /* TODO_RUNTIME_STUB: 当前没有停止 STA/MQTT/BLE 的实际动作；启用前需定义状态与重连策略。 */
            break;
        }
        
        case ENTITY_NET_MODE_BLE_BIND:
        {
            int ble_init_ret = 0;

            Entity_Net_Mode_Log_Heap_Stage("ble_bind_before_ble_ensure_init", &heap_log);
            ble_init_ret = Entity_Ble_Ensure_Init();
            Entity_Net_Mode_Log_Heap_Stage("ble_bind_after_ble_ensure_init", &heap_log);
            if(ble_init_ret != 0)
            {
                ENTITY_LOGE("BLE绑定失败：BLE初始化失败 ret=%d\r\n", ble_init_ret);
                break;
            }
            Entity_Net_Mode_Log_Heap_Stage("ble_bind_before_ble_adv_data", &heap_log);
            Entity_Ble_Set_Adv_Scan_Rsp_Data();//设置广播应答数据 
            Entity_Net_Mode_Log_Heap_Stage("ble_bind_after_ble_adv_data", &heap_log);
            Entity_Net_Mode_Log_Heap_Stage("ble_bind_before_ble_adv_start", &heap_log);
            Entity_Ble_Adv_Start();//启动蓝牙广播
            Entity_Net_Mode_Log_Heap_Stage("ble_bind_after_ble_adv_start", &heap_log);
            break;
        }
        default:
            Entity_Net_Mode_Log_Heap_Stage("set_mode_unknown", &heap_log);
            return;
    }
    Entity_Net_Mode = mode;
    Entity_Net_Mode_Log_Heap_Stage("set_mode_done", &heap_log);
}


/**
*@名称 		Get_Wifi_Account_Param
*@功能 		获取WIFI相关信息
*@参数 		void
*@返回值 	Wifi_Account_Param_t*
*@使用说明	
*/
Wifi_Account_Param_t *Get_Wifi_Account_Param(void)
{
    return &Wifi_Account_Param;
}

/**
*@名称 		Set_Flag_Config_Net_Progress
*@功能 		设置配网标志位
*@参数 		unsigned char value
*@返回值 	void
*@使用说明	
*/
void Set_Flag_Config_Net_Progress(unsigned char value)
{
    Wifi_Account_Param.Flag_Config_Net_Progress = value ? 1 : 0;
}

/**
*@名称 		Get_Flag_Config_Net_Progress
*@功能 		获取配网标志位
*@参数 		void
*@返回值 	unsigned char
*@使用说明	
*/
unsigned char Get_Flag_Config_Net_Progress(void)
{
    return Wifi_Account_Param.Flag_Config_Net_Progress;
}


/**
*@名称 		Entity_Set_Net_Mode_Msg_Send
*@功能 		设置当前网络模式消息发送
*@参数 		Entity_Net_Mode_e mode
*@返回值 	void
*@使用说明	
*/
void Entity_Set_Net_Mode_Msg_Send(Entity_Net_Mode_e mode)
{
    Entity_App_Msg_Queue_Send(ENTITY_MSG_TYPE_SET_NET_MODE, (const void*)&mode, sizeof(Entity_Net_Mode_e));
}



