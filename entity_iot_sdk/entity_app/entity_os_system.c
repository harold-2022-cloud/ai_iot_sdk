//entity_os_system.c
#include "entity_os_system.h"

#include "entity_dev_info.h"
#include "entity_authorization.h"
#include "entity_msg_queue.h"
#include "entity_ble_gatt.h"
#include "entity_wifi.h"
#include "entity_config_net.h"
#include "entity_log.h"
#include "entity_uart.h"
#include "entity_mqtt_app.h"
#include "entity_mqtt_event_report.h"
#include "entity_report.h"
#include "factory_test.h"

#include "entity_iot_func.h"

#include <stddef.h>

typedef struct
{
    size_t internal_free;
    size_t internal_largest;
    size_t dma_free;
    size_t dma_largest;
    size_t psram_free;
    size_t psram_largest;
} Entity_Net_Heap_Snapshot_t;

typedef struct
{
    unsigned char has_prev;
    Entity_Net_Heap_Snapshot_t prev;
} Entity_Net_Heap_Log_Ctx_t;

static int Entity_Net_Heap_Delta(size_t now, size_t prev)
{
    return (now >= prev) ? (int)(now - prev) : -(int)(prev - now);
}

static void Entity_Net_Heap_Snapshot(Entity_Net_Heap_Snapshot_t *snapshot)
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

static void Entity_Net_Log_Heap_Stage(const char *stage, Entity_Net_Heap_Log_Ctx_t *ctx)
{
    Entity_Net_Heap_Snapshot_t now;
    unsigned char has_prev = (ctx != NULL) && ctx->has_prev;

    Entity_Net_Heap_Snapshot(&now);
    ENTITY_LOGI("[ENTITY_BOOT_NET_HEAP] stage=%s "
                "internal_free=%u internal_largest=%u internal_d_free=%d internal_d_largest=%d "
                "dma_free=%u dma_largest=%u dma_d_free=%d dma_d_largest=%d "
                "psram_free=%u psram_largest=%u psram_d_free=%d psram_d_largest=%d\r\n",
                stage ? stage : "unknown",
                (unsigned int)now.internal_free,
                (unsigned int)now.internal_largest,
                has_prev ? Entity_Net_Heap_Delta(now.internal_free, ctx->prev.internal_free) : 0,
                has_prev ? Entity_Net_Heap_Delta(now.internal_largest, ctx->prev.internal_largest) : 0,
                (unsigned int)now.dma_free,
                (unsigned int)now.dma_largest,
                has_prev ? Entity_Net_Heap_Delta(now.dma_free, ctx->prev.dma_free) : 0,
                has_prev ? Entity_Net_Heap_Delta(now.dma_largest, ctx->prev.dma_largest) : 0,
                (unsigned int)now.psram_free,
                (unsigned int)now.psram_largest,
                has_prev ? Entity_Net_Heap_Delta(now.psram_free, ctx->prev.psram_free) : 0,
                has_prev ? Entity_Net_Heap_Delta(now.psram_largest, ctx->prev.psram_largest) : 0);

    if (ctx != NULL)
    {
        ctx->prev = now;
        ctx->has_prev = 1;
    }
}

static unsigned char Entity_System_Dev_State_Needs_Ble(unsigned char dev_state)
{
    return ((dev_state == DEV_UNPROVISION_STATE) ||
            (dev_state == DEV_BLE_PROVISION_STATE)) ? 1 : 0;
}


/**
*@名称 		Entity_Iot_System_Init
*@功能 		SDK系统初始化
*@参数 		const char *pid, const char *product_secret, const char *dev_version, const char *sub_version, Entity_Triple_Info_t *test_triple
*@返回值 	void
*@使用说明	
*/
int Entity_Iot_System_Init(const char *pid, const char *product_secret, const char *dev_version, const char *sub_version, Entity_Triple_Info_t *test_triple)
{
    Entity_Net_Heap_Log_Ctx_t heap_log = {0};

    Entity_Net_Log_Heap_Stage("system_init_entry", &heap_log);
    unsigned char dev_state = Entity_Load_Dev_Info(pid, product_secret, dev_version, sub_version,test_triple);
    Entity_Net_Log_Heap_Stage("after_Entity_Load_Dev_Info", &heap_log);
    ENTITY_LOGI("===============================================================\r\n");
    ENTITY_LOGI("dev_version:%s, sub_version:%s, pid:%s, dev_state:%d\r\n", dev_version, sub_version, pid, dev_state);
    ENTITY_LOGI("===============================================================\r\n");
    Entity_App_Msg_Queue_Init();//消息队列处理初始化
    Entity_Report_Worker_Init();//telemetry report worker: MQTT/HTTP send never runs on caller thread
    Entity_Mqtt_Event_Report_Init();//token-pending临界区在开机时初始化，确保启动早于任何调用者
    Entity_Net_Log_Heap_Stage("after_Entity_App_Msg_Queue_Init", &heap_log);
    
    if(dev_state == DEV_UNAUTHORIZED_STATE)//设备未授权  
    {
        //Factory_Test_Wifi_Config();//连接厂测默认WIFI热点，获取到IP后启动厂测流程
        Entity_Net_Log_Heap_Stage("before_Factory_Test_Auth_Check_Task_Start", &heap_log);
        Factory_Test_Auth_Check_Task_Start();//根据扫描到特定热点来进入相关的三元组授权模式
        Entity_Net_Log_Heap_Stage("after_Factory_Test_Auth_Check_Task_Start", &heap_log);
        return 0;
    }
    if(Entity_System_Dev_State_Needs_Ble(dev_state))
    {
        Entity_Net_Log_Heap_Stage("before_Entity_Ble_Init", &heap_log);
        Entity_Ble_Ensure_Init();//仅在配网/蓝牙绑定启动路径初始化BLE
        Entity_Net_Log_Heap_Stage("after_Entity_Ble_Init", &heap_log);
    }
    else
    {
        ENTITY_LOGI("[ENTITY_BOOT] skip_Entity_Ble_Init dev_state=%d\r\n", dev_state);
        Entity_Net_Log_Heap_Stage("skip_Entity_Ble_Init", &heap_log);
    }
    Entity_Net_Log_Heap_Stage("before_Entity_Wifi_Init", &heap_log);
    Entity_Wifi_Init(ENTITY_WIFI_STA_MODE);//WIFI初始化
    Entity_Net_Log_Heap_Stage("after_Entity_Wifi_Init", &heap_log);
    Entity_Wifi_Info_t *wifi_info = Entity_Get_Wifi_Info();
    Entity_Net_Log_Heap_Stage("before_Entity_Wifi_Config_Net_Info", &heap_log);
    Entity_Wifi_Config_Net_Info(wifi_info->Ssid, wifi_info->Key);
    Entity_Net_Log_Heap_Stage("after_Entity_Wifi_Config_Net_Info", &heap_log);

    //未绑定状态：WiFi初始化后立即启动预缓存扫描，不等异步消息队列
    if(dev_state == DEV_UNPROVISION_STATE)
    {
        ENTITY_LOGI("[WiFi缓存] WiFi初始化完成, 立即启动预缓存扫描\r\n");
        Entity_Net_Log_Heap_Stage("before_Entity_Wifi_Scan_Cache_Start", &heap_log);
        Entity_Wifi_Scan_Cache_Start();
        Entity_Net_Log_Heap_Stage("after_Entity_Wifi_Scan_Cache_Start", &heap_log);
    }

    switch(dev_state)
    {
        case DEV_WIFI_PROVISION_STATE://WIFI绑定
        {
            ENTITY_LOGI("================启动 WiFi 连接===============\r\n");
            Entity_Net_Log_Heap_Stage("before_send_net_mode_sta_start", &heap_log);
            Entity_Set_Net_Mode_Msg_Send(ENTITY_NET_MODE_STA_START);
            Entity_Net_Log_Heap_Stage("after_send_net_mode_sta_start", &heap_log);
            break;
        }
        case DEV_BLE_PROVISION_STATE://蓝牙绑定
        {
            ENTITY_LOGI("================启动蓝牙连接===============\r\n");
            Entity_Net_Log_Heap_Stage("before_send_net_mode_ble_bind", &heap_log);
            Entity_Set_Net_Mode_Msg_Send(ENTITY_NET_MODE_BLE_BIND);//进入蓝牙绑定模式
            Entity_Net_Log_Heap_Stage("after_send_net_mode_ble_bind", &heap_log);
            break;
        }
        case DEV_UNPROVISION_STATE://未绑定
        {
            ENTITY_LOGI("================开始配网===============\r\n");
            Entity_Net_Log_Heap_Stage("before_send_net_mode_ble_config", &heap_log);
            Entity_Set_Net_Mode_Msg_Send(ENTITY_NET_MODE_BLE_CONFIG);
            Entity_Net_Log_Heap_Stage("after_send_net_mode_ble_config", &heap_log);
            break;
        }
        default:
        {
            ENTITY_LOGE("未知类型\\rn");
            break;  
        }   
    }
    Entity_Net_Log_Heap_Stage("system_init_done", &heap_log);
    return 0;
}

/**
*@名称 		Entity_Config_Net_Manual_Start
*@功能 		手动进入配网
*@参数 		void
*@返回值 	void
*@使用说明	
*/
void Entity_Config_Net_Manual_Start(void)
{
    ENTITY_LOGI("================%s===============\r\n", __func__);
    Entity_Set_Net_Mode_Msg_Send(ENTITY_NET_MODE_BLE_CONFIG);
}

/**
*@名称      Entity_Manual_Config_Net_Process
*@功能      设备本地进入配网，不重启系统
*@参数      unsigned char need_clear 是否清除用户数据
*@返回值    void
*@使用说明  用于本地按键触发重新配网，保留旧重置上报和清理动作，但不执行系统重启
*/
void Entity_Manual_Config_Net_Process(unsigned char need_clear)
{
    ENTITY_LOGI("手动配网流程入口：need_clear:%d\r\n", need_clear);

    ENTITY_LOGI("手动配网清理阶段入口：清除配网与物模型缓存\r\n");
    Entity_Reset_Config_Net_Info_To_Flash();//清除配网信息
    Entity_Reset_Thing_Model_To_Flash();//清除物模型信息
    ENTITY_LOGI("手动配网清理阶段完成\r\n");

    ENTITY_LOGI("手动配网云端重置上报阶段入口：need_clear:%d\r\n", need_clear);
    Entity_Mqtt_Event_Reset_Report(need_clear, 1);
    ENTITY_LOGI("手动配网云端重置上报阶段完成：已请求发布重置事件\r\n");

    ENTITY_LOGI("手动配网网络切换阶段入口：停止MQTT并启动BLE配网\r\n");
    Entity_Mqtt_Client_Task_Stop();
    Entity_Config_Net_Manual_Start();
    ENTITY_LOGI("手动配网网络切换阶段完成：已请求进入BLE配网，不执行系统重启\r\n");
}

/**
*@名称 		Entity_Manual_Reset_Process
*@功能 		设备本地重置的处理接口
*@参数 		unsigned char need_clear 是否清除用户数据
*@返回值 	void
*@使用说明	
*/
void Entity_Manual_Reset_Process(unsigned char need_clear)
{
    ENTITY_LOGI("%s need_clear:%d\r\n", __FUNCTION__, need_clear);
    
    Entity_Reset_Config_Net_Info_To_Flash();//清除配网信息
    Entity_Reset_Thing_Model_To_Flash();//清除物模型信息
    Entity_Mqtt_Event_Reset_Report(need_clear, 1);
    Entity_Mqtt_Client_Task_Stop();
    Entity_Sleep_Ms(2000);
#if 1
    //系统延时重启  
    Entity_System_Reset();
#else
    Entity_Config_Net_Manual_Start();
#endif
}






