//entity_mqtt_event_report.c
#include "entity_mqtt_event_report.h"

#include "entity_mqtt_v2_define.h"
#include "entity_mqtt_app.h"
#include "entity_report.h"

#include "entity_iot_cloud.h"

#include "entity_log.h"
#include "entity_wifi.h"
#include "entity_iot_func.h"
#include "entity_dev_info.h"

#include "com_utils.h"
#include "ai_dialog_diag.h"
#include "cJSON.h"

#include <sys/time.h>
#include <string.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef ENTITY_AGORA_DEVICE_ACCESS_QOS
#define ENTITY_AGORA_DEVICE_ACCESS_QOS QOS0_MOST_ONCE
#endif

#ifndef ENTITY_AGORA_DEVICE_ACCESS_USE_LLM_EXTRA_PARAMS
#define ENTITY_AGORA_DEVICE_ACCESS_USE_LLM_EXTRA_PARAMS 0
#endif

/**
*@名称 		Entity_Mqtt_Event_Publish
*@功能 		事件发布
*@参数 		cJSON *root, 
*@参数 		Mqtt_Qos_Type_e qos, 服务质量级别
*@参数 		int retain 是否是保留消息
*@返回值 	void
*@使用说明	将cjson转成无模式字符串后发布
*/
static void Entity_Mqtt_Event_Publish(cJSON *root, Mqtt_Qos_Type_e qos, int retain)
{
    char *json = cJSON_PrintUnformatted(root);
    if (json)
    {
        AI_HOTPATH_VERBOSE_DO(
            ENTITY_LOGD("publish json:%s", json);
        );
        Entity_Mqtt_App_Topic_Publish(TOPIC_TYPE_EVENT_PUBLISH, json, strlen(json), qos, retain);
        cJSON_free(json);
    }
}

/**
*@名称 		Entity_Mqtt_Event_Report_Load_Base
*@功能 		加载事件上报基础内容
*@参数 		cJSON *root, 
*@参数 		const char *msgId
*@参数 		int res
*@参数 		const char *code
*@返回值 	void
*@使用说明	
*/
void Entity_Mqtt_Event_Report_Load_Base(cJSON *root,const char *msgId, int ack, const char *code)
{
    unsigned int timestamp = Entity_Get_Time_Stamp();
    cJSON_AddStringToObject(root, "id", msgId);
    cJSON_AddNumberToObject(root, "ts", timestamp);
    cJSON_AddStringToObject(root, "code", code);
    cJSON_AddNumberToObject(root, "ack", ack);
}


/**
*@名称 		Entity_Mqtt_Event_Reset_Report
*@功能 		事件上报-设备重置
*@参数 		unsigned char clearData 是否清除数据
*@参数      unsigned char ack       是否需要应答
*@返回值 	void
*@使用说明	
*/
void Entity_Mqtt_Event_Reset_Report(unsigned char clearData, unsigned char ack)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    char *msgId = Random_MsgId();
    Custom_Assert_Check_RequireString_Goto((root && data && msgId), quit, "cJSON_CreateObject error");
    Entity_Mqtt_Event_Report_Load_Base(root, msgId, ack, ENTITY_EVENT_RESET_CODE);
    cJSON_AddBoolToObject(data, "cleanData", clearData);
    cJSON_AddItemToObject(root, "data", data);
    Entity_Mqtt_Event_Publish(root, QOS1_LEAST_ONCE, 0);
quit:
    FREE_MEMORY(msgId);
    cJSON_Delete(root);
}


/**
*@名称 		Entity_Mqtt_Event_Get_Time_Request
*@功能 		事件请求-获取云端时间
*@参数      unsigned char ack
*@返回值 	void
*@使用说明	
*/
void Entity_Mqtt_Event_Get_Time_Request_Qos(unsigned char ack, Mqtt_Qos_Type_e qos)
{
    cJSON *root = cJSON_CreateObject();
    char *msgId = Random_MsgId();
    Custom_Assert_Check_RequireString_Goto((root && msgId), quit, "cJSON_CreateObject error");
    Entity_Mqtt_Event_Report_Load_Base(root, msgId, ack, ENTITY_EVENT_TIME_CODE);
    Entity_Mqtt_Event_Publish(root, qos, 0);
quit:
    FREE_MEMORY(msgId);
    cJSON_Delete(root);
}

void Entity_Mqtt_Event_Get_Time_Request(unsigned char ack)
{
    Entity_Mqtt_Event_Get_Time_Request_Qos(ack, QOS1_LEAST_ONCE);
}

/**
*@名称 		Entity_Mqtt_Event_Get_Model_Request
*@功能 		事件请求-获取物模型
*@参数      Model_Type_e model_type, 
*@参数      unsigned char ack 0:不需要回复; 1:需要回复
*@返回值 	void
*@使用说明	
*/
void Entity_Mqtt_Event_Get_Model_Request(Model_Type_e model_type, unsigned char ack)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    char *msgId = Random_MsgId();
    Custom_Assert_Check_RequireString_Goto((root && data && msgId), quit, "cJSON_CreateObject error");
    Entity_Mqtt_Event_Report_Load_Base(root, msgId, ack, ENTITY_EVENT_MODEL_CODE);
    switch (model_type)
    {
        case MODEL_TYPE_COMPLETE:
            cJSON_AddStringToObject(data, "format", "complete");
            break;
        case MODEL_TYPE_SIMPLE:
            cJSON_AddStringToObject(data, "format", "simple");
            break;
        case MODEL_TYPE_MINI:
            cJSON_AddStringToObject(data, "format", "mini");
            break;
        default:
            ENTITY_LOGE("format rang 0~2, complete / simple / mini");
            goto quit;
    }
    cJSON_AddItemToObject(root, "data", data);
    Entity_Mqtt_Event_Publish(root, QOS1_LEAST_ONCE, 0);
quit:
    FREE_MEMORY(msgId);
    cJSON_Delete(root);
}

/**
*@名称 		Entity_Mqtt_Event_Get_Config_Request
*@功能 		事件请求-获取远程配置
*@参数      unsigned char ack 0:不需要回复; 1:需要回复
*@返回值 	void
*@使用说明	
*/
void Entity_Mqtt_Event_Get_Config_Request(unsigned char ack)
{
    cJSON *root = cJSON_CreateObject();
    char *msgId = Random_MsgId();
    Custom_Assert_Check_RequireString_Goto((root && msgId), quit, "cJSON_CreateObject error");
    Entity_Mqtt_Event_Report_Load_Base(root, msgId, ack, ENTITY_EVENT_CONFIG_CODE);
    Entity_Mqtt_Event_Publish(root, QOS1_LEAST_ONCE, 0);
quit:
    FREE_MEMORY(msgId);
    cJSON_Delete(root);
}

/**
*@名称 		Entity_Mqtt_Event_Device_Bind_Report
*@功能 		事件上报-设备绑定
*@参数      const char *userId    配网用户的ID
*@参数      const char *assetId   资产ID
*@参数      const char *version   固件版本号
*@参数      const char *mcuVersion    MCU版本号
*@参数      unsigned char clean_data    清除数据
*@参数      unsigned char ack 0:不需要回复; 1:需要回复
*@返回值 	void
*@使用说明	
*/
void Entity_Mqtt_Event_Device_Bind_Report(const char *userId, const char *assetId, const char *version, const char *mcuVersion, unsigned char clean_data, unsigned char ack)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    char *msgId = Random_MsgId();
    Custom_Assert_Check_RequireString_Goto((root && data && msgId), quit, "cJSON_CreateObject error");
    Entity_Mqtt_Event_Report_Load_Base(root, msgId, ack, ENTITY_EVENT_BIND_CODE);
    cJSON_AddStringToObject(data, "userId", userId);
    cJSON_AddStringToObject(data, "assetId", assetId);
    cJSON_AddStringToObject(data, "version", version);
    cJSON_AddStringToObject(data, "mcuVersion", mcuVersion);
    cJSON_AddBoolToObject(data, "cleanData", clean_data);
    cJSON_AddItemToObject(root, "data", data);
    Entity_Mqtt_Event_Publish(root, QOS1_LEAST_ONCE, 0);
quit:
    FREE_MEMORY(msgId);
    cJSON_Delete(root);
}


/**
*@名称 		Entity_Mqtt_Event_Device_Info_Report
*@功能 		事件上报-设备信息上报
*@参数      unsigned char bindStatus 设备绑定状态 0:未绑定 1：已绑定
*@参数      const char *version 固件版本号
*@参数      const char *mcuVersion MCU版本号
*@参数      cJSON *property 设备信息选填,key-value形式
*@参数      unsigned char ack 0:不需要回复; 1:需要回复
*@返回值 	void
*@使用说明	
*/
void Entity_Mqtt_Event_Device_Info_Report_Qos(unsigned char bindStatus, const char *version, const char *mcuVersion, cJSON *property, unsigned char ack, Mqtt_Qos_Type_e qos)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    char *msgId = Random_MsgId();
    Custom_Assert_Check_RequireString_Goto((root && data && msgId), quit, "cJSON_CreateObject error");
    Entity_Mqtt_Event_Report_Load_Base(root, msgId, ack, ENTITY_EVENT_INFO_CODE);
    cJSON_AddNumberToObject(data, "bindStatus", bindStatus);
    cJSON_AddStringToObject(data, "version", version);
    cJSON_AddStringToObject(data, "mcuVersion", mcuVersion);
    if(property) 
    {
        char *config = cJSON_PrintUnformatted(property);
        if(config) {
            cJSON_AddStringToObject(data, "config", config);
            cJSON_free(config);
        }
    }
    cJSON_AddItemToObject(root, "data", data);
    Entity_Mqtt_Event_Publish(root, qos, 0);
quit:
    FREE_MEMORY(msgId);
    cJSON_Delete(root);
}

void Entity_Mqtt_Event_Device_Info_Report(unsigned char bindStatus, const char *version, const char *mcuVersion, cJSON *property, unsigned char ack)
{
    Entity_Mqtt_Event_Device_Info_Report_Qos(bindStatus, version, mcuVersion, property, ack, QOS1_LEAST_ONCE);
}

/**
*@名称 		Entity_Mqtt_Event_Property_Report
*@功能 		事件上报-属性上报
*@参数      cJSON *property key-value形式json字符串
*@参数      unsigned char ack 0:不需要回复; 1:需要回复
*@返回值 	void
*@使用说明	
*/
void Entity_Mqtt_Event_Property_Report(cJSON *property, unsigned char ack)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    char *msgId = Random_MsgId();
    Custom_Assert_Check_RequireString_Goto((root && data && msgId), quit, "cJSON_CreateObject error");
    Entity_Mqtt_Event_Report_Load_Base(root, msgId, ack, ENTITY_EVENT_PROPERTY_REPORT_CODE);
    cJSON *object1 = cJSON_Duplicate(property, 1);
    cJSON_AddItemToObject(data, "properties", object1);
    cJSON_AddItemToObject(root, "data", data);
    Entity_Event_Report_Send(root, QOS1_LEAST_ONCE);
quit:
    FREE_MEMORY(msgId);
    cJSON_Delete(root);
}


/**
*@名称 		Entity_Mqtt_Event_Ota_Downloading_Report
*@功能 		事件上报-OTA进度上报
*@参数      int resCode 0:成功, -1:下载超时, -2:文件不存在, -3:签名过期, -4:MD5不匹配, -5:更新固件失败
*@参数      const char *subPid    子设备产品ID-非必选, 若是升级的是网关子设备必填, 传子设备的pid
*@参数      const char *subUuid   子设备uuid-非必选, 若是升级的是网关子设备必填, 传子设备的uuid
*@参数      unsigned char percent   ota进度
*@参数      unsigned char ack 0:不需要回复; 1:需要回复
*@返回值 	void
*@使用说明	
*/
void Entity_Mqtt_Event_Ota_Downloading_Report(int resCode, const char *subPid, const char *subUuid, unsigned char percent, unsigned char ack)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    char *msgId = Random_MsgId();
    Custom_Assert_Check_RequireString_Goto((root && data && msgId), quit, "cJSON_CreateObject error");
    Entity_Mqtt_Event_Report_Load_Base(root, msgId, ack, ENTITY_EVENT_OTA_PROGRESS_CODE);
    cJSON_AddNumberToObject(data, "resCode", resCode);
    if(subPid) {
        cJSON_AddStringToObject(data, "subPid", subPid);
    }
    if(subUuid) {
        cJSON_AddStringToObject(data, "subUuid", subUuid);
    }
    cJSON_AddStringToObject(data, "type", "downloading");
    cJSON_AddNumberToObject(data, "percent", percent);

    cJSON_AddItemToObject(root, "data", data);
    Entity_Event_Report_Send(root, QOS1_LEAST_ONCE);
quit:
    FREE_MEMORY(msgId);
    cJSON_Delete(root);
}


/**
*@名称 		Entity_Mqtt_Event_Ota_Burning_Report
*@功能 		事件上报-烧录进度上报
*@参数      int resCode 0:成功, -1:下载超时, -2:文件不存在, -3:签名过期, -4:MD5不匹配, -5:更新固件失败
*@参数      const char *subPid    子设备产品ID-非必选, 若是升级的是网关子设备必填, 传子设备的pid
*@参数      const char *subUuid   子设备uuid-非必选, 若是升级的是网关子设备必填, 传子设备的uuid
*@参数      unsigned char ack 0:不需要回复; 1:需要回复
*@返回值 	void
*@使用说明	
*/
void Entity_Mqtt_Event_Ota_Burning_Report(int resCode, const char *subPid, const char *subUuid, unsigned char ack)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    char *msgId = Random_MsgId();
    Custom_Assert_Check_RequireString_Goto((root && data && msgId), quit, "cJSON_CreateObject error");
    Entity_Mqtt_Event_Report_Load_Base(root, msgId, ack, ENTITY_EVENT_OTA_PROGRESS_CODE);
    cJSON_AddNumberToObject(data, "resCode", resCode);
    if(subPid) {
        cJSON_AddStringToObject(data, "subPid", subPid);
    }
    if(subUuid) {
        cJSON_AddStringToObject(data, "subUuid", subUuid);
    }
    cJSON_AddStringToObject(data, "type", "burning");

    cJSON_AddItemToObject(root, "data", data);
    Entity_Event_Report_Send(root, QOS1_LEAST_ONCE);
quit:
    FREE_MEMORY(msgId);
    cJSON_Delete(root);
}

/**
*@名称 		Entity_Mqtt_Event_Ota_Version_Report
*@功能 		事件上报-版本号上报
*@参数      int resCode 0:成功, -1:下载超时, -2:文件不存在, -3:签名过期, -4:MD5不匹配, -5:更新固件失败
*@参数      const char *subPid    子设备产品ID-非必选, 若是升级的是网关子设备必填, 传子设备的pid
*@参数      const char *subUuid   子设备uuid-非必选, 若是升级的是网关子设备必填, 传子设备的uuid
*@参数      const char *version,    设备当前版本号(设备升级完成时上报)
*@参数      const char *mcuVersion  MCU当前版本号(设备升级完成时上报)
*@参数      unsigned char ack 0:不需要回复; 1:需要回复
*@返回值 	void
*@使用说明	OTA成功才上报版本号
*/
void Entity_Mqtt_Event_Ota_Version_Report(int resCode, const char *subPid, const char *subUuid, const char *version, const char *mcuVersion, unsigned char ack)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    char *msgId = Random_MsgId();
    Custom_Assert_Check_RequireString_Goto((root && data && msgId), quit, "cJSON_CreateObject error");
    Entity_Mqtt_Event_Report_Load_Base(root, msgId, ack, ENTITY_EVENT_OTA_PROGRESS_CODE);
    cJSON_AddNumberToObject(data, "resCode", resCode);
    cJSON_AddStringToObject(data, "type", "report");
    if(subPid)
        cJSON_AddStringToObject(data, "subPid", subPid);
    if(subUuid)
        cJSON_AddStringToObject(data, "subUuid", subUuid);
    if(version)
        cJSON_AddStringToObject(data, "version", version);
    if(mcuVersion)
        cJSON_AddStringToObject(data, "mcuVersion", mcuVersion);

    cJSON_AddItemToObject(root, "data", data);
    Entity_Mqtt_Event_Publish(root, QOS1_LEAST_ONCE, 0);
quit:
    FREE_MEMORY(msgId);
    cJSON_Delete(root);
}

/**
*@名称 		Entity_Mqtt_Event_Ota_Fail_Report
*@功能 		事件上报-OTA失败上报
*@参数      int resCode 0:成功, -1:下载超时, -2:文件不存在, -3:签名过期, -4:MD5不匹配, -5:更新固件失败
*@参数      const char *subPid    子设备产品ID-非必选, 若是升级的是网关子设备必填, 传子设备的pid
*@参数      const char *subUuid   子设备uuid-非必选, 若是升级的是网关子设备必填, 传子设备的uuid
*@参数      unsigned char ack 0:不需要回复; 1:需要回复
*@返回值 	void
*@使用说明	
*/
void Entity_Mqtt_Event_Ota_Fail_Report(int resCode, const char *subPid, const char *subUuid, unsigned char ack)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    char *msgId = Random_MsgId();
    Custom_Assert_Check_RequireString_Goto((root && data && msgId), quit, "cJSON_CreateObject error");
    Entity_Mqtt_Event_Report_Load_Base(root, msgId, ack, ENTITY_EVENT_OTA_PROGRESS_CODE);
    cJSON_AddNumberToObject(data, "resCode", resCode);
    cJSON_AddStringToObject(data, "type", "fail");
    if(subPid)
        cJSON_AddStringToObject(data, "subPid", subPid);
    if(subUuid)
        cJSON_AddStringToObject(data, "subUuid", subUuid);

    cJSON_AddItemToObject(root, "data", data);
    Entity_Mqtt_Event_Publish(root, QOS1_LEAST_ONCE, 0);
quit:
    FREE_MEMORY(msgId);
    cJSON_Delete(root);
}


/**
*@名称 		Entity_Mqtt_Event_Sub_Bind_Report
*@功能 		事件上报-子设备绑定
*@参数      const char *uuid  子设备uuid
*@参数      const char *pid    子设备产品类型
*@参数      const char *version   设备当前版本号
*@参数      const char *mcuVersion  MCU当前版本号 
*@参数      const char *icloud_name iCloud 名称,例如: null:rino-犀云
*@参数      unsigned char ack 0:不需要回复; 1:需要回复
*@返回值 	void
*@使用说明	
*/
void Entity_Mqtt_Event_Sub_Bind_Report(const char *uuid, const char *pid, const char *version, const char *mcuVersion, const char *icloud_name, unsigned char ack)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    cJSON *object = cJSON_CreateObject();
    cJSON *array = cJSON_CreateArray();
    char *msgId = Random_MsgId();
    Custom_Assert_Check_RequireString_Goto((root && data && object && array && msgId), quit, "cJSON_CreateObject error");
    Entity_Mqtt_Event_Report_Load_Base(root, msgId, ack, ENTITY_EVENT_SUB_BIND_CODE);
    cJSON_AddStringToObject(object, "uuid", uuid);
    cJSON_AddStringToObject(object, "pid", pid);
    cJSON_AddStringToObject(object, "version", version);
    cJSON_AddStringToObject(object, "mcuVersion", mcuVersion);
    if (icloud_name == NULL)
    {
        cJSON_AddStringToObject(object, "type", "rino");
    }
    else
    {
        cJSON_AddStringToObject(object, "type", icloud_name);
    }
    cJSON_AddItemToArray(array, object);
    cJSON_AddItemToObject(data, "devices", array);
    cJSON_AddItemToObject(root, "data", data);
    Entity_Mqtt_Event_Publish(root, QOS1_LEAST_ONCE, 0);
quit:
    FREE_MEMORY(msgId);
    cJSON_Delete(root);
}

/**
*@名称 		Entity_Mqtt_Event_Sub_Delete_Report
*@功能 		事件上报-子设备解绑
*@参数      const char *uuid  子设备uuid
*@参数      const char *pid    子设备产品类型
*@参数      unsigned char ack 0:不需要回复; 1:需要回复
*@返回值 	void
*@使用说明	
*/
void Entity_Mqtt_Event_Sub_Delete_Report(const char *uuid, const char *pid, unsigned char ack)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    cJSON *object = cJSON_CreateObject();
    cJSON *array = cJSON_CreateArray();
    char *msgId = Random_MsgId();
    Custom_Assert_Check_RequireString_Goto((root && data && object && array && msgId), quit, "cJSON_CreateObject error");
    Entity_Mqtt_Event_Report_Load_Base(root, msgId, ack, ENTITY_EVENT_SUB_DELETE_CODE);
    cJSON_AddStringToObject(object, "uuid", uuid);
    cJSON_AddStringToObject(object, "pid", pid);
    cJSON_AddItemToArray(array, object);
    cJSON_AddItemToObject(data, "devices", array);
    cJSON_AddItemToObject(root, "data", data);
    Entity_Mqtt_Event_Publish(root, QOS1_LEAST_ONCE, 0);
quit:
    FREE_MEMORY(msgId);
    cJSON_Delete(root);
}

/**
*@名称 		Entity_Mqtt_Event_Sub_Login_Report
*@功能 		事件上报-子设备上线
*@参数      const char **uuid  子设备uuid
*@参数      const char **pid    子设备产品类型
*@参数      unsigned char count 子设备个数
*@参数      unsigned char ack 0:不需要回复; 1:需要回复
*@返回值 	void
*@使用说明	
*/
void Entity_Mqtt_Event_Sub_Login_Report(const char **uuid, const char **pid, unsigned char count, unsigned char ack)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    cJSON *array = cJSON_CreateArray();
    char *msgId = Random_MsgId();
    Custom_Assert_Check_RequireString_Goto((root && data && array && msgId), quit, "cJSON_CreateObject error");
    Entity_Mqtt_Event_Report_Load_Base(root, msgId, ack, ENTITY_EVENT_SUB_LOGIN_CODE);
    for(int i=0; i<count; i++)
    {
        if(uuid[i] && pid[i])
        {
            cJSON *object1 = cJSON_CreateObject();
            cJSON_AddStringToObject(object1, "uuid", uuid[i]);
            cJSON_AddStringToObject(object1, "pid", pid[i]);
            cJSON_AddItemToArray(array, object1);
        }
    }
    cJSON_AddItemToObject(data, "devices", array);
    cJSON_AddItemToObject(root, "data", data);
    Entity_Mqtt_Event_Publish(root, QOS1_LEAST_ONCE, 0);
quit:
    FREE_MEMORY(msgId);
    cJSON_Delete(root);
}


/**
*@名称 		Entity_Mqtt_Event_Sub_Loginout_Report
*@功能 		事件上报-子设备下线
*@参数      const char **uuid  子设备uuid
*@参数      const char **pid    子设备产品类型
*@参数      unsigned char count 子设备个数
*@参数      unsigned char ack 0:不需要回复; 1:需要回复
*@返回值 	void
*@使用说明	
*/
void Entity_Mqtt_Event_Sub_Loginout_Report(const char **uuid, const char **pid, unsigned char count,unsigned char ack)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    cJSON *array = cJSON_CreateArray();
    char *msgId = Random_MsgId();
    Custom_Assert_Check_RequireString_Goto((root && data && array && msgId), quit, "cJSON_CreateObject error");
    Entity_Mqtt_Event_Report_Load_Base(root, msgId, ack, ENTITY_EVENT_SUB_LOGINOUT_CODE);
    for(int i=0; i<count; i++)
    {
        if(uuid[i] && pid[i])
        {
            cJSON *object1 = cJSON_CreateObject();
            cJSON_AddStringToObject(object1, "uuid", uuid[i]);
            cJSON_AddStringToObject(object1, "pid", pid[i]);
            cJSON_AddItemToArray(array, object1);
        }
    }
    cJSON_AddItemToObject(data, "devices", array);
    cJSON_AddItemToObject(root, "data", data);
    Entity_Mqtt_Event_Publish(root, QOS1_LEAST_ONCE, 0);
quit:
    FREE_MEMORY(msgId);
    cJSON_Delete(root);
}

/**
*@名称 		Entity_Mqtt_Event_Get_Sub_Model_Request
*@功能 		事件请求-获取子设备物模型
*@参数      Model_Type_e format
*@参数      const char *subPid    子设备产品类型
*@参数      unsigned char ack 0:不需要回复; 1:需要回复
*@返回值 	void
*@使用说明	
*/
void Entity_Mqtt_Event_Get_Sub_Model_Request(Model_Type_e format, const char *subPid, unsigned char ack)
{
     cJSON *root = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    char *msgId = Random_MsgId();
    Custom_Assert_Check_RequireString_Goto((root && data && msgId), quit, "cJSON_CreateObject error");
    Entity_Mqtt_Event_Report_Load_Base(root, msgId, ack, ENTITY_EVENT_SUB_MODEL_CODE);
    switch (format)
    {
        case MODEL_TYPE_COMPLETE:
            cJSON_AddStringToObject(data, "format", "complete");
            break;
        case MODEL_TYPE_SIMPLE:
            cJSON_AddStringToObject(data, "format", "simple");
            break;
        case MODEL_TYPE_MINI:
            cJSON_AddStringToObject(data, "format", "mini");
            break;
        default:
            goto quit;
    }
    cJSON_AddStringToObject(data, "subPid", subPid);

    cJSON_AddItemToObject(root, "data", data);
    Entity_Mqtt_Event_Publish(root, QOS1_LEAST_ONCE, 0);
quit:
    FREE_MEMORY(msgId);
    cJSON_Delete(root);
}


/**
*@名称 		Entity_Mqtt_Event_Sub_Property_Report
*@功能 		事件上报-子设备属性上报
*@参数      const char *uuid 设备ID
*@参数      const char *pid    子设备产品类型
*@参数      cJSON *property key-value形式json字符串
*@参数      unsigned char ack 0:不需要回复; 1:需要回复
*@返回值 	void
*@使用说明	
*/
void Entity_Mqtt_Event_Sub_Property_Report(const char *uuid, const char *pid, cJSON *property, unsigned char ack)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    cJSON *array = cJSON_CreateArray();
    cJSON *object = cJSON_CreateObject();
    char *msgId = Random_MsgId();
    Custom_Assert_Check_RequireString_Goto((root && data && array && object && msgId), quit, "cJSON_CreateObject error");
    Entity_Mqtt_Event_Report_Load_Base(root, msgId, ack, ENTITY_EVENT_SUB_PROPERTY_REPORT_CODE);
    
    cJSON_AddStringToObject(object, "uuid", uuid);
    cJSON_AddStringToObject(object, "pid", pid);
    cJSON *object2 = cJSON_Duplicate(property, 1);//1递归子目录
    cJSON_AddItemToObject(object, "properties", object2);
    cJSON_AddItemToArray(array, object);
    cJSON_AddItemToObject(data, "devices", array);
    cJSON_AddItemToObject(root, "data", data);
    Entity_Mqtt_Event_Publish(root, QOS1_LEAST_ONCE, 0);
quit:
    FREE_MEMORY(msgId);
    cJSON_Delete(root);
}

/**
*@名称 		Entity_Mqtt_Event_Local_Config_Request
*@功能 		事件请求-通知云端下发网关本地群组/场景/一键执行
*@参数      unsigned char ack 0:不需要回复; 1:需要回复
*@返回值 	void
*@使用说明	
*/
void Entity_Mqtt_Event_Local_Config_Request(unsigned char ack)
{
    cJSON *root = cJSON_CreateObject();
    char *msgId = Random_MsgId();
    Custom_Assert_Check_RequireString_Goto((root && msgId), quit, "cJSON_CreateObject error");
    Entity_Mqtt_Event_Report_Load_Base(root, msgId, ack, ENTITY_EVENT_LOCAL_CONFIG_CODE);
    Entity_Mqtt_Event_Publish(root, QOS1_LEAST_ONCE, 0);
quit:
    FREE_MEMORY(msgId);
    cJSON_Delete(root);
}

/**
*@名称 		Entity_Mqtt_Event_Sub_List_Request
*@功能 		事件请求-获取子设备列表
*@参数      unsigned char ack 0:不需要回复; 1:需要回复
*@返回值 	void
*@使用说明	
*/
void Entity_Mqtt_Event_Sub_List_Request(unsigned char ack)
{
    cJSON *root = cJSON_CreateObject();
    char *msgId = Random_MsgId();
    Custom_Assert_Check_RequireString_Goto((root && msgId), quit, "cJSON_CreateObject error");
    Entity_Mqtt_Event_Report_Load_Base(root, msgId, ack, ENTITY_EVENT_SUB_LIST_CODE);
    Entity_Mqtt_Event_Publish(root, QOS1_LEAST_ONCE, 0);
quit:
    FREE_MEMORY(msgId);
    cJSON_Delete(root);
}


/**
*@名称 		Entity_Mqtt_Event_Agora_Token_Request
*@功能 		事件请求-获取声网Token(RTM/RTC)
*@参数      unsigned char rtcNum 需要获取的rtc token 数量, 不传默认获取1个
*@参数      const char *channelName 通道名称
*@参数      unsigned char ack 0:不需要回复; 1:需要回复
*@返回值 	void
*@使用说明	
*/
void Entity_Mqtt_Event_Agora_Token_Request(unsigned char rtcNum, const char *channelName, unsigned char ack)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    char *msgId = Random_MsgId();
    Custom_Assert_Check_RequireString_Goto((root && data && msgId), quit, "cJSON_CreateObject error");
    Entity_Mqtt_Event_Report_Load_Base(root, msgId, ack, ENTITY_EVENT_AGORA_TOKEN_CODE);
    cJSON_AddNumberToObject(data, "rtcNum", rtcNum);
    cJSON_AddStringToObject(data, "channelName", channelName);

    cJSON_AddItemToObject(root, "data", data);
    Entity_Mqtt_Event_Publish(root, QOS1_LEAST_ONCE, 0);
quit:
    FREE_MEMORY(msgId);
    cJSON_Delete(root);
}


/**
*@名称 		Entity_Mqtt_Event_Ipc_Token_Bind_Report
*@功能 		事件上报-IPC token绑定设备
*@参数      const char *token
*@参数      unsigned char ack 0:不需要回复; 1:需要回复
*@返回值 	void
*@使用说明	
*/
void Entity_Mqtt_Event_Ipc_Token_Bind_Report(const char *token, unsigned char ack)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    char *msgId = Random_MsgId();
    Custom_Assert_Check_RequireString_Goto((root && data && msgId), quit, "cJSON_CreateObject error");
    Entity_Mqtt_Event_Report_Load_Base(root, msgId, ack, ENTITY_EVENT_IPC_TOKEN_BIND_CODE);
    cJSON_AddStringToObject(data, "token", token);

    cJSON_AddItemToObject(root, "data", data);
    Entity_Mqtt_Event_Publish(root, QOS1_LEAST_ONCE, 0);
quit:
    FREE_MEMORY(msgId);
    cJSON_Delete(root);
}

/**
*@名称 		Entity_Mqtt_Event_Find_Alert_Request
*@功能 		事件请求-查找弹出设备
*@参数      const char *subPid 设备类型ID
*@参数      const char *subUuid 设备ID
*@参数      unsigned char ack 0:不需要回复; 1:需要回复
*@返回值 	void
*@使用说明	
*/
void Entity_Mqtt_Event_Find_Alert_Request(const char *subPid, const char *subUuid, unsigned char ack)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    char *msgId = Random_MsgId();
    Custom_Assert_Check_RequireString_Goto((root && data && msgId), quit, "cJSON_CreateObject error");
    Entity_Mqtt_Event_Report_Load_Base(root, msgId, ack, ENTITY_EVENT_FIND_ALERT_CODE);
    cJSON_AddStringToObject(data, "subPid", subPid);
    cJSON_AddStringToObject(data, "subUuid", subUuid);

    cJSON_AddItemToObject(root, "data", data);
    Entity_Mqtt_Event_Publish(root, QOS1_LEAST_ONCE, 0);
quit:
    FREE_MEMORY(msgId);
    cJSON_Delete(root);
}

/**
*@名称 		Entity_Mqtt_Event_Find_Report
*@功能 		事件上报-网关上报搜索到的设备
*@参数      const char *uuid 设备ID
*@参数      const char *pid 设备类型
*@参数      unsigned char ack 0:不需要回复; 1:需要回复
*@返回值 	void
*@使用说明	
*/
void Entity_Mqtt_Event_Find_Report(const char *uuid, const char *pid, unsigned char ack)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    cJSON *array = cJSON_CreateArray();
    cJSON *object = cJSON_CreateObject();
    char *msgId = Random_MsgId();
    Custom_Assert_Check_RequireString_Goto((root && data && array && object && msgId), quit, "cJSON_CreateObject error");
    Entity_Mqtt_Event_Report_Load_Base(root, msgId, ack, ENTITY_EVENT_FIND_REPORT_CODE);
    
    cJSON_AddStringToObject(object, "uuid", uuid);
    cJSON_AddStringToObject(object, "pid", pid);
    cJSON_AddItemToArray(array, object);
    cJSON_AddItemToObject(data, "devices", array);
    cJSON_AddItemToObject(root, "data", data);
    Entity_Mqtt_Event_Publish(root, QOS1_LEAST_ONCE, 0);
quit:
    FREE_MEMORY(msgId);
    cJSON_Delete(root);
}

/**
*@名称 		Entity_Mqtt_Event_Ipc_Live_Get_Request
*@功能 		事件请求--IPC设备获取实时流推送地址
*@参数      Ipc_Push_Type_e type 实时流类型： Ipc_Push_Type_e
*@参数      unsigned char ack 0:不需要回复; 1:需要回复
*@返回值 	void
*@使用说明	
*/
void Entity_Mqtt_Event_Ipc_Live_Get_Request(Ipc_Push_Type_e type, unsigned char ack)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    char *msgId = Random_MsgId();
    Custom_Assert_Check_RequireString_Goto((root && data && msgId), quit, "cJSON_CreateObject error");
    Entity_Mqtt_Event_Report_Load_Base(root, msgId, ack, ENTITY_EVENT_IPC_LIVE_GET_CODE);
    switch (type)
    {
    case IPC_RTSPS:
        cJSON_AddStringToObject(data, "type", "rtsps");
        break;
    case IPC_RTMPS:
        cJSON_AddStringToObject(data, "subPid", "rtmps");
        break;
    default:
        goto quit;
    }

    cJSON_AddItemToObject(root, "data", data);
    Entity_Mqtt_Event_Publish(root, QOS1_LEAST_ONCE, 0);
quit:
    FREE_MEMORY(msgId);
    cJSON_Delete(root);
}

/**
*@名称 		Mqtt_Event_Agora_Agent_Nfc_Report
*@功能 		事件上报-nfcid上报
*@参数      const char *nfcIdentifier nfc标签的id
*@参数      int ack 0:不需要回复; 1:需要回复
*@参数      int onlyReport 0:需要创建会话; 1:不需要创建会话
*@返回值 	void
*@使用说明	
*/
void Mqtt_Event_Agora_Agent_Nfc_Report(const char *nfcIdentifier, int ack,int onlyReport)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    char *msgId = Random_MsgId();
    Custom_Assert_Check_RequireString_Goto((root && data && msgId), quit, "cJSON_CreateObject error");
    Entity_Mqtt_Event_Report_Load_Base(root, msgId, ack, ENTITY_EVENT_AGORA_AGENT_NFC_REPORT_CODE);
    cJSON_AddStringToObject(data,"nfcIdentifier",nfcIdentifier);
    cJSON_AddNumberToObject(data,"onlyReport",onlyReport);
    cJSON_AddItemToObject(root, "data", data);
    Entity_Mqtt_Event_Publish(root, QOS1_LEAST_ONCE, 0);
quit:
    FREE_MEMORY(msgId);
    cJSON_Delete(root);
}
/**
*@名称 		Mqtt_Event_Agora_Rtc_Stop_Report
*@功能 		事件上报-停止rtc会话
*@参数      const char *channelName 通道名称
*@参数      int ack 0:不需要回复; 1:需要回复
*@返回值 	void
*@使用说明	
*/
void Mqtt_Event_Agora_Rtc_Stop_Report(const char *channelName, int ack)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    char *msgId = Random_MsgId();
    Custom_Assert_Check_RequireString_Goto((root && data && msgId), quit, "cJSON_CreateObject error");
    Entity_Mqtt_Event_Report_Load_Base(root, msgId, ack, ENTITY_EVENT_AGORA_RTC_STOP_REPORT_CODE);
    cJSON_AddStringToObject(data,"channelName",channelName);
    cJSON_AddItemToObject(root, "data", data);
    Entity_Mqtt_Event_Publish(root, QOS1_LEAST_ONCE, 0);
quit:
    FREE_MEMORY(msgId);
    cJSON_Delete(root);
}

/**
*@名称 		Mqtt_Event_Agora_Agent_Device_Access_Report
*@功能 		事件上报-无NFC的设备请求声网访问信息
*@参数      int ack 0:不需要回复; 1:需要回复
*@参数      const char *persona_id 当前选中的 AI 角色 ID，非空时写入 data.llmExtraParams.personaId
*@参数      const char *language 当前会话语言，非空时写入 data.llmExtraParams.language
*@返回值 	void
*@使用说明
*/
static uint64_t s_token_pub_ms = 0;
static char s_token_pub_id[32] = {0};
static Entity_Critical_t s_token_pending_mux = NULL;

/* @brief 一次性初始化 token-pending 临界区（幂等，避免重复建立泄漏） */
void Entity_Mqtt_Event_Report_Init(void)
{
    if (s_token_pending_mux != NULL) { return; }
    Entity_Critical_Create(&s_token_pending_mux);
}

uint64_t Entity_Mqtt_Get_Token_Pub_Ms(void)
{
    uint64_t pub_ms;

    Entity_Critical_Enter(&s_token_pending_mux);
    pub_ms = s_token_pub_ms;
    Entity_Critical_Exit(&s_token_pending_mux);
    return pub_ms;
}

const char *Entity_Mqtt_Get_Token_Pub_Id(void)
{
    const char *id;

    Entity_Critical_Enter(&s_token_pending_mux);
    id = s_token_pub_id[0] ? s_token_pub_id : NULL;
    Entity_Critical_Exit(&s_token_pending_mux);
    return id;
}

bool Entity_Mqtt_Is_Token_Pending(void)
{
    bool pending;

    Entity_Critical_Enter(&s_token_pending_mux);
    pending = (s_token_pub_ms > 0U) && (s_token_pub_id[0] != '\0');
    Entity_Critical_Exit(&s_token_pending_mux);
    return pending;
}

bool Entity_Mqtt_Is_Token_Response_Current(const char *response_id)
{
    bool current = false;

    Entity_Critical_Enter(&s_token_pending_mux);
    if (response_id == NULL || response_id[0] == '\0' || s_token_pub_id[0] == '\0')
    {
        current = false;
    }
    else
    {
        current = (strcmp(response_id, s_token_pub_id) == 0);
    }
    Entity_Critical_Exit(&s_token_pending_mux);

    return current;
}

bool Entity_Mqtt_Get_Token_Pending_Snapshot(char *id, size_t id_len, uint64_t *pub_ms)
{
    bool pending;

    Entity_Critical_Enter(&s_token_pending_mux);
    pending = (s_token_pub_ms > 0U) && (s_token_pub_id[0] != '\0');
    if (id && id_len > 0U)
    {
        if (pending)
        {
            size_t copy_len = strnlen(s_token_pub_id, id_len - 1U);
            memcpy(id, s_token_pub_id, copy_len);
            id[copy_len] = '\0';
        }
        else
        {
            id[0] = '\0';
        }
    }
    if (pub_ms)
    {
        *pub_ms = pending ? s_token_pub_ms : 0U;
    }
    Entity_Critical_Exit(&s_token_pending_mux);

    return pending;
}

void Entity_Mqtt_Clear_Token_Pending(const char *response_id)
{
    Entity_Critical_Enter(&s_token_pending_mux);
    if (response_id == NULL)
    {
        s_token_pub_id[0] = '\0';
        s_token_pub_ms = 0;
        Entity_Critical_Exit(&s_token_pending_mux);
        return;
    }

    if ((response_id[0] == '\0') ||
        (s_token_pub_id[0] == '\0') ||
        (strcmp(response_id, s_token_pub_id) != 0))
    {
        Entity_Critical_Exit(&s_token_pending_mux);
        return;
    }

    s_token_pub_id[0] = '\0';
    s_token_pub_ms = 0;
    Entity_Critical_Exit(&s_token_pending_mux);
}

void Mqtt_Event_Agora_Agent_Device_Access_Report(int ack, const char *persona_id, const char *language)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    char *msgId = Random_MsgId();
    bool has_persona = (persona_id != NULL && persona_id[0] != '\0');
    bool has_language = (language != NULL && language[0] != '\0');
    uint64_t pub_ms = 0;
    Custom_Assert_Check_RequireString_Goto((root && data && msgId), quit, "cJSON_CreateObject error");
    pub_ms = Entity_Get_Uptime_Ms();
    Entity_Critical_Enter(&s_token_pending_mux);
    s_token_pub_ms = pub_ms;
    {
        size_t copy_len = strnlen(msgId, sizeof(s_token_pub_id) - 1U);
        memcpy(s_token_pub_id, msgId, copy_len);
        s_token_pub_id[copy_len] = '\0';
    }
    Entity_Critical_Exit(&s_token_pending_mux);
    AI_DIALOG_DIAG_LOGI("ENTITY_MQTT", "token_publish_payload",
                        "msgId=%s qos=%d ack=%d persona=%s language=%s pub_mono=%" PRIu64 "ms",
                        msgId,
                        ENTITY_AGORA_DEVICE_ACCESS_QOS,
                        ack,
                        has_persona ? persona_id : "(none)",
                        has_language ? language : "(none)",
                        pub_ms);
    AI_HOTPATH_VERBOSE_DO(
        ENTITY_LOGI("[RTC_TOKEN] request agora_agent_device_access: msgId=%s personaId=%s language=%s mono_t=%" PRIu64 "ms\r\n",
                  msgId,
                  has_persona ? persona_id : "(none)",
                  has_language ? language : "(none)",
                  pub_ms);
    );
    Entity_Mqtt_Event_Report_Load_Base(root, msgId, ack, ENTITY_EVENT_AGORA_AGENT_DEVICE_ACCESS_CODE);

    /*
     * P0: 临时对齐 PC 验证 payload，agora_agent_device_access 只发送 data:{}。
     * 如需恢复角色/语言扩展，把 ENTITY_AGORA_DEVICE_ACCESS_USE_LLM_EXTRA_PARAMS 改为 1。
     */
#if ENTITY_AGORA_DEVICE_ACCESS_USE_LLM_EXTRA_PARAMS
    /* 原始逻辑保留：当 persona_id 或 language 非空时，注入 data.llmExtraParams。 */
    if (has_persona || has_language)
    {
        cJSON *llm_extra = cJSON_CreateObject();
        if (llm_extra)
        {
            if (has_persona)
            {
                cJSON_AddStringToObject(llm_extra, "personaId", persona_id);
            }
            if (has_language)
            {
                cJSON_AddStringToObject(llm_extra, "language", language);
            }
            cJSON_AddItemToObject(data, "llmExtraParams", llm_extra);
        }
        else
        {
            ENTITY_LOGW("[RTC_TOKEN] llmExtraParams create failed, fallback to data:{}\r\n");
        }
    }
#else
    if (has_persona || has_language)
    {
        AI_HOTPATH_VERBOSE_DO(
            ENTITY_LOGW("[RTC_TOKEN][P0] temporarily ignore llmExtraParams, send PC-verified data:{} personaId=%s language=%s\r\n",
                      has_persona ? persona_id : "(none)",
                      has_language ? language : "(none)");
        );
    }
#endif
    cJSON_AddItemToObject(root, "data", data);
    data = NULL;

    bool mqtt_conn = Entity_Mqtt_App_Is_Connected();
    AI_DIALOG_DIAG_LOGI("ENTITY_MQTT", "token_publish_submit",
                        "mqtt_connected=%d qos=%d",
                        mqtt_conn ? 1 : 0,
                        ENTITY_AGORA_DEVICE_ACCESS_QOS);
    AI_HOTPATH_VERBOSE_DO(
        ENTITY_LOGI("[RTC_TOKEN] MQTT state before publish=%s\r\n", mqtt_conn ? "connected" : "disconnected/reconnecting");
        ENTITY_LOGW("[RTC_TOKEN][QOS0_DIAG] agora_agent_device_access temporarily uses QoS0 to verify whether PUBACK/outbox affects token response\r\n");
    );
    Ai_Dialog_Timing_Mark_Publish();
    Entity_Mqtt_Event_Publish(root, ENTITY_AGORA_DEVICE_ACCESS_QOS, 0);
    AI_HOTPATH_VERBOSE_DO(
        ENTITY_LOGI("[RTC_TOKEN] agora_agent_device_access MQTT request published qos=%d\r\n", ENTITY_AGORA_DEVICE_ACCESS_QOS);
    );
quit:
    cJSON_Delete(data);
    FREE_MEMORY(msgId);
    cJSON_Delete(root);
}
