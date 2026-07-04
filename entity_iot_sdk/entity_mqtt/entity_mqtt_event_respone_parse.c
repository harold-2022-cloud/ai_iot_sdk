//entity_mqtt_event_respone_parse.c
#include "entity_mqtt_event_respone_parse.h"

#include "entity_mqtt_v2_define.h"
#include "entity_mqtt_event_report.h"
#include "entity_mqtt_app.h"
#include "entity_iot_cloud.h"

#include "entity_log.h"
#include "entity_wifi.h"
#include "entity_iot_func.h"
#include "entity_error_code.h"
#include "entity_dev_info.h"
#include "entity_config_net.h"

#include "com_utils.h"
#include "ai_dialog_diag.h"
#include "cJSON.h"

#include <sys/time.h>
#include <string.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>


static Entity_Mqtt_Event_Respone_Parse_Cbs_t Entity_Mqtt_Event_Respone_Parse_Cbs;



const char *Mqtt_Event_Code_List[] = {
    ENTITY_EVENT_RESET_CODE,
    ENTITY_EVENT_TIME_CODE,
    ENTITY_EVENT_MODEL_CODE,
    ENTITY_EVENT_CONFIG_CODE,
    ENTITY_EVENT_BIND_CODE,
    ENTITY_EVENT_INFO_CODE,
    ENTITY_EVENT_PROPERTY_REPORT_CODE,
    ENTITY_EVENT_OTA_PROGRESS_CODE,
    ENTITY_EVENT_SUB_BIND_CODE,
    ENTITY_EVENT_SUB_DELETE_CODE,
    ENTITY_EVENT_SUB_LOGIN_CODE,
    ENTITY_EVENT_SUB_LOGINOUT_CODE,
    ENTITY_EVENT_SUB_MODEL_CODE,
    ENTITY_EVENT_SUB_PROPERTY_REPORT_CODE,
    ENTITY_EVENT_LOCAL_CONFIG_CODE,
    ENTITY_EVENT_SUB_LIST_CODE,
    ENTITY_EVENT_IPC_CLOUD_CODE,
    ENTITY_EVENT_P2P_CONFIG_CODE,
    ENTITY_EVENT_AGORA_TOKEN_CODE,
    ENTITY_EVENT_DIY_EVENT_CODE,
    ENTITY_EVENT_IPC_TOKEN_BIND_CODE,
    ENTITY_EVENT_FIND_ALERT_CODE,
    ENTITY_EVENT_FIND_REPORT_CODE,
    ENTITY_EVENT_IPC_LIVE_GET_CODE,
    ENTITY_EVENT_AGORA_AGENT_NFC_REPORT_CODE,
    ENTITY_EVENT_AGORA_RTC_STOP_REPORT_CODE,
    ENTITY_EVENT_AGORA_AGENT_DEVICE_ACCESS_CODE
};

//事件码对应序号，与mqtt_event_code_list保持一致
typedef enum
{
    ENTITY_EVENT_RESET_CODE_INDEX,
    ENTITY_EVENT_TIME_CODE_INDEX,
    ENTITY_EVENT_MODEL_CODE_INDEX,
    ENTITY_EVENT_CONFIG_CODE_INDEX,
    ENTITY_EVENT_BIND_CODE_INDEX,
    ENTITY_EVENT_INFO_CODE_INDEX,
    ENTITY_EVENT_PROPERTY_REPORT_CODE_INDEX,
    ENTITY_EVENT_OTA_PROGRESS_CODE_INDEX,
    ENTITY_EVENT_SUB_BIND_CODE_INDEX,
    ENTITY_EVENT_SUB_DELETE_CODE_INDEX,
    ENTITY_EVENT_SUB_LOGIN_CODE_INDEX,
    ENTITY_EVENT_SUB_LOGINOUT_CODE_INDEX,
    ENTITY_EVENT_SUB_MODEL_CODE_INDEX,
    ENTITY_EVENT_SUB_PROPERTY_REPORT_CODE_INDEX,
    ENTITY_EVENT_LOCAL_CONFIG_CODE_INDEX,
    ENTITY_EVENT_SUB_LIST_CODE_INDEX,
    ENTITY_EVENT_IPC_CLOUD_CODE_INDEX,
    ENTITY_EVENT_P2P_CONFIG_CODE_INDEX,
    ENTITY_EVENT_AGORA_TOKEN_CODE_INDEX,
    ENTITY_EVENT_DIY_EVENT_CODE_INDEX,
    ENTITY_EVENT_IPC_TOKEN_BIND_CODE_INDEX,
    ENTITY_EVENT_FIND_ALERT_CODE_INDEX,
    ENTITY_EVENT_FIND_REPORT_CODE_INDEX,
    ENTITY_EVENT_IPC_LIVE_GET_CODE_INDEX,
    ENTITY_EVENT_AGORA_AGENT_NFC_REPORT_CODE_INDEX,
    ENTITY_EVENT_AGORA_RTC_STOP_REPORT_CODE_INDEX,
    ENTITY_EVENT_AGORA_AGENT_DEVICE_ACCESS_CODE_INDEX,
    ENTITY_EVENT_CODE_INDEX_NUM,
}Entity_Event_Code_Index_e; 



/**
*@名称 		Entity_Mqtt_Event_Respone_Parse_Cbs_Init
*@功能 		mqtt事件消息云端应答数据处理回调注册
*@参数 		Entity_Mqtt_Event_Respone_Parse_Cbs_t *cbs
*@返回值 	void
*@使用说明	
*/
void Entity_Mqtt_Event_Respone_Parse_Cbs_Init(Entity_Mqtt_Event_Respone_Parse_Cbs_t *cbs)
{
    Entity_Mqtt_Event_Respone_Parse_Cbs = *cbs;
}

/**
*@名称 		Entity_Mqtt_Event_Respone_Time_Parse
*@功能 		云端下发时间的处理
*@参数 		cJSON *root
*@返回值 	void
*@使用说明	
*/
void Entity_Mqtt_Event_Respone_Time_Parse(cJSON *root)
{
    cJSON *data = cJSON_GetObjectItem(root, "data");
    if(!data)
        return;
    cJSON *cjson_ts = cJSON_GetObjectItem(data, "ts");//时间戳
    cJSON *cjson_zone = cJSON_GetObjectItem(data, "zone_offset");//时区偏移量 秒
    cJSON *cjson_tz = cJSON_GetObjectItem(data, "sys_tz");//时区字符串 例："America/New_York"
    if(!cJSON_IsNumber(cjson_ts) || !cJSON_IsNumber(cjson_zone) || !cJSON_IsString(cjson_tz))
    {
        ENTITY_LOGE("%s invalid time response fields\r\n", __func__);
        return;
    }
    unsigned int timestamp = cjson_ts->valueint;
    int zone_offset = cjson_zone->valueint;
    char *sys_tz_str = cjson_tz->valuestring;
    ENTITY_LOGD("timestamp:%d, zone_offset:%d, sys_tz_str:%s\r\n", timestamp, zone_offset, sys_tz_str);
    Entity_Set_Time_Stamp(timestamp);
    if(Entity_Mqtt_Event_Respone_Parse_Cbs.Event_Time_Parse_Cb)
    {
        Entity_Mqtt_Event_Respone_Parse_Cbs.Event_Time_Parse_Cb(timestamp, zone_offset, sys_tz_str);
    }
    else
    {
        Entity_Set_Zone_Offset(zone_offset);
    }
}

/**
*@名称 		Thing_Model_Complete_Extract_Mini
*@功能 		将完整版的物模型转换成自定义的迷你版 避免信息丢失
*@参数 		cJSON *item
*@返回值 	cJSON*                
*@使用说明	
*/
cJSON* Thing_Model_Complete_Extract_Mini(cJSON *item) 
{
    cJSON *dpBusiId = cJSON_GetObjectItem(item, "dpBusiId");
    cJSON *dataType = cJSON_GetObjectItem(item, "dataType");
    cJSON *specs = cJSON_GetObjectItemCaseSensitive(dataType, "specs");//参数信息
    cJSON *dataTypeStr = cJSON_GetObjectItem(item, "dataTypeStr");//数据类型
    cJSON *identifier = cJSON_GetObjectItem(item, "identifier");//标识符
    cJSON *accessMode = cJSON_GetObjectItem(item, "accessMode");//属性读写类型：只读（r）或读写（rw）
    if (!cJSON_IsString(dpBusiId) || !specs ||
        !cJSON_IsString(dataTypeStr) || !cJSON_IsString(identifier) ||
        !cJSON_IsString(accessMode))
    {
        ENTITY_LOGW("%s invalid model property item, drop\r\n", __func__);
        return NULL;
    }

    // Create a new JSON object for the mini format
    cJSON *output_item = cJSON_CreateObject();
    if(output_item == NULL)
    {
        ENTITY_LOGD("%s cJSON_CreateObject faild\r\n",__func__ );
        return NULL;
    }
    cJSON_AddItemToObject(output_item, "b", cJSON_CreateString(dpBusiId->valuestring));//BSSID
    cJSON_AddItemToObject(output_item, "s", cJSON_Duplicate(specs, 1));//数据类型中的详细参数信息
    cJSON_AddItemToObject(output_item, "t", cJSON_CreateString(dataTypeStr->valuestring));//数据类型字符串
    cJSON_AddItemToObject(output_item, "i", cJSON_CreateString(identifier->valuestring));//标识符
    cJSON_AddItemToObject(output_item, "m", cJSON_CreateString(accessMode->valuestring));//属性读写类型：只读（r）或读写（rw）
    return output_item;
}


/**
*@名称 		Entity_Mqtt_Event_Respone_Model_Process
*@功能 		云端下发物模型的处理
*@参数 		cJSON *root
*@返回值 	void
*@使用说明	
*/
void Entity_Mqtt_Event_Respone_Model_Process(const cJSON *root)
{
    cJSON *data = cJSON_GetObjectItem(root, "data");
    if(!data)
        return;
    cJSON *iot_properties = Entity_Mqtt_Get_Dev_Thing_Model_Cjson();
    if(iot_properties)//已经有物模型了，则不处理
        return ;
    cJSON *model_properties = cJSON_CreateArray();
     if(model_properties == NULL)
    {
        ENTITY_LOGE("%s cJSON_CreateArray faild\r\n", __func__);
        return;
    }
    cJSON *item = NULL;
    cJSON_ArrayForEach(item, cJSON_GetObjectItem(data, "properties"))//遍历属性数组 
    {
        cJSON* cjson_mini = Thing_Model_Complete_Extract_Mini(item);//转成MINI后再保存
        if(cjson_mini)
        {
            cJSON_AddItemToArray(model_properties, cjson_mini);
        }
    }
    iot_properties = cJSON_Duplicate(model_properties, 1);//申请新空间再复制
    if(iot_properties == NULL)
    {
        ENTITY_LOGE("%s cJSON_Duplicate faild\r\n", __func__);
        cJSON_Delete(model_properties);
        return;
    }
    cJSON_Delete(model_properties);

    Entity_Dev_Thing_Model_Info_t *model_info = Get_Dev_Thing_Model_Info();
    char *content = cJSON_PrintUnformatted(iot_properties);
    if(content == NULL)
    {
        ENTITY_LOGE("%s cJSON_PrintUnformatted faild\r\n", __func__);
        return;
    }
    if(strlen(content) > 0)//有物模型数据
    {
        strncpy(model_info->Content, content, sizeof(model_info->Content)-1);
        model_info->Len = strlen(model_info->Content);
        ENTITY_LOGD("model string len:%d, model:%s", model_info->Len, model_info->Content);
        if(model_info->Len)
        {
            model_info->Have_Model = 1;
            Entity_Save_Thing_Model_Info_To_Flash(); 
            Entity_App_Param_t *app_param = Get_Entity_App_Param();
            app_param->Dev_Info.Thing_Model_Properties = cJSON_Parse(model_info->Content);
            if(app_param->Dev_Info.Thing_Model_Properties == NULL)
            {
                ENTITY_LOGE("%s model_info->Content cJSON_Parse faild\r\n", __func__);
            }
        }
        cJSON_free(content);
        if(Entity_Mqtt_Event_Respone_Parse_Cbs.Event_Model_Parse_Cb)
            Entity_Mqtt_Event_Respone_Parse_Cbs.Event_Model_Parse_Cb(iot_properties); 
    }
    else
    {
        cJSON_free(content);
    }
}

/**
*@名称 		Entity_Mqtt_Event_Respone_Bind_Process
*@功能 		云端绑定的处理
*@参数 		cJSON *root
*@返回值 	void
*@使用说明	收到绑定的成功应答，配网才算真正成功，此时才置标志
*/
void Entity_Mqtt_Event_Respone_Bind_Process(const cJSON *root)
{
    unsigned char bind_state=false;
    cJSON *res_item = cJSON_GetObjectItem(root, "res");
    cJSON *msg_item = cJSON_GetObjectItem(root, "msg");
    if (!cJSON_IsNumber(res_item) || !cJSON_IsString(msg_item))
    {
        ENTITY_LOGE("%s invalid bind response res=%p msg=%p\r\n",
                    __func__, (void *)res_item, (void *)msg_item);
        if(Entity_Mqtt_Event_Respone_Parse_Cbs.Event_Bind_Parse_Cb)
            Entity_Mqtt_Event_Respone_Parse_Cbs.Event_Bind_Parse_Cb(false);
        return;
    }
    int res = res_item->valueint;
    char *msg = msg_item->valuestring;
    if((res == 0) && (strcmp(msg, "success") == 0))
    {
        ENTITY_LOGD("bind success\r\n");
        bind_state = true;
        Entity_App_Param_t *app_param = Get_Entity_App_Param();
        Entity_Dev_Config_Net_Info_t *dev_config_net_info = &app_param->Dev_Config_Net_Info;
        dev_config_net_info->Flag_Bind = true;//绑定状态 成功
        Entity_Save_Config_Net_Info_To_Flash();//保存到FLASH
        Entity_Config_Net_Timer_Stop();//停止配网超时定时器 
        Entity_Mqtt_Event_Get_Time_Request(1);
    }
    if(Entity_Mqtt_Event_Respone_Parse_Cbs.Event_Bind_Parse_Cb)
        Entity_Mqtt_Event_Respone_Parse_Cbs.Event_Bind_Parse_Cb(bind_state);
}

/**
*@名称 		Entity_Mqtt_Event_Respone_Agora_Agent_Nfc_Process
*@功能 		上报nfc的id后，云端下发的rtc token 和通道名称的处理
*@参数 		cJSON *root
*@返回值 	void
*@使用说明	
*/
void Entity_Mqtt_Event_Respone_Agora_Agent_Nfc_Process(cJSON *root)
{
    cJSON *res = cJSON_GetObjectItem(root,"res");
    if(!cJSON_IsNumber(res) || res->valueint!=0)//nfc上报后，云端失败了
    {
        ENTITY_LOGD("error code:%d\r\n", res ? res->valueint : -1);
        if(Entity_Mqtt_Event_Respone_Parse_Cbs.Event_Agora_Agent_Nfc_Parse_Cb)
            Entity_Mqtt_Event_Respone_Parse_Cbs.Event_Agora_Agent_Nfc_Parse_Cb(-1, NULL, NULL, NULL, 0);
        return ;
    }
    cJSON *data = cJSON_GetObjectItem(root, "data");
    if(!cJSON_IsObject(data)){
        ENTITY_LOGD("data is null\r\n");
        if(Entity_Mqtt_Event_Respone_Parse_Cbs.Event_Agora_Agent_Nfc_Parse_Cb)
            Entity_Mqtt_Event_Respone_Parse_Cbs.Event_Agora_Agent_Nfc_Parse_Cb(-1, NULL, NULL, NULL, 0);
        return;
    }
    cJSON *cjson_rtcToken = cJSON_GetObjectItem(data, "rtcToken");//rtc接入token
    cJSON *cjson_channelName = cJSON_GetObjectItem(data, "channelName");//rtc通道
    cJSON *cjson_appId = cJSON_GetObjectItem(data, "appId");//rtc通道
    cJSON *cjson_uid = cJSON_GetObjectItem(data, "uid");
    if(!cJSON_IsString(cjson_rtcToken) || !cJSON_IsString(cjson_channelName) ||
       !cJSON_IsString(cjson_appId) || !cJSON_IsNumber(cjson_uid)){
        ENTITY_LOGD("rtcToken or channelName or uid or appId is NULL\r\n");
        if(Entity_Mqtt_Event_Respone_Parse_Cbs.Event_Agora_Agent_Nfc_Parse_Cb)
            Entity_Mqtt_Event_Respone_Parse_Cbs.Event_Agora_Agent_Nfc_Parse_Cb(-1, NULL, NULL, NULL, 0);
        return;
    }
    char *rtcToken_string = cjson_rtcToken->valuestring;
    char *channelName_string = cjson_channelName->valuestring;
    char *appId_string = cjson_appId->valuestring;
    int uid = cjson_uid->valueint;
    char token_mask[32];
    Utils_Mask_Secret(rtcToken_string, token_mask, sizeof(token_mask));
    ENTITY_LOGD("%s rtcToken_mask:%s, channelName_string:%s,appId_string:%s,uid:%d \r\n", __func__, token_mask, channelName_string,appId_string,uid);
    if(Entity_Mqtt_Event_Respone_Parse_Cbs.Event_Agora_Agent_Nfc_Parse_Cb)
            Entity_Mqtt_Event_Respone_Parse_Cbs.Event_Agora_Agent_Nfc_Parse_Cb(0,rtcToken_string,channelName_string,appId_string,uid);
    
}


/**
*@名称 		Entity_Mqtt_Event_Respone_Agora_Agent_Device_Access_process
*@功能 		设备请求后，云端下发的rtc token 和通道名称的处理
*@参数 		cJSON *root
*@返回值 	void
*@使用说明	
*/
void Entity_Mqtt_Event_Respone_Agora_Agent_Device_Access_process(cJSON *root)
{
    cJSON *response_id = cJSON_GetObjectItem(root, "id");
    const char *response_id_str = cJSON_IsString(response_id) ? response_id->valuestring : NULL;
    char pending_id[32] = {0};
    (void)Entity_Mqtt_Get_Token_Pending_Snapshot(pending_id, sizeof(pending_id), NULL);
    if (!Entity_Mqtt_Is_Token_Response_Current(response_id_str))
    {
        AI_DIALOG_DIAG_LOGW("ENTITY_MQTT", "token_sub_stale",
                            "response_id=%s pending_id=%s",
                            response_id_str ? response_id_str : "(null)",
                            pending_id[0] ? pending_id : "(none)");
        ENTITY_LOGW("[ai_diag] agora_agent_device_access 过期响应丢弃: response_id=%s pending_id=%s\r\n",
                  response_id_str ? response_id_str : "(null)",
                  pending_id[0] ? pending_id : "(none)");
        return;
    }

    /* Bug1修復：res 可能不存在，需要 NULL guard 再讀 valueint */
    cJSON *res = cJSON_GetObjectItem(root, "res");
    if (!cJSON_IsNumber(res) || res->valueint != 0) {
        AI_DIALOG_DIAG_LOGE("ENTITY_MQTT", "token_sub_fail",
                            "reason=cloud_res res=%d",
                            res ? res->valueint : -1);
        ENTITY_LOGE("[ai_diag] agora_agent_device_access 云端失败: res=%d\r\n",
                  res ? res->valueint : -1);
        Entity_Mqtt_Clear_Token_Pending(response_id_str);
        if (Entity_Mqtt_Event_Respone_Parse_Cbs.Event_Agora_Agent_Device_Access_Parse_Cb)
            Entity_Mqtt_Event_Respone_Parse_Cbs.Event_Agora_Agent_Device_Access_Parse_Cb(-1, NULL, NULL, NULL, 0);
        return;
    }
    cJSON *data = cJSON_GetObjectItem(root, "data");
    /* Bug2修復：所有解析失敗路徑都必須回調 -1，否則 Controller 永遠卡在 pending 狀態 */
    if (!cJSON_IsObject(data)) {
        AI_DIALOG_DIAG_LOGE("ENTITY_MQTT", "token_sub_fail", "reason=data_null");
        ENTITY_LOGE("[ai_diag] agora_agent_device_access 解析失败: data is null\r\n");
        Entity_Mqtt_Clear_Token_Pending(response_id_str);
        if (Entity_Mqtt_Event_Respone_Parse_Cbs.Event_Agora_Agent_Device_Access_Parse_Cb)
            Entity_Mqtt_Event_Respone_Parse_Cbs.Event_Agora_Agent_Device_Access_Parse_Cb(-1, NULL, NULL, NULL, 0);
        return;
    }
    cJSON *cjson_rtcToken = cJSON_GetObjectItem(data, "rtcToken");
    cJSON *cjson_channelName = cJSON_GetObjectItem(data, "channelName");
    cJSON *cjson_appId = cJSON_GetObjectItem(data, "appId");
    cJSON *cjson_uid = cJSON_GetObjectItem(data, "uid");
    if (!cJSON_IsString(cjson_rtcToken) || !cJSON_IsString(cjson_channelName) ||
        !cJSON_IsString(cjson_appId) || !cJSON_IsNumber(cjson_uid)) {
        AI_DIALOG_DIAG_LOGE("ENTITY_MQTT", "token_sub_fail",
                            "reason=field_missing token=%p channel=%p appId=%p uid=%p",
                            (void *)cjson_rtcToken,
                            (void *)cjson_channelName,
                            (void *)cjson_appId,
                            (void *)cjson_uid);
        ENTITY_LOGE("[ai_diag] agora_agent_device_access 解析失败: token=%p channel=%p appId=%p uid=%p\r\n",
                  (void *)cjson_rtcToken, (void *)cjson_channelName,
                  (void *)cjson_appId, (void *)cjson_uid);
        Entity_Mqtt_Clear_Token_Pending(response_id_str);
        if (Entity_Mqtt_Event_Respone_Parse_Cbs.Event_Agora_Agent_Device_Access_Parse_Cb)
            Entity_Mqtt_Event_Respone_Parse_Cbs.Event_Agora_Agent_Device_Access_Parse_Cb(-1, NULL, NULL, NULL, 0);
        return;
    }
    char *rtcToken_string = cjson_rtcToken->valuestring;
    char *channelName_string = cjson_channelName->valuestring;
    char *appId_string = cjson_appId->valuestring;
    int uid = cjson_uid->valueint;
    uint64_t now_ms  = Entity_Get_Uptime_Ms();
    uint64_t pub_ms  = Entity_Mqtt_Get_Token_Pub_Ms();
    uint64_t rtt_ms  = (pub_ms > 0 && now_ms >= pub_ms) ? (now_ms - pub_ms) : 0;
    AI_DIALOG_DIAG_LOGI("ENTITY_MQTT", "token_sub_parse_ok",
                        "uid=%d token_len=%u channel=%s pub_mono=%" PRIu64 "ms sub_mono=%" PRIu64 "ms rtt=%" PRIu64 "ms",
                        uid,
                        (unsigned int)(rtcToken_string ? strlen(rtcToken_string) : 0),
                        channelName_string ? channelName_string : "(null)",
                        pub_ms,
                        now_ms,
                        rtt_ms);
    ENTITY_LOGI("[ai_diag] agora_agent_device_access 解析成功: uid=%d token_len=%u channel=%s "
              "pub_mono=%" PRIu64 "ms sub_mono=%" PRIu64 "ms rtt=%" PRIu64 "ms\r\n",
              uid, (unsigned int)(rtcToken_string ? strlen(rtcToken_string) : 0),
              channelName_string ? channelName_string : "(null)",
              pub_ms, now_ms, rtt_ms);
    Entity_Mqtt_Clear_Token_Pending(response_id_str);
    if (Entity_Mqtt_Event_Respone_Parse_Cbs.Event_Agora_Agent_Device_Access_Parse_Cb)
        Entity_Mqtt_Event_Respone_Parse_Cbs.Event_Agora_Agent_Device_Access_Parse_Cb(0, rtcToken_string, channelName_string, appId_string, uid);
}

/**
*@名称 		Entity_Mqtt_Msg_Event_Respone_Parse_Process
*@功能 		mqtt事件应答消息处理
*@参数 		char *msg
*@返回值 	void
*@使用说明	设备上报事件后，云端回复的数据处理
*/
void Entity_Mqtt_Msg_Event_Respone_Parse_Process(char *msg)
{
    if (msg == NULL)
    {
        ENTITY_LOGE("%s msg is NULL", __func__);
        return;
    }
    cJSON *root = cJSON_Parse(msg);
    if (!root)
    {
        ENTITY_LOGE("%s cJSON_CreateObject error", __func__);
        return;
    }
    cJSON_bool object = cJSON_IsObject(root);
    if (!object)
    {
        ENTITY_LOGE("msg Not a json message ");
        goto cleanup;
    }
    AI_HOTPATH_VERBOSE_DO(
        Entity_Log_Long_String(ENTITY_LOG_LEVEL_DEBUG, "cloud--->device respone mqtt event", msg, strlen(msg));
    );
    //ENTITY_LOGD("cloud--->device respone mqtt event msg len=%d, %s", strlen(msg), msg);

    //事件通用回调
    Entity_Mqtt_App_Msg_Reach_Callback_t Entity_Mqtt_App_Msg_Reach_Cbs = Get_Entity_Mqtt_App_Msg_Reach_Cbs();
    if(Entity_Mqtt_App_Msg_Reach_Cbs.Entity_Mqtt_Msg_Event_Cb)
        Entity_Mqtt_App_Msg_Reach_Cbs.Entity_Mqtt_Msg_Event_Cb();

    cJSON *code_item = cJSON_GetObjectItem(root, "code");
    if (!cJSON_IsString(code_item))
    {
        ENTITY_LOGE("%s code field missing or invalid", __func__);
        goto cleanup;
    }
    char *code = code_item->valuestring;
    unsigned char cmd_code = Array_Index_Of(code, Mqtt_Event_Code_List, ENTITY_EVENT_CODE_INDEX_NUM);
    ENTITY_LOGD("cmd_code:%d\r\n",cmd_code);
    
    switch (cmd_code)
    {
        case ENTITY_EVENT_RESET_CODE_INDEX: //重置设备的处理
            break;
        case ENTITY_EVENT_TIME_CODE_INDEX: //获取云端时间的处理
            Entity_Mqtt_Event_Respone_Time_Parse(root);
            break;
        case ENTITY_EVENT_MODEL_CODE_INDEX: //获取物模型
            Entity_Mqtt_Event_Respone_Model_Process(root);
            break;
        case ENTITY_EVENT_CONFIG_CODE_INDEX: //获取远程配置
            break;
        case ENTITY_EVENT_BIND_CODE_INDEX: //设备绑定
            Entity_Mqtt_Event_Respone_Bind_Process(root);
            break;
        case ENTITY_EVENT_INFO_CODE_INDEX://设备信息
            ENTITY_LOGI("[MQTT_DIAG][INFO_RESP_TIME_QOS0] ack=1 reason=avoid_startup_qos1_outbox\r\n");
            Entity_Mqtt_Event_Get_Time_Request_Qos(1, QOS0_MOST_ONCE);//在收到上报设备信息的应答后请求时间
            break;
        case ENTITY_EVENT_PROPERTY_REPORT_CODE_INDEX: //属性上报
            break;
        case ENTITY_EVENT_OTA_PROGRESS_CODE_INDEX: //OTA进度
            break;
        case ENTITY_EVENT_SUB_BIND_CODE_INDEX: //子设备绑定
            break;
        case ENTITY_EVENT_SUB_DELETE_CODE_INDEX: //子设备解绑
            break;
        case ENTITY_EVENT_SUB_LOGIN_CODE_INDEX://子设备上线
            break;
        case ENTITY_EVENT_SUB_LOGINOUT_CODE_INDEX: //子设备下线
            break;
        case ENTITY_EVENT_SUB_MODEL_CODE_INDEX://获取子设备物模型
            break;
        case ENTITY_EVENT_SUB_PROPERTY_REPORT_CODE_INDEX: //子设备属性上报
            break;
        case ENTITY_EVENT_LOCAL_CONFIG_CODE_INDEX: //通知云端下发网关本地群组/场景/一键执行
            break;
        case ENTITY_EVENT_SUB_LIST_CODE_INDEX: //获取子设备列表
            break;
        case ENTITY_EVENT_IPC_CLOUD_CODE_INDEX: //获取IPC云存储信息
            break;
        case ENTITY_EVENT_P2P_CONFIG_CODE_INDEX: //获取P2P配置信息
            break;
        case ENTITY_EVENT_AGORA_TOKEN_CODE_INDEX: //获取声网Token
            break;
        case ENTITY_EVENT_DIY_EVENT_CODE_INDEX: //自定义事件上报
            break;
        case ENTITY_EVENT_IPC_TOKEN_BIND_CODE_INDEX: //IPC token绑定设备
            break;
        case ENTITY_EVENT_FIND_ALERT_CODE_INDEX: //查找弹出设备
            break;
        case ENTITY_EVENT_FIND_REPORT_CODE_INDEX: //网关上报搜索到的设备
            break;
        case ENTITY_EVENT_IPC_LIVE_GET_CODE_INDEX: //IPC设备获取实时流推送地址
            break;
        case ENTITY_EVENT_AGORA_AGENT_NFC_REPORT_CODE_INDEX: //上报nfc的id后，rtc的token和通道名称
            Entity_Mqtt_Event_Respone_Agora_Agent_Nfc_Process(root);
            break;
        case ENTITY_EVENT_AGORA_RTC_STOP_REPORT_CODE_INDEX: //停止rtc对话
            break;
        case ENTITY_EVENT_AGORA_AGENT_DEVICE_ACCESS_CODE_INDEX: //设备请求，rtc的token和通道名称
            Entity_Mqtt_Event_Respone_Agora_Agent_Device_Access_process(root);
            break;
        default:
            break;
    }
cleanup:
    cJSON_Delete(root);
}
