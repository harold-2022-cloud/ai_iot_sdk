//entity_ble_app.c
#include "entity_ble_app.h"

#include "entity_ble_transfer_protocol.h"
#include "entity_ble_gatt.h"

#include "entity_log.h"
#include "entity_wifi.h"
#include "entity_iot_func.h"
#include "entity_dev_info.h"
#include "entity_config_net.h"

#include "com_utils.h"
#include "cJSON.h"

#include <sys/time.h>
#include <string.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

static Entity_Ble_App_Send_Data_f Entity_Ble_App_Send_Data_Cb;


const char *Entity_Ble_Topic_List[] = {
    ENTITY_BLE_PROPERTY_SET_TOPIC,
    ENTITY_BLE_PROPERTY_GET_TOPIC,
    ENTITY_BLE_NETWORK_SET_TOPIC,
    ENTITY_BLE_NETWORK_GETWIFIS_TOPIC,
    ENTITY_BLE_DEV_INFO_GET_TOPIC,
    ENTITY_BLE_THING_MODEL_GET_TOPIC,
    ENTITY_BLE_GROUP_TIME_TOPIC,
    ENTITY_BLE_OTA_UPGRADE_INIT_TOPIC,
    ENTITY_BLE_OTA_FILE_INFO_TOPIC,
    ENTITY_BLE_OTA_FILE_OFFSET_TOPIC,
    ENTITY_BLE_OTA_FILE_DATA_TOPIC,
    ENTITY_BLE_OTA_COMPLETE_TOPIC,
    ENTITY_BLE_DATA_CLEAR_TOPIC,
};

typedef enum
{   
    ENTITY_BLE_PROPERTY_SET_TOPIC_INDEX,
    ENTITY_BLE_PROPERTY_GET_TOPIC_INDEX,
    ENTITY_BLE_NETWORK_SET_TOPIC_INDEX,
    ENTITY_BLE_NETWORK_GETWIFIS_TOPIC_INDEX,
    ENTITY_BLE_DEV_INFO_GET_TOPIC_INDEX,
    ENTITY_BLE_THING_MODEL_GET_TOPIC_INDEX,
    ENTITY_BLE_GROUP_TIME_TOPIC_INDEX,
    ENTITY_BLE_OTA_UPGRADE_INIT_TOPIC_INDEX,
    ENTITY_BLE_OTA_FILE_INFO_TOPIC_INDEX,
    ENTITY_BLE_OTA_FILE_OFFSET_TOPIC_INDEX,
    ENTITY_BLE_OTA_FILE_DATA_TOPIC_INDEX,
    ENTITY_BLE_OTA_COMPLETE_TOPIC_INDEX,
    ENTITY_BLE_DATA_CLEAR_TOPIC_INDEX,
}Entity_Ble_Topic_List_Index_e;


/**
*@名称 		Entity_Ble_App_Register_Send_Data_Callback
*@功能 		设置蓝牙应用层的发送回调
*@参数 		Entity_Ble_App_Send_Data_f cb
*@返回值 	void
*@使用说明	
*/
void Entity_Ble_App_Register_Send_Data_Callback(Entity_Ble_App_Send_Data_f cb)
{
    Entity_Ble_App_Send_Data_Cb = cb;
}

/**
*@名称 		Entity_Ble_Get_Time_Request
*@功能 		蓝牙获取时间的请求发送
*@参数 		Entity_Ble_App_Send_Data_f cb
*@返回值 	void
*@使用说明	
*/
void Entity_Ble_Get_Time_Request(void)
{
    cJSON *root = cJSON_CreateObject();
    if (!root)
    {
        ENTITY_LOGE("%s cJSON_CreateObject error\r\n", __func__);
        return;
    }
    unsigned int timestamp = Entity_Get_Time_Stamp();
    cJSON_AddStringToObject(root, "type", ENTITY_BLE_GROUP_RESPONSE_TIME_TOPIC);
    cJSON_AddNumberToObject(root, "ts", timestamp);
    char *json = cJSON_PrintUnformatted(root);
    if (json)
    {
        ENTITY_LOGD("%s json:%s", __func__, json);
        if(Entity_Ble_App_Send_Data_Cb)
        {
            Entity_Ble_App_Send_Data_Cb((unsigned char *)json, strlen(json));
        }
        cJSON_free(json);
    }
    cJSON_Delete(root);
}

/**
*@名称 		Entity_Ble_Topic_Property_Set_Process
*@功能 		设置属性的处理
*@参数 		cJSON *root   
*@返回值 	void    
*@使用说明	
*/
void Entity_Ble_Topic_Property_Set_Process(cJSON *root)
{
    ENTITY_LOGD("%s\r\n", __func__);
    /* TODO_RUNTIME_STUB: BLE property set topic 已解析到此，但目前未映射到 DP/product handler。 */
}

/**
*@名称 		Entity_Ble_Topic_Property_Get_Respone
*@功能 		获取属性的应答
*@参数 		const char *type,   TOPIC          
*@返回值 	void    
*@使用说明	
*/
void Entity_Ble_Topic_Property_Get_Respone(const char *type, int code)
{
    cJSON *root = cJSON_CreateObject();
    if (!root)
    {
        ENTITY_LOGE("%s cJSON_CreateObject error\r\n", __func__);
        return;
    }
    cJSON_AddStringToObject(root, "type", type);
    cJSON_AddNumberToObject(root, "code", code);
    char *json = cJSON_PrintUnformatted(root);
    if (json)
    {
        ENTITY_LOGD("%s json:%s", __func__, json);
        if(Entity_Ble_App_Send_Data_Cb)
        {
            Entity_Ble_App_Send_Data_Cb((unsigned char *)json, strlen(json));
        }
        cJSON_free(json);
    }
    cJSON_Delete(root);
}

/**
*@名称 		Entity_Ble_Topic_Property_Get_Process
*@功能 		获取属性的处理
*@参数 		cJSON *root   
*@返回值 	void    
*@使用说明	
*/
void Entity_Ble_Topic_Property_Get_Process(cJSON *root)
{
    if (!root)
        return;
    /* TODO_RUNTIME_STUB: 目前只回覆成功並觸發時間請求，未回傳實際 DP snapshot。 */
    Entity_Ble_Topic_Property_Get_Respone(ENTITY_BLE_PROPERTY_GET_RESPONSE_TOPIC, 0);
}


/**
*@名称 		Entity_Ble_Wifi_Network_Set_Respone
*@功能 		双模蓝牙配网的处理的应答
*@参数 		cJSON *root   
*@返回值 	void    
*@使用说明	
*/
void Entity_Ble_Wifi_Network_Set_Respone(int code)
{
    cJSON *object = cJSON_CreateObject();
    if (!object)
    {
        ENTITY_LOGE("%s cJSON_CreateObject error\r\n", __func__);
        return;
    }
    char *msgId = Random_MsgId();
    if (msgId == NULL)
    {
        ENTITY_LOGE("%s msgId is null\r\n", __func__);
        cJSON_Delete(object);
        return;
    }
    unsigned int timestamp = Entity_Get_Time_Stamp();
    cJSON_AddStringToObject(object, "type", ENTITY_BLE_NETWORK_SET_RESPONSE_TOPIC);
    cJSON_AddNumberToObject(object, "code", code);
    cJSON_AddStringToObject(object, "msgId", msgId);
    cJSON_AddNumberToObject(object, "ts", timestamp);
    char *json = cJSON_PrintUnformatted(object);
    if (json)
    {
        ENTITY_LOGD("%s json:%s", __func__, json);
        if(Entity_Ble_App_Send_Data_Cb)
            Entity_Ble_App_Send_Data_Cb((unsigned char *)json, strlen(json));
        cJSON_free(json);
    }
    cJSON_free(msgId);
    cJSON_Delete(object);
}

/**
*@名称 		Entity_Ble_Wifi_Network_Set_Process
*@功能 		双模蓝牙配网的处理
*@参数 		cJSON *data     
*@返回值 	void    
*@使用说明	
*/
int Entity_Ble_Wifi_Network_Set_Process(cJSON *data)
{
    int code = 0;
    uint8_t is_force_bind = 0;//强绑标志
    uint32_t iport = 1883;

    Entity_Config_Net_Info_t *Config_Net_info = Entity_Get_Config_Net_Info();
    Entity_App_Param_t *App_Param = Get_Entity_App_Param();
    Entity_Dev_Config_Net_Info_t *Dev_Config_Net_info = &App_Param->Dev_Config_Net_Info;

    cJSON *force_bind = cJSON_GetObjectItem(data, "force_bind");//是否强制绑定设备
    if (force_bind && force_bind->valueint)
    {
        is_force_bind = 1;
    }
    cJSON *sid = cJSON_GetObjectItem(data, "sid");//路由器ssid
    if (!sid || !sid->valuestring || !sid->valuestring[0])
    {
        code = ENTITY_BLE_DATA_JSON_PARSER_ERR;
        goto xfail;
    }
    cJSON *pw = cJSON_GetObjectItem(data, "pw");//路由器password
    if (!pw || !pw->valuestring || !pw->valuestring[0])
    {
        code = ENTITY_BLE_DATA_JSON_PARSER_ERR;
        goto xfail;
    }
    cJSON *bid = cJSON_GetObjectItem(data, "bid");//bind id
    if (!bid || !bid->valuestring || !bid->valuestring[0])
    {
        code = ENTITY_BLE_DATA_JSON_PARSER_ERR;
        goto xfail;
    }
    cJSON *mq = cJSON_GetObjectItem(data, "mq");//mqtt host
    if (!mq || !mq->valuestring || !mq->valuestring[0])
    {
        code = ENTITY_BLE_DATA_JSON_PARSER_ERR;
        goto xfail;
    }
    cJSON *port = cJSON_GetObjectItem(data, "port");//mqtt port
    if (!port)
    {
        iport = 1883;
    }
    else
    {
        iport = port->valueint;
    }

    cJSON *userId = cJSON_GetObjectItem(data,"userId");// ipc配网绑定
    if(!userId || !userId->valuestring || !userId->valuestring[0])
    {
        code = ENTITY_BLE_DATA_JSON_PARSER_ERR;
        goto xfail;
    }

    ENTITY_LOGI("sid=%s, pw_len:%u, mq=%s:%d\r\n",
              sid->valuestring,
              (unsigned int)strlen(pw->valuestring),
              mq->valuestring,
              iport);
    cJSON *country = cJSON_GetObjectItem(data, "country");
    if (country && country->valuestring && country->valuestring[0])
    {
        strcpy(Config_Net_info->Country_Code, country->valuestring);
    }
    else
    {
        strcpy(Config_Net_info->Country_Code, "CN");
    }

    cJSON *tz = cJSON_GetObjectItem(data, "tz");
    if (tz && tz->valuestring && tz->valuestring[0])
    {
        strcpy(Config_Net_info->User_Tz_Str, tz->valuestring);
    }
    else
    {
        strcpy(Config_Net_info->User_Tz_Str, "Asia/Shanghai");
    }

    if(is_force_bind)
        ENTITY_LOGI("force_bind\r\n");

    //更新数据：WiFi 凭据与绑定判断解耦，无论是否已绑定都先写入 WiFi/MQTT 凭据
    memset(Config_Net_info->Wifi_Info.Ssid, 0, sizeof(Config_Net_info->Wifi_Info.Ssid));
    memset(Config_Net_info->Wifi_Info.Key, 0, sizeof(Config_Net_info->Wifi_Info.Key));
    memset(Config_Net_info->Mqtt_Info.Mqtt_Host, 0, sizeof(Config_Net_info->Mqtt_Info.Mqtt_Host));
    memset(Config_Net_info->Bind_Id, 0, sizeof(Config_Net_info->Bind_Id));
    memset(Config_Net_info->User_Id, 0, sizeof(Config_Net_info->User_Id));// ipc配网绑定特有

    strncpy(Config_Net_info->Mqtt_Info.Mqtt_Host, mq->valuestring, sizeof(Config_Net_info->Mqtt_Info.Mqtt_Host) - 1);
    Config_Net_info->Mqtt_Info.Mqtt_Port = iport;

    strncpy(Config_Net_info->Wifi_Info.Ssid, sid->valuestring, sizeof(Config_Net_info->Wifi_Info.Ssid) - 1);
    strncpy(Config_Net_info->Wifi_Info.Key, pw->valuestring, sizeof(Config_Net_info->Wifi_Info.Key) - 1);
    strncpy(Config_Net_info->Bind_Id, bid->valuestring, sizeof(Config_Net_info->Bind_Id) - 1);
    strncpy(Config_Net_info->User_Id, userId->valuestring, sizeof(Config_Net_info->User_Id) - 1);// ipc配网绑定特有

    Dev_Config_Net_info->Bind_Type = WIFI_BIND_TYPE; //WIFI绑定
    //Config_Net_info->Flag_Bind = 1; //已绑定标志 连上MQTT后再置1

    Entity_Save_Config_Net_Info_To_Flash();//保存设备信息到FALSH

    /* 驗證寫入 Flash 的配網資訊 */
    ENTITY_LOGI("[BLE配網] Flash寫入完成 ssid=\"%s\" passwd_len=%u bid=%s userId=%s\r\n",
              Config_Net_info->Wifi_Info.Ssid,
              (unsigned int)strlen(Config_Net_info->Wifi_Info.Key),
              Config_Net_info->Bind_Id,
              Config_Net_info->User_Id);

    //非强绑且已绑定：WiFi 已更新，仅跳过绑定流程，返回 BIND_FAIL 通知 App 无需重复绑定
    if(!is_force_bind && Dev_Config_Net_info->Flag_Bind)
    {
        ENTITY_LOGI("[BLE配網] 設備已綁定，跳過綁定流程（WiFi 凭据已更新）：is_force_bind=%d bind=%d\r\n",
                  is_force_bind, Dev_Config_Net_info->Flag_Bind);
        code = ENTITY_BLE_DATA_BIND_FAIL;
        goto xfail;
    }

    ENTITY_LOGI("%s, net config success\r\n", __FUNCTION__);
    return code;
xfail:
    ENTITY_LOGI("xfail\n");
    return code;
}



/**
*@名称 		Entity_Single_Ble_Network_Set_Process
*@功能 		单蓝牙配网的处理
*@参数 		cJSON *data     
*@返回值 	void    
*@使用说明	
*/
void Entity_Single_Ble_Network_Set_Process(cJSON *data)
{
#if 0
    char *connect_status = cJSON_GetObjectItem(root, "ble")->valuestring;
    const char *connect_status_list[] = {"bind", "unbind", "init"};
    const unsigned short connect_code_list[] = {BLE_DATA_BIND_OK, BLE_DATA_UNBIND_OK, BLE_DATA_INIT_OK};
    const char *msg_list[] = {"bind success", "unbind success", "init success"};
    unsigned char index = array_indexOf(connect_status, connect_status_list, sizeof(connect_status_list) / sizeof(char *));

    //_ble_response(0, ENTITY_BLE_NETWORK_SET_RESPONSE_TOPIC, connect_code_list[index], (char *)msg_list[index]);
    switch (index)
    {
    case 0:
        break;
    case 1:
    case 2:
        break;
    default:
        break;
    }
#endif
}


/**
*@名称 		Entity_Ble_Topic_NetWork_Set_Process
*@功能 		设置网络信息的处理
*@参数 		cJSON *root      
*@返回值 	void    
*@使用说明	
*/
void Entity_Ble_Topic_NetWork_Set_Process(cJSON *root)
{
    int code = 0;
    if (!root)
        return;
    cJSON *data = cJSON_GetObjectItem(root, "data");
    if(!data)
        return;
    cJSON *ble = cJSON_GetObjectItem(data, "ble");
    if (ble)//单蓝牙配网
    {
        Entity_Single_Ble_Network_Set_Process(data);
    }
    else//双模 给WIFI配网
    {
        code = Entity_Ble_Wifi_Network_Set_Process(data);
        Entity_Ble_Wifi_Network_Set_Respone(code);
        if(code == 0)//配网成功，则连接WIFI
        {
            Entity_Config_Net_Info_t *Config_Net_info = Entity_Get_Config_Net_Info();
            
            Entity_Wifi_Config_Net_Info(Config_Net_info->Wifi_Info.Ssid, Config_Net_info->Wifi_Info.Key);
            Entity_Set_Net_Mode_Msg_Send(ENTITY_NET_MODE_CONFIG_TRY_CONNECT);
        }
    }
}


/**
*@名称 		Entity_Ble_NetWork_Get_wifis_Response
*@功能 		WIFI扫描完成的应答
*@参数 		void *res
*@返回值 	void
*@使用说明	
*/
void Entity_Ble_NetWork_Get_wifis_Response(void *res)
{
    Entity_Wifi_Sta_Scan_Result_t *result = (Entity_Wifi_Sta_Scan_Result_t *)res;
    Entity_Wifi_Sta_Scan_Result_t empty_result = {0};
    if (result == NULL)
    {
        ENTITY_LOGE("[WiFi缓存] 失败: WiFi列表应答收到空扫描结果, 降级返回空列表\r\n");
        result = &empty_result;
    }

    ENTITY_LOGI("%s ap num:%d\r\n", __FUNCTION__, result->Num);
    cJSON *root = cJSON_CreateObject();
    cJSON *object = cJSON_CreateObject();
    cJSON *array = cJSON_CreateArray();
    char *msgId = Random_MsgId();
    uint8_t code = 0;

    if (!root || !object || !array)
    {
        ENTITY_LOGE("cJSON_CreateObject error");
        cJSON_Delete(object);
        cJSON_Delete(array);
        goto quit;
    }
    if (msgId == NULL)
    {
        ENTITY_LOGE("msgId is null\r\n");
        cJSON_Delete(object);
        cJSON_Delete(array);
        goto quit;
    }
    unsigned int timestamp = Entity_Get_Time_Stamp();
    cJSON_AddStringToObject(root, "type", ENTITY_BLE_NETWORK_GETWIFIS_RESPONSE_TOPIC);
    cJSON_AddStringToObject(root, "msgId", msgId);
    cJSON_AddNumberToObject(root, "code", code);
    cJSON_AddNumberToObject(root, "ts", timestamp);

 
    for (int i = 0; i < result->Num; i++)
    {
        cJSON *wifi_obj = cJSON_CreateObject();
        cJSON_AddStringToObject(wifi_obj, "ssid", result->Ap_Infos[i].Ssid);
        cJSON_AddNumberToObject(wifi_obj, "rssi", result->Ap_Infos[i].Rssi);
        cJSON_AddBoolToObject(wifi_obj, "security", result->Ap_Infos[i].Auth == ENTITY_WIFI_SECURITY_OPEN?0:1);
        cJSON_AddItemToArray(array, wifi_obj);
    }
    cJSON_AddItemToObject(object, "wifis", array);
    cJSON_AddItemToObject(root, "data", object);
    char *json = cJSON_PrintUnformatted(root);
    if (json)
    {
        int send_ret = -1;
        unsigned int json_len = (unsigned int)strlen(json);
        ENTITY_LOGD("%s json:%s", __func__, json);
        if(Entity_Ble_App_Send_Data_Cb)
        {
            send_ret = Entity_Ble_App_Send_Data_Cb((unsigned char *)json, json_len);
        }
        ENTITY_LOGI("[BLE_WIFI_LIST] send len=%u ap_num=%u ret=%d\r\n",
                    json_len,
                    (unsigned int)result->Num,
                    send_ret);
        cJSON_free(json);
    }
quit:
    cJSON_free(msgId);
    cJSON_Delete(root);
}

/**
*@名称 		Entity_Ble_NetWork_Getwifis_Process
*@功能 		获取WIFI列表消息解析
*@参数 		cJSON *root      
*@返回值 	void    
*@使用说明	
*/
int Entity_Ble_NetWork_Getwifis_Process(cJSON *root)
{
    int need_scan = 0;
    uint8_t cache_ready;
    unsigned char cache_num;
    cJSON *scan = cJSON_GetObjectItem(root, "scan");    
    if (scan && scan->valueint)
        need_scan = 1;   

    cache_ready = Entity_Wifi_Is_Scan_Cache_Ready();
    cache_num = Entity_Wifi_Get_Scan_Result_Num();
    if(need_scan == 0 && (!cache_ready || !cache_num))//不需要立即扫描，但是缓存过期或没有WIFI列表信息则也需要启动扫描
        need_scan = 1; 
        
    Entity_Wifi_Cbs_t *wifi_cbs = Get_Entity_Wifi_Cbs();
    if(need_scan)
    {
        ENTITY_LOGI("[WiFi缓存] 入口: 手机请求扫描WiFi, 触发新扫描, scan:%d ready:%d AP数量:%d\r\n",
                  need_scan,
                  cache_ready,
                  cache_num);
        if (Entity_Wifi_Sta_Scan_Start(Entity_Ble_NetWork_Get_wifis_Response) != 0)
        {
            Entity_Wifi_Sta_Scan_Result_t fallback_result = {0};
            ENTITY_LOGE("[WiFi缓存] 失败: 手机请求触发WiFi扫描失败, 尝试返回已有缓存\r\n");
            if (Entity_Wifi_Copy_Scan_Results(&fallback_result) == 0)
            {
                ENTITY_LOGI("[WiFi缓存] 完成: 降级返回已有WiFi列表, AP数量:%d\r\n", fallback_result.Num);
                Entity_Ble_NetWork_Get_wifis_Response(&fallback_result);
            }
            else
            {
                ENTITY_LOGE("[WiFi缓存] 失败: 无可用WiFi缓存, 返回空WiFi列表\r\n");
                Entity_Ble_NetWork_Get_wifis_Response(&fallback_result);
            }
        }
    }
    else
    {
        Entity_Wifi_Sta_Scan_Result_t cached_result = {0};
        if(Entity_Wifi_Copy_Scan_Results(&cached_result) == 0)
        {
            ENTITY_LOGI("[WiFi缓存] 完成: 命中缓存, 直接返回WiFi列表, AP数量:%d\r\n", cached_result.Num);
            Entity_Ble_NetWork_Get_wifis_Response(&cached_result);
        }
        else if(wifi_cbs->Wifi_Get_Scan_Results)
        {
            Entity_Wifi_Sta_Scan_Result_t *result = wifi_cbs->Wifi_Get_Scan_Results();
            if (result)
            {
                ENTITY_LOGI("[WiFi缓存] 完成: 命中旧接口缓存, 直接返回WiFi列表, AP数量:%d\r\n", result->Num);
                Entity_Ble_NetWork_Get_wifis_Response(result);
            }
        }
    }
    return 0;
}


/**
*@名称 		Entity_Ble_Dev_Info_Get_Process
*@功能 		获取设备信息
*@参数 		void    
*@返回值 	void    
*@使用说明	
*/
void Entity_Ble_Dev_Info_Get_Process(void)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *object = cJSON_CreateObject();
    if (!root || !object)
    {
        ENTITY_LOGE("%s cJSON_CreateObject error", __func__);
        cJSON_Delete(root);
        return;
    }
    char str_addr[18] = {0};
    uint8_t wifi_mac[6]={0};
    uint8_t ble_mac[6]={0};
    Entity_App_Param_t *app_param = Get_Entity_App_Param();
    Entity_Wifi_Cbs_t *wifi_cbs = Get_Entity_Wifi_Cbs();
    if(wifi_cbs->Wifi_Get_Macaddr)
        wifi_cbs->Wifi_Get_Macaddr(wifi_mac);
    Entity_Ble_Cbs_t *ble_cbs = Get_Entity_Ble_Cbs();
    if(ble_cbs->Ble_Get_Local_Addr)
        ble_cbs->Ble_Get_Local_Addr(ble_mac);
    cJSON_AddStringToObject(root, "type", ENTITY_BLE_DEV_INFO_GET_RESPONSE_TOPIC);
    unsigned int timestamp = Entity_Get_Time_Stamp();
    cJSON_AddNumberToObject(root, "ts", timestamp);
    cJSON_AddStringToObject(object, "version", app_param->Dev_Info.Dev_Version);
    //cJSON_AddStringToObject(object, "mcuVersion", app_param->Dev_Info.Mcu_Version);
    cJSON_AddStringToObject(object, "msgType", "poweron");
    cJSON_AddStringToObject(object, "pid", app_param->Dev_Triple_Info.Triple_Info.Pid);
    if (app_param->Dev_Config_Net_Info.Flag_Bind)
        cJSON_AddTrueToObject(object, "bind");
    else
        cJSON_AddFalseToObject(object, "bind");
    cJSON_AddNumberToObject(object, "gateway", 0);

    sprintf(str_addr, "%02x:%02x:%02x:%02x:%02x:%02x", wifi_mac[0], wifi_mac[1], wifi_mac[2], wifi_mac[3], wifi_mac[4], wifi_mac[5]);
    cJSON_AddStringToObject(object, "wifi_mac", str_addr);
    sprintf(str_addr, "%02x:%02x:%02x:%02x:%02x:%02x", ble_mac[0], ble_mac[1], ble_mac[2], ble_mac[3], ble_mac[4], ble_mac[5]);
    cJSON_AddStringToObject(object, "ble_mac", str_addr);

    //cJSON_AddBoolToObject(object, "thing_model", app_param->Dev_Thing_Model_Info.Have_Model);
    //cJSON_AddBoolToObject(object, "get_thing_model", !app_param->Dev_Thing_Model_Info.Have_Model);

    cJSON_AddItemToObject(root, "data", object);
    char *json = cJSON_PrintUnformatted(root);
    if (json)
    {
        ENTITY_LOGD("%s json:%s", __func__, json);
        if(Entity_Ble_App_Send_Data_Cb)
            Entity_Ble_App_Send_Data_Cb((unsigned char *)json, strlen(json));
        cJSON_free(json);
    }
    cJSON_Delete(root);
}

/**
*@名称 		Entity_Ble_Sys_Time_Get_Process
*@功能 		获取时间信息
*@参数 		cJSON *root  
*@返回值 	void    
*@使用说明	
*/
void Entity_Ble_Sys_Time_Get_Process(cJSON *root)
{
    cJSON *data = cJSON_GetObjectItem(root, "data");
    if (data == NULL)
        return;
    cJSON *ts = cJSON_GetObjectItem(data, "ts");
    if(!ts)
        return;
    unsigned int timestamp = ts->valueint;
    ENTITY_LOGD("[%s] timestamp:%d\r\n", __func__,timestamp);
    cJSON *tz = cJSON_GetObjectItem(data, "zone_offset");
    if(!tz)
        return;
    int zone_offset = cJSON_GetObjectItem(root, "zone_offset")->valueint;
    ENTITY_LOGD("[%s] zone_offset:%d\r\n", __func__,zone_offset);
    cJSON *sys_tz = cJSON_GetObjectItem(data, "sys_tz");
    if(!sys_tz)
        return;
    char *sys_tz_str = sys_tz->valuestring;
    ENTITY_LOGD("[%s] sys_tz:%s\r\n", __func__, sys_tz_str);

    /* TODO_RUNTIME_STUB: 已解析時間/時區，但尚未套用到系統時間或保存 timezone。 */
    (void)timestamp; (void)zone_offset; (void)sys_tz_str;

}



/**
*@名称 		Entity_Ble_Data_Clear_Respone
*@功能 		清除数据的应答
*@参数 		int status
*@返回值 	void    
*@使用说明	
*/
void Entity_Ble_Data_Clear_Respone(int status)
{
    cJSON *object = cJSON_CreateObject();
    if(!object) {
        ENTITY_LOGE("%s cJSON_CreateObject object error\r\n", __func__);
        return;
    }
    cJSON *root = cJSON_CreateObject();
    if (!root)
    {
        cJSON_Delete(object);
        ENTITY_LOGE("%s cJSON_CreateObject root error\r\n", __func__);
        return;
    }
    unsigned int timestamp = Entity_Get_Time_Stamp();
    cJSON_AddStringToObject(root, "type", ENTITY_BLE_DATA_CLEAR_RESPONSE_TOPIC);
    cJSON_AddNumberToObject(root, "ts", timestamp);
    cJSON_AddNumberToObject(object, "status", status);
    cJSON_AddItemToObject(root, "data", object);
    char *json = cJSON_PrintUnformatted(root);
    if (json)
    {
        ENTITY_LOGD("%s json:%s", __func__, json);
        if(Entity_Ble_App_Send_Data_Cb)
            Entity_Ble_App_Send_Data_Cb((unsigned char *)json, strlen(json));
        cJSON_free(json);
    }
    cJSON_Delete(root);
}


/**
*@名称 		Entity_Ble_Data_Clear_Process
*@功能 		清除数据的解析处理
*@参数 		cJSON *root   
*@返回值 	void    
*@使用说明	
*/
void Entity_Ble_Data_Clear_Process(cJSON *root)
{
    cJSON *data = cJSON_GetObjectItem(root, "data");
    if (data == NULL)
        return;
    int status=0;
    cJSON *flag_json = cJSON_GetObjectItem(data, "flag");
    if(flag_json == NULL)
        return;
    uint8_t flag = flag_json->valueint;
    if(flag == 0)
    {
        ENTITY_LOGD("[%s] now, user data clear\r\n", __func__);
        /* TODO_RUNTIME_STUB: 已收到清除請求，但尚未清理 alarm/todo/cache 等產品資料。 */

    }
    else
    {
        status = 1;//失败
    }
    Entity_Ble_Data_Clear_Respone(status);//应答
}

/**
*@名称 		Entity_Ble_App_Msg_Process
*@功能 		蓝牙消息应用层的处理
*@参数 		unsigned char *msg, unsigned short msg_len
*@返回值 	void
*@使用说明	
*/
void Entity_Ble_App_Msg_Process(unsigned char *msg, unsigned short msg_len)
{
    cJSON *root = cJSON_Parse((char*)msg);

    if (!root || !cJSON_IsObject(root))
    {
        ENTITY_LOGE("%s cJSON_CreateObject error\r\n", __func__);
        return;
    }
    ENTITY_LOGI("[%s] ble msg:%s", __func__, msg);

    char *type = cJSON_GetObjectItem(root, "type")->valuestring;
    switch (Array_Index_Of(type, Entity_Ble_Topic_List, sizeof(Entity_Ble_Topic_List) / sizeof(char *)))
    {
    case ENTITY_BLE_PROPERTY_SET_TOPIC_INDEX:     //设置属性
        Entity_Ble_Topic_Property_Set_Process(root);
        break;
    case ENTITY_BLE_PROPERTY_GET_TOPIC_INDEX:     //获取属性,收到此指令后，立即发送获取时间指令
        Entity_Ble_Topic_Property_Get_Process(root);
        Entity_Ble_Get_Time_Request();   //请求时间
        break;
    case ENTITY_BLE_NETWORK_SET_TOPIC_INDEX:      //设置配网信息
        Entity_Ble_Topic_NetWork_Set_Process(root);
        break;
    case ENTITY_BLE_NETWORK_GETWIFIS_TOPIC_INDEX: //获取WIFI列表
        Entity_Ble_NetWork_Getwifis_Process(root);
        break;
    case ENTITY_BLE_DEV_INFO_GET_TOPIC_INDEX:     //获取设备信息
        Entity_Ble_Dev_Info_Get_Process();
        break;
    case ENTITY_BLE_THING_MODEL_GET_TOPIC_INDEX:  //获取物模型
        //rlink_ble_thing_model_get_handle(msg);
        break;
    case ENTITY_BLE_GROUP_TIME_TOPIC_INDEX://获取时间的处理
        Entity_Ble_Sys_Time_Get_Process(root);
        break;
    case ENTITY_BLE_DATA_CLEAR_TOPIC_INDEX:
        Entity_Ble_Data_Clear_Process(root);
        break;
    case ENTITY_BLE_OTA_UPGRADE_INIT_TOPIC_INDEX:
        /* TODO_RUNTIME_STUB: BLE OTA topic 保留，尚未實作 initiate 流程。 */
        break;
    case ENTITY_BLE_OTA_FILE_INFO_TOPIC_INDEX:
        /* TODO_RUNTIME_STUB: BLE OTA topic 保留，尚未實作 file info 流程。 */
        break;
    case ENTITY_BLE_OTA_FILE_OFFSET_TOPIC_INDEX:
        /* TODO_RUNTIME_STUB: BLE OTA topic 保留，尚未實作 file offset 流程。 */
        break;
    case ENTITY_BLE_OTA_FILE_DATA_TOPIC_INDEX:
        /* TODO_RUNTIME_STUB: BLE OTA topic 保留，尚未實作 file data 寫入流程。 */
        break;
    case ENTITY_BLE_OTA_COMPLETE_TOPIC_INDEX:
        /* TODO_RUNTIME_STUB: BLE OTA topic 保留，尚未實作 complete/校驗流程。 */
        break;
    default:
        break;
    }
    cJSON_Delete(root);
}







