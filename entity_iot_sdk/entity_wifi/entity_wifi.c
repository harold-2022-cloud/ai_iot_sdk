//entity_wifi.c
#include "entity_wifi.h"

#include "entity_ble_gatt.h"
#include "entity_config_net.h"
#include "entity_log.h"
#include "entity_iot_func.h"
#include "entity_dev_info.h"
#include "entity_mqtt_app.h"

static Entity_Wifi_Cbs_t Entity_Wifi_Cbs;

#define ENTITY_WIFI_SCAN_CACHE_TTL_MS     (30U * 1000U)

#ifndef ENTITY_RELEASE_BLE_AFTER_PROVISION_SUCCESS
#define ENTITY_RELEASE_BLE_AFTER_PROVISION_SUCCESS 0
#endif

static unsigned char Flag_Get_Ip;
static uint8_t Flag_Wifi_Scan_Cache_Ready;
static uint32_t Wifi_Scan_Cache_Update_Time_Ms;

typedef void (*Entity_Wifi_Scan_Done_Cb_f)(void *res);

static Entity_Wifi_Scan_Done_Cb_f s_wifi_scan_user_done_cb;

static void Entity_Wifi_Cache_Scan_Done_Callback(void *res);
static void Entity_Wifi_Scan_Done_Dispatch_Callback(void *res);

static uint32_t Entity_Wifi_Cache_Now_Ms(void)
{
    if (Entity_Get_Run_Time_Ms)
    {
        return Entity_Get_Run_Time_Ms();
    }

    return 0;
}


/**
*@名称 		Entity_Wifi_Get_Ip_Success
*@功能 		WIFI成功获取IP标志
*@参数 		void	
*@返回值 	unsigned char
*@使用说明	
*/
unsigned char Entity_Wifi_Get_Ip_Success(void)
{
    return Flag_Get_Ip;
}

/**
*@名称 		Entity_Wifi_App_Event_Callback
*@功能 		WIFI事件回调
*@参数 		void	
*@返回值 	void
*@使用说明	
*/
void Entity_Wifi_App_Event_Callback(int event)
{
    Wifi_Account_Param_t *account_info = Get_Wifi_Account_Param();
    switch (event) 
    {
        case ENTITY_WIFI_EVT_SCAN_DONE://扫描完成事件
            ENTITY_LOGI("[%s]WiFi: Scan done\r\n", __func__);
            break;
        case ENTITY_WIFI_EVT_STA_CONNECTED://WIFI已连接事件
            ENTITY_LOGI("\r\n-------------------------------[%s]WiFi: Connected---------------------------------\r\n\r\n", __func__);
            break;
        case ENTITY_WIFI_EVT_STA_DISCONNECTED://WIFI已断连
            ENTITY_LOGI("\r\n------------------------------[%s]WiFi: Disconnected-------------------------------\r\n\r\n", __func__);
            if(Flag_Get_Ip)
            {
                Flag_Get_Ip = 0;
                Entity_Set_Dev_Status(DEV_WIFI_DISCONNECT_STATE);
            }
            break;
        case ENTITY_WIFI_EVT_WPS_TIMEOUT://超时事件
            ENTITY_LOGI("[%s]WiFi: wps is timeout\r\n", __func__);
            break;
        case ENTITY_WIFI_EVT_IP_CHANGE://IP地址更新
            ENTITY_LOGI("\r\n------------------------------[%s]WiFi: IP changed----------------------------------\r\n\r\n", __func__);
            if(account_info->Flag_Config_Net_Progress)//正在配网过程中,连网获取到IP，说明WIFI信息是正确的。
            {
                account_info->Flag_Config_Net_Progress = 0;
                account_info->Flag_Wifi_Info_Vaild = 1;//获取到有效WIFI信息标志
                Entity_Ble_Disconnect();//蓝牙主动断开当前连接
                Entity_Ble_Adv_Stop();//配网已完成，停止蓝牙广播
#if ENTITY_RELEASE_BLE_AFTER_PROVISION_SUCCESS
                /* 省内存模式会彻底释放 BLE 控制器+协议栈内存，释放后本次开机周期不能再重新配网。 */
                ENTITY_LOGI("配网完成阶段入口：开始释放 BLE 内存\r\n");
                Entity_Ble_Disable();
                ENTITY_LOGI("配网完成阶段完成：BLE 内存释放调用返回\r\n");
#else
                ENTITY_LOGI("配网完成BLE保留阶段入口：已停止广播，保留BLE栈用于返回键重新配网\r\n");
                ENTITY_LOGI("配网完成BLE保留阶段完成：未释放BLE内存\r\n");
#endif
            }
            else if(account_info->Flag_Wifi_Need_Reconnect)//正在WIFI断网重连过程中
            {
                account_info->Flag_Wifi_Need_Reconnect = 0; //在WIFI断网时蓝牙会自动连接通信，而WIFI恢复后要断开蓝牙通信。
                Entity_Ble_Disconnect();//蓝牙主动断开当前连接
                Entity_Ble_Adv_Stop();//配网已完成，停止蓝牙广播    
                Entity_Set_Dev_Status(DEV_WIFI_CONNECTED_STATE);
            }
            else
            {
                Entity_Set_Dev_Status(DEV_WIFI_CONNECTED_STATE);
            }
            Flag_Get_Ip = 1;
            Flag_Wifi_Scan_Cache_Ready = 0;//配网完成，清除WiFi扫描缓存
            Wifi_Scan_Cache_Update_Time_Ms = 0;
            Entity_Mqtt_Client_Task_Start();//启动MQTT客户端任务
            break;
        default:
            break;
    }
}


/**
*@名称 		Get_Entity_Wifi_Cbs
*@功能 		获取WIFI接口函数
*@参数 		void
*@返回值 	Entity_Wifi_Cbs_t *
*@使用说明	
*/
Entity_Wifi_Cbs_t *Get_Entity_Wifi_Cbs(void)
{
    return &Entity_Wifi_Cbs;
}

/**
*@名称 		Entity_Wifi_Cbs_Init
*@功能 		WIFI接口函数初始化
*@参数 		Entity_Wifi_Cbs_t *cbs
*@返回值 	void
*@使用说明	
*/
void Entity_Wifi_Cbs_Init(Entity_Wifi_Cbs_t *cbs)
{
    Entity_Wifi_Cbs = *cbs;
}

/**
*@名称 		Entity_Wifi_Get_Scan_Result_Num
*@功能 		返回WIFI扫描结果AP的数量
*@参数 		void
*@返回值 	unsigned char
*@使用说明	
*/
unsigned char Entity_Wifi_Get_Scan_Result_Num(void)
{
    Entity_Wifi_Sta_Scan_Result_t result;
    if (Entity_Wifi_Copy_Scan_Results(&result) == 0)
    {
        return result.Num;
    }

    if(Entity_Wifi_Cbs.Wifi_Get_Scan_Results)
    {
        Entity_Wifi_Sta_Scan_Result_t *legacy_result = (Entity_Wifi_Sta_Scan_Result_t *)Entity_Wifi_Cbs.Wifi_Get_Scan_Results();
        if(legacy_result == NULL)
            return 0;
        return legacy_result->Num;
    }  
    else
        return 0;
}

/**
*@名称        Entity_Wifi_Copy_Scan_Results
*@功能        复制WIFI扫描结果快照
*@参数        Entity_Wifi_Sta_Scan_Result_t *result
*@返回值      int
*@使用说明    优先走底层快照复制，避免读取全局缓存时和扫描完成回调并发
*/
int Entity_Wifi_Copy_Scan_Results(Entity_Wifi_Sta_Scan_Result_t *result)
{
    if (result == NULL)
    {
        return -1;
    }

    memset(result, 0, sizeof(Entity_Wifi_Sta_Scan_Result_t));
    if (Entity_Wifi_Cbs.Wifi_Copy_Scan_Results)
    {
        return Entity_Wifi_Cbs.Wifi_Copy_Scan_Results(result, sizeof(Entity_Wifi_Sta_Scan_Result_t));
    }

    return -1;
}


/**
*@名称 		Entity_Wifi_Init
*@功能 		WIFI初始化
*@参数 		uint8_t mode
*@返回值 	void
*@使用说明	
*/
void Entity_Wifi_Init(uint8_t mode)
{
    if(Entity_Wifi_Cbs.Wifi_Init)
        Entity_Wifi_Cbs.Wifi_Init(mode);
    if(Entity_Wifi_Cbs.Register_Wifi_Event_App_Cb)
        Entity_Wifi_Cbs.Register_Wifi_Event_App_Cb(Entity_Wifi_App_Event_Callback);
}

/**
*@名称 		Entity_Wifi_Config_Net_Info
*@功能 		
*@参数 		char *ssid, char *passwd
*@返回值 	void
*@使用说明	
*/
void Entity_Wifi_Config_Net_Info(char *ssid, char *passwd)
{
    if(Entity_Wifi_Cbs.Wifi_Sta_Mode_Config)
        Entity_Wifi_Cbs.Wifi_Sta_Mode_Config(ssid, passwd, ENTITY_SECURITY_AUTO);
}

/**
*@名称 		Entity_Get_Wifi_Mac
*@功能 		获取WIFI MAC
*@参数 		uint8_t *mac_addr
*@返回值 	int
*@使用说明	
*/
int Entity_Get_Wifi_Mac(uint8_t *mac_addr)
{
    if(Entity_Wifi_Cbs.Wifi_Get_Macaddr)
    {
        Entity_Wifi_Cbs.Wifi_Get_Macaddr(mac_addr);
        return 0;
    }
    return -1;
}

/**
*@名称 		Entity_Wifi_Sta_Conncet
*@功能 		WIFI连接热点,指定密码类型为BSP_SECURITY_WPA2PSK
*@参数 		void
*@返回值 	int
*@使用说明
*/
int Entity_Wifi_Sta_Conncet(void)
{
    if(Entity_Wifi_Cbs.Wifi_Sta_Conncet)
    {
        return Entity_Wifi_Cbs.Wifi_Sta_Conncet();
    }
    return -1;
}

/**
*@名称 		Entity_Wifi_Sta_Disconnect
*@功能 		sta断开连接
*@参数 		void
*@返回值 	int
*@使用说明	
*/
int Entity_Wifi_Sta_Disconnect(void)
{
    if(Entity_Wifi_Cbs.Wifi_Sta_Disconnect)
    {
        return Entity_Wifi_Cbs.Wifi_Sta_Disconnect();
    }
    return -1;
}

/**
*@名称      Entity_Wifi_Sta_Auto_Reconnect_Enable
*@功能      控制STA断线自动重连
*@参数      uint8_t enable
*@返回值    void
*@使用说明  进入BLE配网时关闭，正常STA连接前恢复
*/
void Entity_Wifi_Sta_Auto_Reconnect_Enable(uint8_t enable)
{
    if(Entity_Wifi_Cbs.Wifi_Sta_Auto_Reconnect_Enable)
        Entity_Wifi_Cbs.Wifi_Sta_Auto_Reconnect_Enable(enable);
}

/**
*@名称 		Entity_Wifi_Sta_Scan_Start
*@功能 		WIFI扫描
*@参数 		void* callback
*@返回值 	int
*@使用说明	
*/
int Entity_Wifi_Sta_Scan_Start(void* callback)
{
    //ENTITY_LOGI("%s\r\n", __func__);
    if(Entity_Wifi_Cbs.Wifi_Sta_Scan_Start)
    {
        s_wifi_scan_user_done_cb = (Entity_Wifi_Scan_Done_Cb_f)callback;
        return Entity_Wifi_Cbs.Wifi_Sta_Scan_Start(Entity_Wifi_Scan_Done_Dispatch_Callback);
    }
    return -1;
}

/**
*@名称 		Entity_Print_Scan_Result
*@功能 		打印WIFI扫描结果
*@参数 		void*res
*@返回值 	void 
*@使用说明	
*/
void Entity_Print_Scan_Result(void *res)
{
    ENTITY_LOGI("%s\r\n", __func__);
    if(Entity_Wifi_Cbs.Print_Scan_Result)
    {
        Entity_Wifi_Cbs.Print_Scan_Result(res);
    }
}

/**
*@名称 		Entity_Wifi_Cache_Scan_Done_Callback
*@功能 		WiFi预缓存扫描完成回调
*@参数 		void *res 扫描结果
*@返回值 	void
*@使用说明	配网模式启动时触发的预缓存扫描，完成后标记缓存就绪
*/
static void Entity_Wifi_Cache_Scan_Done_Callback(void *res)
{
    Entity_Wifi_Sta_Scan_Result_t *result = (Entity_Wifi_Sta_Scan_Result_t *)res;
    if (result == NULL)
    {
        Flag_Wifi_Scan_Cache_Ready = 0;
        Wifi_Scan_Cache_Update_Time_Ms = 0;
        ENTITY_LOGE("[WiFi缓存] 失败: 预缓存扫描回调结果为空\r\n");
        return;
    }

    Flag_Wifi_Scan_Cache_Ready = 1;
    Wifi_Scan_Cache_Update_Time_Ms = Entity_Wifi_Cache_Now_Ms();
    ENTITY_LOGI("[WiFi缓存] 完成: 预缓存扫描完成, AP数量:%d, ttl:%ums\r\n",
              result->Num,
              (unsigned int)ENTITY_WIFI_SCAN_CACHE_TTL_MS);
    Entity_Print_Scan_Result(res);
}

static void Entity_Wifi_Scan_Done_Dispatch_Callback(void *res)
{
    Entity_Wifi_Scan_Done_Cb_f user_cb = s_wifi_scan_user_done_cb;

    s_wifi_scan_user_done_cb = NULL;
    Entity_Wifi_Cache_Scan_Done_Callback(res);

    if ((user_cb != NULL) && (user_cb != Entity_Wifi_Cache_Scan_Done_Callback))
    {
        ENTITY_LOGI("[WiFi缓存] 完成: 分发扫描结果到请求方 cb:%p\r\n", user_cb);
        user_cb(res);
    }
}

/**
*@名称 		Entity_Wifi_Scan_Cache_Start
*@功能 		启动WiFi预缓存扫描（配网模式入口调用）
*@参数 		void
*@返回值 	void
*@使用说明	在配网模式开启时调用，提前缓存WiFi列表供手机端快速获取
*/
void Entity_Wifi_Scan_Cache_Start(void)
{
    Flag_Wifi_Scan_Cache_Ready = 0;
    Wifi_Scan_Cache_Update_Time_Ms = 0;
    ENTITY_LOGI("[WiFi缓存] 入口: 开始预缓存WiFi扫描, ttl:%ums\r\n",
              (unsigned int)ENTITY_WIFI_SCAN_CACHE_TTL_MS);
    if (Entity_Wifi_Sta_Scan_Start(Entity_Wifi_Cache_Scan_Done_Callback) != 0)
    {
        ENTITY_LOGE("[WiFi缓存] 失败: 启动预缓存WiFi扫描失败\r\n");
    }
}

/**
*@名称 		Entity_Wifi_Is_Scan_Cache_Ready
*@功能 		查询WiFi扫描缓存是否就绪
*@参数 		void
*@返回值 	uint8_t 1-就绪 0-未就绪
*@使用说明
*/
uint8_t Entity_Wifi_Is_Scan_Cache_Ready(void)
{
    uint32_t now_ms;
    uint32_t age_ms;

    if (!Flag_Wifi_Scan_Cache_Ready)
    {
        return 0;
    }

    now_ms = Entity_Wifi_Cache_Now_Ms();
    age_ms = now_ms - Wifi_Scan_Cache_Update_Time_Ms;
    if (age_ms >= ENTITY_WIFI_SCAN_CACHE_TTL_MS)
    {
        Flag_Wifi_Scan_Cache_Ready = 0;
        Wifi_Scan_Cache_Update_Time_Ms = 0;
        ENTITY_LOGI("[WiFi缓存] 完成: 扫描缓存已过期, age:%ums ttl:%ums\r\n",
                  age_ms,
                  (unsigned int)ENTITY_WIFI_SCAN_CACHE_TTL_MS);
        return 0;
    }

    return Flag_Wifi_Scan_Cache_Ready;
}

/**
*@名称 		Entity_Wifi_Clear_Scan_Cache
*@功能 		清除WiFi扫描缓存标志
*@参数 		void
*@返回值 	void
*@使用说明	配网完成或退出配网时调用
*/
void Entity_Wifi_Clear_Scan_Cache(void)
{
    Flag_Wifi_Scan_Cache_Ready = 0;
    Wifi_Scan_Cache_Update_Time_Ms = 0;
}

/**
*@名称 		Entity_Wifi_Ap_Start
*@功能 		启动AP模式
*@参数 		void
*@返回值 	int
*@使用说明
*/
int Entity_Wifi_Ap_Start(void)
{
    if(Entity_Wifi_Cbs.Wifi_Ap_Start)
        return Entity_Wifi_Cbs.Wifi_Ap_Start();
    return 0;
}

/**
*@名称 		Entity_Wifi_Ap_Stop
*@功能 		停止AP模式
*@参数 		void
*@返回值 	int
*@使用说明
*/
int Entity_Wifi_Ap_Stop(void)
{
    if(Entity_Wifi_Cbs.Wifi_Ap_Stop)
        return Entity_Wifi_Cbs.Wifi_Ap_Stop();
    return 0;
}

/**
*@名称 		Entity_Wifi_Ap_Mode_Config
*@功能 		设置AP模式的参数
*@参数 		char *ap_ssid, char *ap_key, const char *local_ip, const char *gw_ip, const char * ip_mask
*@返回值 	int
*@使用说明
*/
int Entity_Wifi_Ap_Mode_Config(char *ap_ssid, char *ap_key, const char *local_ip, const char *gw_ip, const char * ip_mask)
{
    if(Entity_Wifi_Cbs.Wifi_Ap_Mode_Config)
        return Entity_Wifi_Cbs.Wifi_Ap_Mode_Config(ap_ssid, ap_key, local_ip, gw_ip, ip_mask);
     return 0;
}

/**
*@名称 		Entity_Wifi_Load_Signal_Level_Quality
*@功能 		获取当前所连WIFI热点信号水平及质量百分比
*@参数 		uint8_t *level
*@参数 		uint8_t *quality
*@返回值 	int
*@使用说明	
*/
int Entity_Wifi_Load_Signal_Level_Quality(uint8_t *level, uint8_t *quality)
{
    if(Entity_Wifi_Cbs.Wifi_Load_Signal_Level_Quality)
    {
        return Entity_Wifi_Cbs.Wifi_Load_Signal_Level_Quality(level, quality);
    }
    return -200;   
}
