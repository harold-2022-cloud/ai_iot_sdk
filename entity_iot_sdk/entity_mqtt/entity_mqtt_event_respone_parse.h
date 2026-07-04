//entity_mqtt_event_respone_parse.h
#pragma once

#include "cJSON.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>


typedef void (*Mqtt_Event_Parse_Cb)(cJSON* root);

//事件应答的解析处理回调
typedef struct 
{
    void (*Event_Time_Parse_Cb)(unsigned int stamp, int zone_offset, char *sys_tz_str);
    void (*Event_Model_Parse_Cb)(cJSON*);
    void (*Event_Bind_Parse_Cb)(unsigned char bind_state);
    Mqtt_Event_Parse_Cb Event_Config_Parse_Cb;
    Mqtt_Event_Parse_Cb Event_Info_Parse_Cb;
    Mqtt_Event_Parse_Cb Event_Property_Report_Parse_Cb;
    Mqtt_Event_Parse_Cb Event_Ota_progress_Parse_Cb;
    Mqtt_Event_Parse_Cb Event_Sub_Bind_Parse_Cb;
    Mqtt_Event_Parse_Cb Event_Sub_Delete_Parse_Cb;
    Mqtt_Event_Parse_Cb Event_Sub_Login_Parse_Cb;
    Mqtt_Event_Parse_Cb Event_Sub_Loginout_Parse_Cb;
    Mqtt_Event_Parse_Cb Event_Sub_Model_Parse_Cb;
    Mqtt_Event_Parse_Cb Event_Sub_Property_Report_Parse_Cb;
    Mqtt_Event_Parse_Cb Event_Local_Config_Parse_Cb;
    Mqtt_Event_Parse_Cb Event_Sub_List_Parse_Cb;
    Mqtt_Event_Parse_Cb Event_Ipc_Cloud_Parse_Cb;
    Mqtt_Event_Parse_Cb Event_P2p_Config_Parse_Cb;
    Mqtt_Event_Parse_Cb Event_Agora_Token_Parse_Cb;
    Mqtt_Event_Parse_Cb Event_Event_Parse_Cb;
    Mqtt_Event_Parse_Cb Event_Ipc_Token_Bind_Parse_Cb;
    Mqtt_Event_Parse_Cb Event_Find_Alert_Parse_Cb;
    Mqtt_Event_Parse_Cb Event_Find_Report_Parse_Cb;
    Mqtt_Event_Parse_Cb Event_Ipc_Live_Get_Parse_Cb;
    void (*Event_Agora_Agent_Nfc_Parse_Cb)(int result, const char *rtcToken, const char *channelName, const char *appId, int uid);
    void (*Event_Agora_Agent_Device_Access_Parse_Cb)(int result, const char *rtcToken, const char *channelName, const char *appId, int uid);
}Entity_Mqtt_Event_Respone_Parse_Cbs_t;


//mqtt事件消息云端应答数据处理回调注册
void Entity_Mqtt_Event_Respone_Parse_Cbs_Init(Entity_Mqtt_Event_Respone_Parse_Cbs_t *cbs);

//mqtt事件应答消息处理
void Entity_Mqtt_Msg_Event_Respone_Parse_Process(char *msg);









