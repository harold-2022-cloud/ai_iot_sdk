//entity_mqtt_cmd_respone.c
#include "entity_mqtt_cmd_respone.h"

#include "entity_mqtt_v2_define.h"
#include "entity_mqtt_app.h"

#include "entity_log.h"
#include "entity_wifi.h"
#include "entity_iot_func.h"
#include "entity_dev_info.h"

#include "com_utils.h"
#include "cJSON.h"

#include <sys/time.h>
#include <string.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>


/**
*@名称 		Entity_Mqtt_Cmd_Publish
*@功能 		命令发布
*@参数 		cJSON *root, 
*@参数 		Mqtt_Qos_Type_e qos, 
*@参数 		int retain
*@返回值 	void
*@使用说明	将cjson转成无模式字符串后发布
*/
static void Entity_Mqtt_Cmd_Publish(cJSON *root, Mqtt_Qos_Type_e qos, int retain)
{
    char *json = cJSON_PrintUnformatted(root);
    if (json)
    {
        ENTITY_LOGD("json:%s", json);
        Entity_Mqtt_App_Topic_Publish(TOPIC_TYPE_CMD_PUBLISH, json, strlen(json), qos, retain);
        cJSON_free(json);
    }
}

/**
*@名称 		Entity_Mqtt_Cmd_Respone_Load_Base
*@功能 		加载命令应答基础内容
*@参数 		cJSON *root, 
*@参数 		const char *msgId
*@参数 		int res
*@参数 		const char *code
*@返回值 	void
*@使用说明	将cjson转成无模式字符串后发布
*/
void Entity_Mqtt_Cmd_Respone_Load_Base(cJSON *root,const char *msgId, int res, const char *code)
{
    unsigned int timestamp = Entity_Get_Time_Stamp();
    cJSON_AddNumberToObject(root, "res", res);
    cJSON_AddStringToObject(root, "id", msgId);
    cJSON_AddNumberToObject(root, "ts", timestamp);
    cJSON_AddStringToObject(root, "code", code);
}


/**
*@名称 		Entity_Mqtt_Cmd_Reset_Respone
*@功能 		指令应答-设备重置
*@参数 		const char *msgId 
*@参数 		int res 状态码 0代表成功,其他代表失败
*@返回值 	void
*@使用说明	将cjson转成无模式字符串后发布
*/
void Entity_Mqtt_Cmd_Reset_Respone(const char *msgId, int res)
{
    cJSON *root = cJSON_CreateObject();
    Custom_Assert_Check_RequireString_Return_Null(root, "cJSON_CreateObject error");
    Entity_Mqtt_Cmd_Respone_Load_Base(root, msgId, res, ENTITY_COMMAND_RESET_CODE);
    Entity_Mqtt_Cmd_Publish(root, QOS1_LEAST_ONCE, 0);
    cJSON_Delete(root);
}

/**
*@名称 		Entity_Mqtt_Cmd_Ota_Respone
*@功能 		指令应答-固件升级
*@参数 		const char *msgId 
*@参数 		int res 状态码 0代表成功,其他代表失败
*@返回值 	void
*@使用说明	将cjson转成无模式字符串后发布
*/
void Entity_Mqtt_Cmd_Ota_Respone(const char *msgId, int res)
{
    cJSON *root = cJSON_CreateObject();
    Custom_Assert_Check_RequireString_Return_Null(root, "cJSON_CreateObject error");
    Entity_Mqtt_Cmd_Respone_Load_Base(root, msgId, res, ENTITY_COMMAND_OTA_CODE);
    Entity_Mqtt_Cmd_Publish(root, QOS1_LEAST_ONCE, 0);
    cJSON_Delete(root);
}

/**
*@名称 		Entity_Mqtt_Cmd_Ping_Respone
*@功能 		指令应答-检测设备在线状态
*@参数 		cJSON *data
*@参数 		const char *msgId 
*@参数 		int res 状态码 0代表成功,其他代表失败
*@返回值 	void
*@使用说明	将cjson转成无模式字符串后发布
*/
void Entity_Mqtt_Cmd_Ping_Respone(cJSON *data,const char *msgId, int res)
{
    cJSON *root = cJSON_CreateObject();
    Custom_Assert_Check_RequireString_Return_Null(root, "cJSON_CreateObject error");
    Entity_Mqtt_Cmd_Respone_Load_Base(root, msgId, res, ENTITY_COMMAND_PING_CODE);
    //20240123增加WIFI信号强度回复
    cJSON *object1 = cJSON_Duplicate(data, 1);
    cJSON_AddItemToObject(root, "data", object1);
    Entity_Mqtt_Cmd_Publish(root, QOS1_LEAST_ONCE, 0);
    cJSON_Delete(root);
}


/**
*@名称 		Entity_Mqtt_Cmd_Property_Set_Respone
*@功能 		指令应答-设置属性
*@参数 		cJSON *property key-value形式json字符串
*@参数 		const char *msgId 
*@参数 		int res 状态码 0代表成功,其他代表失败
*@返回值 	void
*@使用说明	将cjson转成无模式字符串后发布
*/
void Entity_Mqtt_Cmd_Property_Set_Respone(cJSON *property, const char *msgId, int res)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    Custom_Assert_Check_RequireString_Goto((root && data), quit, "cJSON_CreateObject error");
    Entity_Mqtt_Cmd_Respone_Load_Base(root, msgId, res, ENTITY_COMMAND_PROPERTY_SET_CODE);
    cJSON *object1 = cJSON_Duplicate(property, 1);
    cJSON_AddItemToObject(data, "properties", object1);
    cJSON_AddItemToObject(root, "data", data);
    Entity_Mqtt_Cmd_Publish(root, QOS1_LEAST_ONCE, 0);
quit:
    cJSON_Delete(root);
}

/**
*@名称 		Entity_Mqtt_Cmd_Update_Local_Config_Respone
*@功能 		指令应答-本地群组/场景/一键执行配置下发
*@参数 		const char *msgId 
*@参数 		int res 状态码 0代表成功,其他代表失败
*@返回值 	void
*@使用说明	将cjson转成无模式字符串后发布
*/
void Entity_Mqtt_Cmd_Update_Local_Config_Respone(const char *msgId, int res)
{
    cJSON *root = cJSON_CreateObject();
    Custom_Assert_Check_RequireString_Return_Null(root, "cJSON_CreateObject error");
    Entity_Mqtt_Cmd_Respone_Load_Base(root, msgId, res, ENTITY_COMMAND_UPDATE_LOCAL_CONFIG_CODE);
    Entity_Mqtt_Cmd_Publish(root, QOS1_LEAST_ONCE, 0);
    cJSON_Delete(root);
}


/**
*@名称 		Entity_Mqtt_Cmd_Find_Bind_Respone
*@功能 		指令应答-搜索并绑定子设备
*@参数 		const char *msgId 
*@参数 		int res 状态码 0代表成功,其他代表失败
*@返回值 	void
*@使用说明	将cjson转成无模式字符串后发布
*/
void Entity_Mqtt_Cmd_Find_Bind_Respone(const char *msgId, int res)
{
    cJSON *root = cJSON_CreateObject();
    Custom_Assert_Check_RequireString_Return_Null(root, "cJSON_CreateObject error");
    Entity_Mqtt_Cmd_Respone_Load_Base(root, msgId, res, ENTITY_COMMAND_FIND_BIND_CODE);
    Entity_Mqtt_Cmd_Publish(root, QOS1_LEAST_ONCE, 0);
    cJSON_Delete(root);
}


/**
*@名称 		Entity_Mqtt_Cmd_Sub_Property_Set_Respone
*@功能 		指令应答-批量设置子设备属性
*@参数 		cJSON *data
*@参数 		const char *msgId 
*@参数 		int res 状态码 0代表成功,其他代表失败
*@返回值 	void
*@使用说明	将cjson转成无模式字符串后发布
*/
void Entity_Mqtt_Cmd_Sub_Property_Set_Respone(cJSON *data, const char *msgId, int res)
{
    cJSON *root = cJSON_CreateObject();
    Custom_Assert_Check_RequireString_Return_Null(root, "cJSON_CreateObject error");
    Entity_Mqtt_Cmd_Respone_Load_Base(root, msgId, res, ENTITY_COMMAND_SUB_PROPERTY_SET_CODE);
    cJSON *object1 = cJSON_Duplicate(data, 1);
    cJSON_AddItemToObject(root, "data", object1);
    Entity_Mqtt_Cmd_Publish(root, QOS1_LEAST_ONCE, 0);
    cJSON_Delete(root);
}

/**
*@名称 		Entity_Mqtt_Cmd_Sub_Delete_Respone
*@功能 		指令应答-删除子设备
*@参数 		const char *msgId 
*@参数 		int res 状态码 0代表成功,其他代表失败
*@返回值 	void
*@使用说明	将cjson转成无模式字符串后发布
*/
void Entity_Mqtt_Cmd_Sub_Delete_Respone(const char *msgId, int res)
{
    cJSON *root = cJSON_CreateObject();
    Custom_Assert_Check_RequireString_Return_Null(root, "cJSON_CreateObject error");
    Entity_Mqtt_Cmd_Respone_Load_Base(root, msgId, res, ENTITY_COMMAND_SUB_DELETE_CODE);
    Entity_Mqtt_Cmd_Publish(root, QOS1_LEAST_ONCE, 0);
    cJSON_Delete(root);
}

/**
*@名称 		Entity_Mqtt_Cmd_Local_Group_Set_Respone
*@功能 		指令应答-本地群组控制
*@参数 		unsigned short shortId 群组短ID
*@参数 		cJSON *property key-value形式json字符串
*@参数 		const char *msgId 
*@参数 		int res 状态码 0代表成功,其他代表失败
*@返回值 	void
*@使用说明	将cjson转成无模式字符串后发布
*/
void Entity_Mqtt_Cmd_Local_Group_Set_Respone(unsigned short shortId, cJSON *property, const char *msgId, int res)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    Custom_Assert_Check_RequireString_Goto((root && data), quit, "cJSON_CreateObject error");
    Entity_Mqtt_Cmd_Respone_Load_Base(root, msgId, res, ENTITY_COMMAND_LOCAL_GROUP_SET_CODE);
    cJSON *object1 = cJSON_Duplicate(property, 1);
    cJSON_AddItemToObject(data, "properties", object1);
    cJSON_AddNumberToObject(data, "shortId", shortId);
    cJSON_AddItemToObject(root, "data", data);
    Entity_Mqtt_Cmd_Publish(root, QOS1_LEAST_ONCE, 0);
 quit:   
    cJSON_Delete(root);
}

/**
*@名称 		Entity_Mqtt_Cmd_Local_Rule_Exec_Respone
*@功能 		指令应答-本地一键执行
*@参数 		unsigned short shortId 群组短ID
*@参数 		const char *msgId 
*@参数 		int res 状态码 0代表成功,其他代表失败
*@返回值 	void
*@使用说明	
*/
void Entity_Mqtt_Cmd_Local_Rule_Exec_Respone(int shortId, const char *msgId, int res)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    Custom_Assert_Check_RequireString_Goto((root && data), quit, "cJSON_CreateObject error");
    Entity_Mqtt_Cmd_Respone_Load_Base(root, msgId, res, ENTITY_COMMAND_LOCAL_RULE_EXEC_CODE);
    cJSON_AddNumberToObject(data, "shortId", shortId);
    cJSON_AddItemToObject(root, "data", data);
    Entity_Mqtt_Cmd_Publish(root, QOS1_LEAST_ONCE, 0);
quit:
    cJSON_Delete(root);
}

/**
*@名称 		Entity_Mqtt_Cmd_Wake_Up_Respone
*@功能 		指令应答-唤醒设备
*@参数 		const char *subPid 子设备pid 非必选 若是网关子设备必填 传子设备的pid
*@参数 		const char *subUuid 子设备uuid 非必选 若是网关子设备必填 传子设备的uuid
*@参数 		const char *msgId 
*@参数 		int res 状态码 0代表成功,其他代表失败
*@返回值 	void
*@使用说明	
*/
void Entity_Mqtt_Cmd_Wake_Up_Respone(const char *subPid, const char *subUuid, const char *msgId, int res)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    Custom_Assert_Check_RequireString_Goto((root && data), quit, "cJSON_CreateObject error");
    Entity_Mqtt_Cmd_Respone_Load_Base(root, msgId, res, ENTITY_COMMAND_WAKE_UP_CODE);
    cJSON_AddStringToObject(data, "subPid", subPid);
    cJSON_AddStringToObject(data, "subUuid", subUuid);
    cJSON_AddItemToObject(root, "data", data);
    Entity_Mqtt_Cmd_Publish(root, QOS1_LEAST_ONCE, 0);
quit:
    cJSON_Delete(root);
}

/**
*@名称 		Entity_Mqtt_Cmd_Agora_Join_Respone
*@功能 		指令应答-通知设备加入声网通道
*@参数 		const char *msgId 
*@参数 		int res 状态码 0代表成功,其他代表失败
*@返回值 	void
*@使用说明	具体应答未知，暂时补上
*/
void Entity_Mqtt_Cmd_Agora_Join_Respone(const char *msgId, int res)
{
    cJSON *root = cJSON_CreateObject();
    Custom_Assert_Check_RequireString_Return_Null(root, "cJSON_CreateObject error");
    Entity_Mqtt_Cmd_Respone_Load_Base(root, msgId, res, ENTITY_COMMAND_AGORA_JOIN_CODE);
    Entity_Mqtt_Cmd_Publish(root, QOS1_LEAST_ONCE, 0);
    cJSON_Delete(root);
}

/**
*@名称 		Entity_Mqtt_Cmd_Ipc_Cloud_Open_Respone
*@功能 		指令应答-IPC云存储开通
*@参数 		int result true-设备成功处理 false-设备处理失败
*@参数 		const char *msgId 
*@参数 		int res 状态码 0代表成功,其他代表失败
*@返回值 	void
*@使用说明	
*/
void Entity_Mqtt_Cmd_Ipc_Cloud_Open_Respone(int result, const char *msgId, int res)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();

    Custom_Assert_Check_RequireString_Goto((root && data), quit, "cJSON_CreateObject error");
    Entity_Mqtt_Cmd_Respone_Load_Base(root, msgId, res, ENTITY_COMMAND_IPC_CLOUD_OPEN_CODE);

    if(result) {
        cJSON_AddTrueToObject(data, "result");
    } 
    else {
        cJSON_AddFalseToObject(data, "result");
    }
    cJSON_AddItemToObject(root, "data", data);

    Entity_Mqtt_Cmd_Publish(root, QOS1_LEAST_ONCE, 0);
quit:
    cJSON_Delete(root);
}

/**
*@名称 		Entity_Mqtt_Cmd_Agora_Event_Respone
*@功能 		指令应答-声网事件通知
*@参数 		int eventType 事件类型, 103-主播加入频道, 104-主播离开频道
*@参数 		const char *channelName 通道名称
*@参数 		int channelUserNum 通道用户数, 不包含设备
*@参数 		const char *uid,  用户声网uid
*@参数 		const char *userId 用户id
*@参数 		int result true-设备成功处理 false-设备处理失败
*@参数 		const char *msgId 
*@参数 		int res 状态码 0代表成功,其他代表失败
*@返回值 	void
*@使用说明	
*/
void Entity_Mqtt_Cmd_Agora_Event_Respone(int eventType, const char *channelName, int channelUserNum, const char *uid, const char *userId, const char *msgId, int res)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    Custom_Assert_Check_RequireString_Goto((root && data), quit, "cJSON_CreateObject error");
    Entity_Mqtt_Cmd_Respone_Load_Base(root, msgId, res, ENTITY_COMMAND_AGORA_EVENT_CODE);
    cJSON_AddNumberToObject(data, "eventType", eventType);
    cJSON_AddStringToObject(data, "channelName", channelName);
    cJSON_AddNumberToObject(data, "channelUserNum", channelUserNum);
    cJSON_AddStringToObject(data, "uid", uid);
    cJSON_AddStringToObject(data, "userId", userId);
    cJSON_AddItemToObject(root, "data", data);
    Entity_Mqtt_Cmd_Publish(root, QOS1_LEAST_ONCE, 0);
quit:
    cJSON_Delete(root);
}


/**
*@名称 		Entity_Mqtt_Cmd_Common_Cmd_Respone
*@功能 		指令应答-通用透传指令
*@参数 		const char *respType 指令类型
*@参数 		int uid 加一个uid, APP这边用于过滤, 支持设备和用户点对点的交互
*@参数 		int format 格式
*@参数 		int channel 通道
*@参数 		int rate 帧率
*@参数 		int volume 音量
*@参数 		int samples 比特率
*@参数 		const char *msgId 
*@参数 		int res 状态码 0代表成功,其他代表失败
*@返回值 	void
*@使用说明	
*/
void Entity_Mqtt_Cmd_Common_Cmd_Respone(const char *respType, int uid, int format, int channel, int rate, int volume, int samples, const char *msgId, int res)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    cJSON *respData = cJSON_CreateObject();
    Custom_Assert_Check_RequireString_Goto((root && data && respData), quit, "cJSON_CreateObject error");
    Entity_Mqtt_Cmd_Respone_Load_Base(root, msgId, res, ENTITY_COMMAND_COMMON_CMD_CODE);
    cJSON_AddNumberToObject(respData, "uid", uid);
    cJSON_AddNumberToObject(respData, "format", format);
    cJSON_AddNumberToObject(respData, "channel", channel);
    cJSON_AddNumberToObject(respData, "rate", rate);
    cJSON_AddNumberToObject(respData, "volume", volume);
    cJSON_AddNumberToObject(respData, "Samples", samples);
    cJSON_AddItemToObject(data, "respData", respData);
    cJSON_AddStringToObject(data, "respType", respType);
    cJSON_AddItemToObject(root, "data", data);
    Entity_Mqtt_Cmd_Publish(root, QOS1_LEAST_ONCE, 0);
quit:
    cJSON_Delete(root);
}

/**
*@名称 		Entity_Mqtt_Cmd_Local_Group_Update_Respone
*@功能 		指令应答-1.4.17 本地群组变化通知
*@参数 		cJSON *data
*@参数 		const char *msgId 
*@参数 		int res 状态码 0代表成功,其他代表失败
*@返回值 	void
*@使用说明	
*/
void Entity_Mqtt_Cmd_Local_Group_Update_Respone(cJSON *data, const char *msgId, int res)
{
    cJSON *root = cJSON_CreateObject();
    Custom_Assert_Check_RequireString_Return_Null(root, "cJSON_CreateObject error");
    Entity_Mqtt_Cmd_Respone_Load_Base(root, msgId, res, ENTITY_COMMAND_LOCAL_GROUP_UPDATE_CODE);
    cJSON *object1 = cJSON_Duplicate(data, 1);
    cJSON_AddItemToObject(root, "data", object1);
    Entity_Mqtt_Cmd_Publish(root, QOS1_LEAST_ONCE, 0);
    cJSON_Delete(root);
}

/**
*@名称 		Entity_Mqtt_Cmd_Local_Scene_Update_Respone
*@功能 		指令应答-本地场景/一键执行变化通知
*@参数 		cJSON *data
*@参数 		const char *msgId 
*@参数 		int res 状态码 0代表成功,其他代表失败
*@返回值 	void
*@使用说明	
*/
void Entity_Mqtt_Cmd_Local_Scene_Update_Respone(cJSON *data, const char *msgId, int res)
{
    cJSON *root = cJSON_CreateObject();
    Custom_Assert_Check_RequireString_Return_Null(root, "cJSON_CreateObject error");
    Entity_Mqtt_Cmd_Respone_Load_Base(root, msgId, res, ENTITY_COMMAND_LOCAL_SCENE_UPDATE_CODE);
    cJSON *object1 = cJSON_Duplicate(data, 1);
    cJSON_AddItemToObject(root, "data", object1);
    Entity_Mqtt_Cmd_Publish(root, QOS1_LEAST_ONCE, 0);
    cJSON_Delete(root);
}

/**
*@名称 		Entity_Mqtt_Cmd_Log_Switch_Respone
*@功能 		指令应答-设备日志开关
*@参数 		const char *msgId 
*@参数 		int res 状态码 0代表成功,其他代表失败
*@返回值 	void
*@使用说明	
*/
void Entity_Mqtt_Cmd_Log_Switch_Respone(const char *msgId, int res)
{
    cJSON *root = cJSON_CreateObject();
    Custom_Assert_Check_RequireString_Return_Null(root, "cJSON_CreateObject error");
    Entity_Mqtt_Cmd_Respone_Load_Base(root, msgId, res, ENTITY_COMMAND_LOG_SWITCH_CODE);
    Entity_Mqtt_Cmd_Publish(root, QOS1_LEAST_ONCE, 0);
    cJSON_Delete(root);
}

/**
*@名称 		Entity_Mqtt_Cmd_Reboot_Respone
*@功能 		指令应答-设备重启
*@参数 		const char *msgId 
*@参数 		int res 状态码 0代表成功,其他代表失败
*@返回值 	void
*@使用说明	
*/
void Entity_Mqtt_Cmd_Reboot_Respone(const char *msgId, int res)
{
    cJSON *root = cJSON_CreateObject();
    Custom_Assert_Check_RequireString_Return_Null(root, "cJSON_CreateObject error");
    Entity_Mqtt_Cmd_Respone_Load_Base(root, msgId, res, ENTITY_COMMAND_REBOOT_CODE);
    Entity_Mqtt_Cmd_Publish(root, QOS1_LEAST_ONCE, 0);
    cJSON_Delete(root);
}

/**
*@名称 		Entity_Mqtt_Cmd_Sub_Replace_Respone
*@功能 		指令应答-子设备替换
*@参数 		const char *pid 新 pid
*@参数 		const char *uuid 旧uuid, 被替换的uuid
*@参数 		const char *newUuid 新uuid
*@参数 		const char *msgId 
*@参数 		int res 状态码 0代表成功,其他代表失败
*@参数 		const char *result
*@返回值 	void
*@使用说明	
*/
void Entity_Mqtt_Cmd_Sub_Replace_Respone(const char *pid, const char *uuid, const char *newUuid, const char *msgId, int res, const char *result)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    Custom_Assert_Check_RequireString_Goto((root && data), quit, "cJSON_CreateObject error");
    Entity_Mqtt_Cmd_Respone_Load_Base(root, msgId, res, ENTITY_COMMAND_SUB_REPLACE_CODE);
    cJSON_AddStringToObject(data, "pid", pid);
    cJSON_AddStringToObject(data, "uuid", uuid);
    cJSON_AddStringToObject(data, "newUuid", newUuid);
    cJSON_AddStringToObject(data, "result", result);
    cJSON_AddItemToObject(root, "data", data);
    Entity_Mqtt_Cmd_Publish(root, QOS1_LEAST_ONCE, 0);
quit:
    cJSON_Delete(root);
}

/**
*@名称 		Entity_Mqtt_Cmd_Config_Settings_Respone
*@功能 		指令应答-设置设备配置
*@参数 		const char*  type 操作类型  wifi_set wifi_list white_list
*@参数 		cJSON *settings 设置内容
*@参数 		const char *msgId 
*@参数 		int res 状态码 0代表成功,其他代表失败
*@参数 		const char *result
*@返回值 	void
*@使用说明	
*/
void Entity_Mqtt_Cmd_Config_Settings_Respone(const char* type, cJSON *settings, const char *msgId, int res, const char *result)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    Custom_Assert_Check_RequireString_Goto((root && data), quit, "cJSON_CreateObject error");
    Entity_Mqtt_Cmd_Respone_Load_Base(root, msgId, res, ENTITY_COMMAND_CONFIG_SETTINGS);
    cJSON_AddStringToObject(data, "type", type);
    cJSON *object1 = cJSON_Duplicate(settings, 1);
    cJSON_AddItemToObject(data, "settings", object1);
    cJSON_AddStringToObject(data, "result", result);
    cJSON_AddItemToObject(root, "data", data);
    Entity_Mqtt_Cmd_Publish(root, QOS1_LEAST_ONCE, 0);
 quit:   
    cJSON_Delete(root);
}

/**
*@名称 		Entity_Mqtt_Cmd_Dp_Bind_Respone
*@功能 		指令应答-DP点绑定本地执行
*@参数 		cJSON *dpBinds
*@参数 		const char *subPid
*@参数 		const char *subUuid
*@参数 		const char *msgId 
*@参数 		int res
*@返回值 	void
*@使用说明	
*/
void Entity_Mqtt_Cmd_Dp_Bind_Respone(cJSON *dpBinds, const char *subPid, const char *subUuid, const char *msgId, int res)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    Custom_Assert_Check_RequireString_Goto((root && data), quit, "cJSON_CreateObject error");
    Entity_Mqtt_Cmd_Respone_Load_Base(root, msgId, res, ENTITY_COMMAND_DP_BIND_CODE);
    if(subPid)
        cJSON_AddStringToObject(data, "subPid", subPid);
    if(subUuid)
        cJSON_AddStringToObject(data, "subUuid", subUuid);
    cJSON *object1 = cJSON_Duplicate(dpBinds, 1);
    cJSON_AddItemToObject(data, "dpBinds", object1);
    cJSON_AddItemToObject(root, "data", data);
    Entity_Mqtt_Cmd_Publish(root, QOS1_LEAST_ONCE, 0);
 quit:   
    cJSON_Delete(root);
}

/**
*@名称 		Entity_Mqtt_Cmd_Local_Dp_Group_Update_Respone
*@功能 		指令应答-本地DP群组变化通知 应答
*@参数 		uint16_t shortId
*@参数 		uint8_t operatorType
*@参数 		const char *msgId 
*@参数 		int res 状态码 0代表成功,其他代表失败
*@参数 		const char *result
*@返回值 	void
*@使用说明	
*/
void Entity_Mqtt_Cmd_Local_Dp_Group_Update_Respone(uint16_t shortId, uint8_t operatorType, const char *msgId, int res, const char *result)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    Custom_Assert_Check_RequireString_Goto((root && data), quit, "cJSON_CreateObject error");
    Entity_Mqtt_Cmd_Respone_Load_Base(root, msgId, res, ENTITY_COMMAND_LOCAL_DP_GROUP_UPDATE);
    cJSON_AddNumberToObject(data, "shortId", shortId);
    cJSON_AddNumberToObject(data, "operatorType", operatorType);
    cJSON_AddStringToObject(data, "result", result);
    cJSON_AddItemToObject(root, "data", data);
    Entity_Mqtt_Cmd_Publish(root, QOS1_LEAST_ONCE, 0);
quit:
    cJSON_Delete(root);
}

/**
*@名称 		Entity_Mqtt_Cmd_Clean_Data_Respone
*@功能 		指令应答-清除数据
*@参数 		const char *msgId 
*@参数 		int res 状态码 0代表成功,其他代表失败
*@返回值 	void
*@使用说明	
*/
void Entity_Mqtt_Cmd_Clean_Data_Respone(const char *msgId, int res)
{
    cJSON *root = cJSON_CreateObject();
    Custom_Assert_Check_RequireString_Return_Null(root, "cJSON_CreateObject error");
    Entity_Mqtt_Cmd_Respone_Load_Base(root, msgId, res, ENTITY_COMMAND_CLEAN_DATA_CODE);
    Entity_Mqtt_Cmd_Publish(root, QOS1_LEAST_ONCE, 0);
    cJSON_Delete(root);
}











































