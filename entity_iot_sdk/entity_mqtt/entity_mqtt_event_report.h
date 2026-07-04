//entity_mqtt_event_report.h
#pragma once

#include "cJSON.h"

#include "entity_iot_func.h"
#include "entity_iot_cloud.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

//一次性初始化 token-pending 临界区（幂等，重连安全）
void Entity_Mqtt_Event_Report_Init(void);

//事件上报-设备重置
void Entity_Mqtt_Event_Reset_Report(unsigned char clearData, unsigned char ack);

//事件请求-获取云端时间
void Entity_Mqtt_Event_Get_Time_Request(unsigned char ack);
void Entity_Mqtt_Event_Get_Time_Request_Qos(unsigned char ack, Mqtt_Qos_Type_e qos);

//事件请求-获取物模型
void Entity_Mqtt_Event_Get_Model_Request(Model_Type_e model_type, unsigned char ack);

//事件请求-获取远程配置
void Entity_Mqtt_Event_Get_Config_Request(unsigned char ack);

//事件上报-设备绑定
void Entity_Mqtt_Event_Device_Bind_Report(const char *userId, const char *assetId, const char *version, const char *mcuVersion, unsigned char clean_data, unsigned char ack);

//事件上报-设备信息上报
void Entity_Mqtt_Event_Device_Info_Report(unsigned char bindStatus, const char *version, const char *mcuVersion, cJSON *property, unsigned char ack);
void Entity_Mqtt_Event_Device_Info_Report_Qos(unsigned char bindStatus, const char *version, const char *mcuVersion, cJSON *property, unsigned char ack, Mqtt_Qos_Type_e qos);

//事件上报-属性上报
void Entity_Mqtt_Event_Property_Report(cJSON *property, unsigned char ack);

//事件上报-OTA进度上报
void Entity_Mqtt_Event_Ota_Downloading_Report(int resCode, const char *subPid, const char *subUuid, unsigned char percent, unsigned char ack);

//事件上报-烧录进度上报
void Entity_Mqtt_Event_Ota_Burning_Report(int resCode, const char *subPid, const char *subUuid, unsigned char ack);

//事件上报-版本号上报
void Entity_Mqtt_Event_Ota_Version_Report(int resCode, const char *subPid, const char *subUuid, const char *version, const char *mcuVersion, unsigned char ack);

//事件上报-OTA失败上报
void Entity_Mqtt_Event_Ota_Fail_Report(int resCode, const char *subPid, const char *subUuid, unsigned char ack);

//事件上报-子设备绑定
void Entity_Mqtt_Event_Sub_Bind_Report(const char *uuid, const char *pid, const char *version, const char *mcuVersion, const char *icloud_name, unsigned char ack);

//事件上报-子设备解绑
void Entity_Mqtt_Event_Sub_Delete_Report(const char *uuid, const char *pid, unsigned char ack);

//事件上报-子设备上线
void Entity_Mqtt_Event_Sub_Login_Report(const char **uuid, const char **pid, unsigned char count, unsigned char ack);

//事件上报-子设备下线
void Entity_Mqtt_Event_Sub_Loginout_Report(const char **uuid, const char **pid, unsigned char count,unsigned char ack);

//事件请求-获取子设备物模型
void Entity_Mqtt_Event_Get_Sub_Model_Request(Model_Type_e format, const char *subPid, unsigned char ack);

//事件上报-子设备属性上报
void Entity_Mqtt_Event_Sub_Property_Report(const char *uuid, const char *pid, cJSON *property, unsigned char ack);

//事件请求-通知云端下发网关本地群组/场景/一键执行
void Entity_Mqtt_Event_Local_Config_Request(unsigned char ack);

//事件请求-获取子设备列表
void Entity_Mqtt_Event_Sub_List_Request(unsigned char ack);

//事件请求-获取声网Token(RTM/RTC)
void Entity_Mqtt_Event_Agora_Token_Request(unsigned char rtcNum, const char *channelName, unsigned char ack);

//事件上报-IPC token绑定设备
void Entity_Mqtt_Event_Ipc_Token_Bind_Report(const char *token, unsigned char ack);

//事件请求-查找弹出设备
void Entity_Mqtt_Event_Find_Alert_Request(const char *subPid, const char *subUuid, unsigned char ack);

//事件上报-网关上报搜索到的设备
void Entity_Mqtt_Event_Find_Report(const char *uuid, const char *pid, unsigned char ack);

//事件请求--IPC设备获取实时流推送地址
void Entity_Mqtt_Event_Ipc_Live_Get_Request(Ipc_Push_Type_e type, unsigned char ack);

//事件上报-nfcid上报
void Mqtt_Event_Agora_Agent_Nfc_Report(const char *nfcIdentifier, int ack,int onlyReport);

//事件上报-停止rtc会话
void Mqtt_Event_Agora_Rtc_Stop_Report(const char *channelName, int ack);

//事件上报-无NFC的设备请求声网访问信息
// persona_id: 当前选中的 AI 角色 ID，非空时写入 data.llmExtraParams.personaId
// language: 当前会话语言，非空时写入 data.llmExtraParams.language
void Mqtt_Event_Agora_Agent_Device_Access_Report(int ack, const char *persona_id, const char *language);

/* 返回最近一次 agora_agent_device_access pub 的單調時間戳（ms），用於計算 pub→sub RTT */
uint64_t Entity_Mqtt_Get_Token_Pub_Ms(void);
/* Legacy direct pointer getter; new concurrent code should use
 * Entity_Mqtt_Get_Token_Pending_Snapshot() to copy under a short critical section. */
const char *Entity_Mqtt_Get_Token_Pub_Id(void);
bool Entity_Mqtt_Is_Token_Pending(void);
bool Entity_Mqtt_Is_Token_Response_Current(const char *response_id);
bool Entity_Mqtt_Get_Token_Pending_Snapshot(char *id, size_t id_len, uint64_t *pub_ms);
/* response_id 為 NULL 時強制清掉當前 token pending，用於取消/超時收口。 */
void Entity_Mqtt_Clear_Token_Pending(const char *response_id);

#ifdef __cplusplus
}
#endif
