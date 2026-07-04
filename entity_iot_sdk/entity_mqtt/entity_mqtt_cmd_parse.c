//entity_mqtt_cmd_parse.c
#include "entity_mqtt_cmd_parse.h"

#include "entity_mqtt_v2_define.h"
#include "entity_mqtt_cmd_respone.h"
#include "entity_mqtt_app.h"

#include "entity_log.h"
#include "entity_wifi.h"
#include "entity_iot_func.h"
#include "entity_dev_info.h"
#include "entity_http_dev_ota.h"

#include "com_utils.h"
#include "cJSON.h"

#include <sys/time.h>
#include <string.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>


static Entity_Mqtt_Cmd_Parse_Cbs_t Entity_Mqtt_Cmd_Parse_Cbs;



//命令码列表
const char *Mqtt_Cmd_Code_List[] = {
    ENTITY_COMMAND_RESET_CODE,
    ENTITY_COMMAND_OTA_CODE,
    ENTITY_COMMAND_PING_CODE,
    ENTITY_COMMAND_PROPERTY_SET_CODE,
    ENTITY_COMMAND_UPDATE_LOCAL_CONFIG_CODE,
    ENTITY_COMMAND_FIND_BIND_CODE,
    ENTITY_COMMAND_SUB_PROPERTY_SET_CODE,
    ENTITY_COMMAND_SUB_DELETE_CODE,
    ENTITY_COMMAND_LOCAL_GROUP_SET_CODE,
    ENTITY_COMMAND_LOCAL_RULE_EXEC_CODE,
    ENTITY_COMMAND_WAKE_UP_CODE,
    ENTITY_COMMAND_AGORA_JOIN_CODE,
    ENTITY_COMMAND_IPC_CLOUD_OPEN_CODE,
    ENTITY_COMMAND_AGORA_EVENT_CODE,
    ENTITY_COMMAND_COMMON_CMD_CODE,
    ENTITY_COMMAND_LOCAL_GROUP_UPDATE_CODE,
    ENTITY_COMMAND_LOCAL_SCENE_UPDATE_CODE,
    ENTITY_COMMAND_LOG_SWITCH_CODE,
    ENTITY_COMMAND_REBOOT_CODE,
    ENTITY_COMMAND_SUB_REPLACE_CODE,
    ENTITY_COMMAND_DP_BIND_CODE,      
    ENTITY_COMMAND_LOCAL_DP_GROUP_UPDATE,
    ENTITY_COMMAND_CONFIG_SETTINGS,   
    ENTITY_COMMAND_CLEAN_DATA_CODE,
};

//命令码对应序号，与command_mqtt_code_list保持一致
typedef enum
{
    ENTITY_COMMAND_RESET_CODE_INDEX,
    ENTITY_COMMAND_OTA_CODE_INDEX,
    ENTITY_COMMAND_PING_CODE_INDEX,
    ENTITY_COMMAND_PROPERTY_SET_CODE_INDEX,
    ENTITY_COMMAND_UPDATE_LOCAL_CONFIG_CODE_INDEX,
    ENTITY_COMMAND_FIND_BIND_CODE_INDEX,
    ENTITY_COMMAND_SUB_PROPERTY_SET_CODE_INDEX,
    ENTITY_COMMAND_SUB_DELETE_CODE_INDEX,
    ENTITY_COMMAND_LOCAL_GROUP_SET_CODE_INDEX,
    ENTITY_COMMAND_LOCAL_RULE_EXEC_CODE_INDEX,
    ENTITY_COMMAND_WAKE_UP_CODE_INDEX,
    ENTITY_COMMAND_AGORA_JOIN_CODE_INDEX,
    ENTITY_COMMAND_IPC_CLOUD_OPEN_CODE_INDEX,
    ENTITY_COMMAND_AGORA_EVENT_CODE_INDEX,
    ENTITY_COMMAND_COMMON_CMD_CODE_INDEX,
    ENTITY_COMMAND_LOCAL_GROUP_UPDATE_CODE_INDEX,
    ENTITY_COMMAND_LOCAL_SCENE_UPDATE_CODE_INDEX,
    ENTITY_COMMAND_LOG_SWITCH_CODE_INDEX,
    ENTITY_COMMAND_REBOOT_CODE_INDEX,
    ENTITY_COMMAND_SUB_REPLACE_CODE_INDEX,
    ENTITY_COMMAND_DP_BIND_CODE_INDEX,      
    ENTITY_COMMAND_LOCAL_DP_GROUP_UPDATE_INDEX,
    ENTITY_COMMAND_CONFIG_SETTINGS_INDEX,   
    ENTITY_COMMAND_CLEAN_DATA_CODE_INDEX,

    ENTITY_COMMAND_CODE_INDEX_NUM,
}Entity_Cmd_Code_Index_e;



/**
*@名称 		Entity_Mqtt_Cmd_Parse_Cbs_Init
*@功能 		mqtt命令消息处理回调注册
*@参数 		Entity_Mqtt_Cmd_Parse_Cbs_t *cbs
*@返回值 	void
*@使用说明	
*/
void Entity_Mqtt_Cmd_Parse_Cbs_Init(Entity_Mqtt_Cmd_Parse_Cbs_t *cbs)
{
    Entity_Mqtt_Cmd_Parse_Cbs = *cbs;
}


/**
*@名称 		Entity_Mqtt_Cmd_Reset_Parse
*@功能 		设备重置命令的处理
*@参数 		cJSON *root
*@返回值 	void
*@使用说明	
*/
void Entity_Mqtt_Cmd_Reset_Parse(cJSON *root)
{
    cJSON *data = cJSON_GetObjectItem(root, "data");
    if(!cJSON_IsObject(data))
        return;
    cJSON *cjson_ack = cJSON_GetObjectItem(root, "ack");
    cJSON *cjson_id  = cJSON_GetObjectItem(root, "id");
    if (!cJSON_IsNumber(cjson_ack) || !cJSON_IsString(cjson_id))
        return;
    unsigned need_ack = cjson_ack->valueint;
    if(need_ack)
        Entity_Mqtt_Cmd_Reset_Respone(cjson_id->valuestring, 0);

    cJSON *cjson_clear = cJSON_GetObjectItem(data, "clearData");
    if (!cjson_clear)
        return;
    unsigned char need_clear = cjson_clear->valueint;
    Entity_Reset_Config_Net_Info_To_Flash();//清除配网信息
    Entity_Reset_Thing_Model_To_Flash();//清除物模型信息
    if(Entity_Mqtt_Cmd_Parse_Cbs.Cmd_Reset_Parse_Cb)
        Entity_Mqtt_Cmd_Parse_Cbs.Cmd_Reset_Parse_Cb(need_clear);

}


/**
*@名称 		Entity_Mqtt_Cmd_Firmware_Ota_Parse
*@功能 		固件OTA命令的处理
*@参数 		cJSON *root
*@返回值 	void
*@使用说明	此处是IOT设备的OTA解析，如果是网关OTA则是另外处理
*/
void Entity_Mqtt_Cmd_Firmware_Ota_Parse(cJSON *root)
{
    int sub_device_ota=0;
    cJSON *cjson_ack = cJSON_GetObjectItem(root, "ack");
    cJSON *cjson_id = cJSON_GetObjectItem(root, "id");
    if(!cJSON_IsNumber(cjson_ack) || !cJSON_IsString(cjson_id))
        return;
    if (cjson_ack->valueint)
    {
        Entity_Mqtt_Cmd_Ota_Respone(cjson_id->valuestring, 0);
    }
    cJSON *data = cJSON_GetObjectItem(root, "data");
    if(!cJSON_IsObject(data))
        return;

    cJSON *cjson_url = cJSON_GetObjectItem(data, "url");
    if(!cJSON_IsString(cjson_url))
    {
        ENTITY_LOGE("url item error\r\n");
        return;
    }  
    char *firmware_url  = cjson_url->valuestring;
    ENTITY_LOGI("%s firmware_url:%s\r\n", __func__, firmware_url);

    cJSON *cjson_version = cJSON_GetObjectItem(data, "version");
    if(!cJSON_IsString(cjson_version))
    {
        ENTITY_LOGE("version item error\r\n");
        return;
    }  
    char *new_version = cjson_version->valuestring;
    ENTITY_LOGI("%s version:%s\r\n", __func__, new_version);

    cJSON *cjson_filesize = cJSON_GetObjectItem(data, "fileSize");
    if(!cJSON_IsNumber(cjson_filesize))
    {
        ENTITY_LOGE("fileSize item error\r\n");
        return;
    }  
    int filesize = cjson_filesize->valueint;
    ENTITY_LOGI("%s filesize:%d\r\n", __func__, filesize);

    cJSON *cjson_md5sum = cJSON_GetObjectItem(data, "md5sum");
    if(!cJSON_IsString(cjson_md5sum))
    {
        ENTITY_LOGE("md5sum item error\r\n");
        return;
    }  
    char *md5sum = cjson_md5sum->valuestring;
    ENTITY_LOGI("%s md5sum:%s\r\n", __func__, md5sum);

    cJSON *cjson_firmwareType = cJSON_GetObjectItem(data, "firmwareType");
    if(!cJSON_IsNumber(cjson_firmwareType))
    {
        ENTITY_LOGE("firmwareType item error\r\n");
        return;
    }  
    int firmwareType = cjson_firmwareType->valueint;
    ENTITY_LOGI("%s firmwareType:%d\r\n", __func__, firmwareType);

    cJSON *cjson_subUuids = cJSON_GetObjectItem(data, "subUuids");
    if(cjson_subUuids && cJSON_GetArraySize(cjson_subUuids))
    {
        sub_device_ota = 1;//子设备升级
        ENTITY_LOGI("is sub_device_ota\r\n");
    }
    else
    {
        sub_device_ota = 0;//自身相关升级
        ENTITY_LOGI("is self device ota\r\n");
    }
    if(!sub_device_ota)//自身相关升级
    {
        if(firmwareType == 2)//设备自身的升级
        {
            ENTITY_LOGI("%s, is ota self firmware...\r\n", __func__);
            Entity_Http_Ota_Param_t ota_param={
                .Mssage_Id = cjson_id->valuestring,
                .Url = firmware_url,
                .New_Version = new_version,
                .File_Size = filesize,
                .Md5_Sum = md5sum,
            };
            Entity_Dev_Ota_Start(&ota_param);
        }
        else if(firmwareType == 3)//与设备相关的模组(MCU)升级
        {
            ENTITY_LOGD("%s, is ota mcu or module firmware...\r\n", __func__);
        }
    }
    else
    {
        ENTITY_LOGI("%s, is ota subdevice firmware...\r\n", __func__);
    }

    if(Entity_Mqtt_Cmd_Parse_Cbs.Cmd_Ota_Parse_Cb)
        Entity_Mqtt_Cmd_Parse_Cbs.Cmd_Ota_Parse_Cb(firmware_url);

}

/**
*@名称 		Entity_Mqtt_Cmd_Clean_Data_Parse
*@功能 		固件清除数据命令的处理
*@参数 		cJSON *root
*@返回值 	void
*@使用说明	是指清除设备配网后产生的用户数据
*/
void Entity_Mqtt_Cmd_Clean_Data_Parse(cJSON *root)
{
    cJSON *cjson_ack = cJSON_GetObjectItem(root, "ack");
    cJSON *cjson_id = cJSON_GetObjectItem(root, "id");
    if(!cJSON_IsNumber(cjson_ack) || !cJSON_IsString(cjson_id))
        return;
    if (cjson_ack->valueint)
    {
        Entity_Mqtt_Cmd_Clean_Data_Respone(cjson_id->valuestring, 0);
    }
    if(Entity_Mqtt_Cmd_Parse_Cbs.Cmd_Clean_Data_Parse_Cb)
        Entity_Mqtt_Cmd_Parse_Cbs.Cmd_Clean_Data_Parse_Cb();
}

/**
*@名称 		Entity_Mqtt_Cmd_Ping_Parse
*@功能 		检测设备在线状态的处理
*@参数 		cJSON *root
*@返回值 	void
*@使用说明	设置设备DP点
*/
void Entity_Mqtt_Cmd_Ping_Parse(cJSON *root)
{
    cJSON *data = cJSON_CreateObject();
    if (data == NULL)
        return;
    uint8_t level=0, value=0;
    Entity_Wifi_Load_Signal_Level_Quality(&level, &value);
    cJSON_AddNumberToObject(data, "signal", level);
    cJSON_AddNumberToObject(data, "signalValue", value);
    cJSON *cjson_id = cJSON_GetObjectItem(root, "id");
    if (!cJSON_IsString(cjson_id))
    {
        cJSON_Delete(data);
        return;
    }
    Entity_Mqtt_Cmd_Ping_Respone(data, cjson_id->valuestring, 0);
}

/**
*@名称 		Entity_Mqtt_Cmd_Property_Set_Parse
*@功能 		设置属性命令的处理
*@参数 		cJSON *root
*@返回值 	void
*@使用说明	设置设备DP点
*/
void Entity_Mqtt_Cmd_Property_Set_Parse(cJSON *root)
{
    cJSON *data_json = cJSON_GetObjectItem(root, "data");
    if(!cJSON_IsObject(data_json))
    {
        ENTITY_LOGE("%s no data\r\n", __func__);
        return;
    }
        
    cJSON *properties_json = cJSON_GetObjectItem(data_json, "properties");
    if(!cJSON_IsObject(properties_json))
    {
        ENTITY_LOGE("%s no properties\r\n", __func__);
        return;  
    }
        
    cJSON *cjson_ack = cJSON_GetObjectItem(root, "ack");
    cJSON *cjson_id  = cJSON_GetObjectItem(root, "id");
    if (!cJSON_IsNumber(cjson_ack) || !cJSON_IsString(cjson_id))
        return;
    if(cjson_ack->valueint)
    {
        Entity_Mqtt_Cmd_Property_Set_Respone(properties_json, cjson_id->valuestring, 0);
    }
    Dp_Obj_Collect_t *Dp_Obj_Collect = Entity_Dev_Dp_Property_Parse(properties_json);
    if(!Dp_Obj_Collect)
        return;
    if(Entity_Mqtt_Cmd_Parse_Cbs.Cmd_Property_Set_Parse_Cb)
        Entity_Mqtt_Cmd_Parse_Cbs.Cmd_Property_Set_Parse_Cb(Dp_Obj_Collect);
    Entity_Dp_Obj_Collect_Mem_Free(Dp_Obj_Collect);
}

/**
*@名称 		Entity_Mqtt_Msg_Cmd_parse_Process
*@功能 		mqtt命令消息处理
*@参数 		char *msg
*@返回值 	void
*@使用说明	
*/
void Entity_Mqtt_Msg_Cmd_parse_Process(char *msg)
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
    //Entity_Log_Long_String(ENTITY_LOG_LEVEL_DEBUG, "cloud--->device mqtt cmd", msg, strlen(msg));
    ENTITY_LOGD("cloud--->device mqtt cmd msg len=%d, %s\r\n", strlen(msg), msg);
    //命令通用回调
    Entity_Mqtt_App_Msg_Reach_Callback_t Entity_Mqtt_App_Msg_Reach_Cbs = Get_Entity_Mqtt_App_Msg_Reach_Cbs();
    if(Entity_Mqtt_App_Msg_Reach_Cbs.Entity_Mqtt_Msg_Cmd_Cb)
        Entity_Mqtt_App_Msg_Reach_Cbs.Entity_Mqtt_Msg_Cmd_Cb();

    cJSON *cjson_code = cJSON_GetObjectItem(root, "code");
    if (!cJSON_IsString(cjson_code))
    {
        ENTITY_LOGE("%s code field missing", __func__);
        goto cleanup;
    }
    char *code = cjson_code->valuestring;
    unsigned char cmd_code = Array_Index_Of(code, Mqtt_Cmd_Code_List, ENTITY_COMMAND_CODE_INDEX_NUM);
        
    switch (cmd_code)
    {
        case ENTITY_COMMAND_RESET_CODE_INDEX://重置设备
            Entity_Mqtt_Cmd_Reset_Parse(root);
            break;
        case ENTITY_COMMAND_OTA_CODE_INDEX: //OTA升级
            Entity_Mqtt_Cmd_Firmware_Ota_Parse(root);
            break;
        case ENTITY_COMMAND_PING_CODE_INDEX:
            Entity_Mqtt_Cmd_Ping_Parse(root);
            break;
        case ENTITY_COMMAND_PROPERTY_SET_CODE_INDEX: //设置属性
            Entity_Mqtt_Cmd_Property_Set_Parse(root);
            break;
        case ENTITY_COMMAND_UPDATE_LOCAL_CONFIG_CODE_INDEX: 
            break;
        case ENTITY_COMMAND_FIND_BIND_CODE_INDEX:   //发现并绑定子设备   
            break;
        case ENTITY_COMMAND_SUB_PROPERTY_SET_CODE_INDEX: //设置子设备属性
            break;
        case ENTITY_COMMAND_SUB_DELETE_CODE_INDEX: //删除子设备
            break;
        case ENTITY_COMMAND_LOCAL_GROUP_SET_CODE_INDEX: //本地群组控制
            break;
        case ENTITY_COMMAND_LOCAL_RULE_EXEC_CODE_INDEX: //本地一键执行
            break;
        case ENTITY_COMMAND_WAKE_UP_CODE_INDEX: //唤醒设备
            break;
        case ENTITY_COMMAND_AGORA_JOIN_CODE_INDEX://通知设备加入声网通道
            break;
        case ENTITY_COMMAND_IPC_CLOUD_OPEN_CODE_INDEX://IPC云存储开通   
            break;
        case ENTITY_COMMAND_AGORA_EVENT_CODE_INDEX: //声网事件通知  
            break;
        case ENTITY_COMMAND_COMMON_CMD_CODE_INDEX:  //通用透传指令 
            if(Entity_Mqtt_Cmd_Parse_Cbs.command_common_cmd_callback) {
                Entity_Mqtt_Cmd_Parse_Cbs.command_common_cmd_callback(root);
            }
            break;
        case ENTITY_COMMAND_LOCAL_GROUP_UPDATE_CODE_INDEX: //本地群组变化通知
            break;
        case ENTITY_COMMAND_LOCAL_SCENE_UPDATE_CODE_INDEX: // 本地场景/一键执行变化通知
            break;
        case ENTITY_COMMAND_LOG_SWITCH_CODE_INDEX: // 设备日志开关
            break;
        case ENTITY_COMMAND_REBOOT_CODE_INDEX:  // 设备重启
            break;
        case ENTITY_COMMAND_SUB_REPLACE_CODE_INDEX: // 子设备替换
            break;
        case ENTITY_COMMAND_DP_BIND_CODE_INDEX:  // DP点绑定本地执行
            break;
        case ENTITY_COMMAND_LOCAL_DP_GROUP_UPDATE_INDEX: //本地DP群组变化通知 
            break;
        case ENTITY_COMMAND_CONFIG_SETTINGS_INDEX:  //设置设备配置
            break;
        case ENTITY_COMMAND_CLEAN_DATA_CODE_INDEX:  //清除数据
            Entity_Mqtt_Cmd_Clean_Data_Parse(root);
            break;    
        default:
            ENTITY_LOGD("not find command");
            break;
    }
cleanup:
    cJSON_Delete(root);
}

















