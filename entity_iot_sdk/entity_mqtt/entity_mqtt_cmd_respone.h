//entity_mqtt_cmd_respone.h
#pragma once


#include "cJSON.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

//指令应答-设备重置
void Entity_Mqtt_Cmd_Reset_Respone(const char *msgId, int res);

//指令应答-固件升级
void Entity_Mqtt_Cmd_Ota_Respone(const char *msgId, int res);

//指令应答-检测设备在线状态
void Entity_Mqtt_Cmd_Ping_Respone(cJSON *data,const char *msgId, int res);

//指令应答-设置属性
void Entity_Mqtt_Cmd_Property_Set_Respone(cJSON *property, const char *msgId, int res);

//指令应答-本地群组/场景/一键执行配置下发
void Entity_Mqtt_Cmd_Update_Local_Config_Respone(const char *msgId, int res);

//指令应答-搜索并绑定子设备
void Entity_Mqtt_Cmd_Find_Bind_Respone(const char *msgId, int res);

//指令应答-批量设置子设备属性
void Entity_Mqtt_Cmd_Sub_Property_Set_Respone(cJSON *data, const char *msgId, int res);

//指令应答-删除子设备
void Entity_Mqtt_Cmd_Sub_Delete_Respone(const char *msgId, int res);

//指令应答-本地群组控制
void Entity_Mqtt_Cmd_Local_Group_Set_Respone(unsigned short shortId, cJSON *property, const char *msgId, int res);

//指令应答-本地一键执行
void Entity_Mqtt_Cmd_Local_Rule_Exec_Respone(int shortId, const char *msgId, int res);

//指令应答-唤醒设备
void Entity_Mqtt_Cmd_Wake_Up_Respone(const char *subPid, const char *subUuid, const char *msgId, int res);

//指令应答-通知设备加入声网通道
void Entity_Mqtt_Cmd_Agora_Join_Respone(const char *msgId, int res);

//指令应答-IPC云存储开通
void Entity_Mqtt_Cmd_Ipc_Cloud_Open_Respone(int result, const char *msgId, int res);

//指令应答-声网事件通知
void Entity_Mqtt_Cmd_Agora_Event_Respone(int eventType, const char *channelName, int channelUserNum, const char *uid, const char *userId, const char *msgId, int res);

//指令应答-通用透传指令
void Entity_Mqtt_Cmd_Common_Cmd_Respone(const char *respType, int uid, int format, int channel, int rate, int volume, int samples, const char *msgId, int res);

//指令应答-1.4.17 本地群组变化通知
void Entity_Mqtt_Cmd_Local_Group_Update_Respone(cJSON *data, const char *msgId, int res);

//指令应答-本地场景/一键执行变化通知
void Entity_Mqtt_Cmd_Local_Scene_Update_Respone(cJSON *data, const char *msgId, int res);

//指令应答-设备日志开关
void Entity_Mqtt_Cmd_Log_Switch_Respone(const char *msgId, int res);

//指令应答-设备重启
void Entity_Mqtt_Cmd_Reboot_Respone(const char *msgId, int res);

//指令应答-子设备替换
void Entity_Mqtt_Cmd_Sub_Replace_Respone(const char *pid, const char *uuid, const char *newUuid, const char *msgId, int res, const char *result);

//指令应答-设置设备配置
void Entity_Mqtt_Cmd_Config_Settings_Respone(const char* type, cJSON *settings, const char *msgId, int res, const char *result);

//指令应答-DP点绑定本地执行
void Entity_Mqtt_Cmd_Dp_Bind_Respone(cJSON *dpBinds, const char *subPid, const char *subUuid, const char *msgId, int res);

//指令应答-本地DP群组变化通知 应答
void Entity_Mqtt_Cmd_Local_Dp_Group_Update_Respone(uint16_t shortId, uint8_t operatorType, const char *msgId, int res, const char *result);

//指令应答-清除数据
void Entity_Mqtt_Cmd_Clean_Data_Respone(const char *msgId, int res);
