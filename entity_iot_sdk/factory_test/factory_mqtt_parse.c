//factory_mqtt_parse.c
#include "factory_mqtt_parse.h"

#include "factory_mqtt_client.h"
#include "factory_test.h"

#include "entity_log.h"
#include "entity_wifi.h"
#include "entity_iot_func.h"
#include "entity_dev_info.h"
#include "entity_mqtt_event_report.h"
#include "entity_msg_queue.h"
#include "entity_config_net.h"
#include "entity_http_ota.h"
#include "entity_mqtt_v2_define.h"
#include "entity_param_check.h"

#include "com_utils.h"
#include "com_mbedtls.h"
#include "cJSON.h"

#include <sys/time.h>
#include <string.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>


//命令码列表
const char *Factory_Mqtt_Cmd_Code_List[] = {
    FACTORY_COMMAND_TEST_SETTINGS_CODE,
    FACTORY_COMMAND_TEST_TESTING_CODE,
    FACTORY_COMMAND_TEST_TESTED_CODE,
    FACTORY_COMMAND_TEST_BURN_CODE,
    FACTORY_COMMAND_TEST_ERASE_CODE,
    FACTORY_COMMAND_TEST_PROPERTY_SET_CODE,

};

//命令码对应序号，与command_mqtt_code_list保持一致
typedef enum
{
    FACTORY_COMMAND_TEST_SETTINGS_CODE_INDEX,
    FACTORY_COMMAND_TEST_TESTING_CODE_INDEX,
    FACTORY_COMMAND_TEST_TESTED_CODE_INDEX,
    FACTORY_COMMAND_TEST_BURN_CODE_INDEX,
    FACTORY_COMMAND_TEST_ERASE_CODE_INDEX,
    FACTORY_COMMAND_TEST_PROPERTY_SET_CODE_INDEX,
    FACTORY_COMMAND_TEST_CODE_INDEX_NUM,
}Factory_Cmd_Code_Index_e;


void Factory_Mqtt_Cmd_Respone_Load_Base(cJSON *root,const char *msgId, int res, const char *code);
static void Factory_Mqtt_Cmd_Publish(cJSON *root, Mqtt_Qos_Type_e qos, int retain);

/**
*@名称 		Factory_Mqtt_Cmd_Test_Settings_Respone
*@功能 		厂测设置测试参数的命令应答
*@参数 		const char *msgId, int authStatus
*@返回值 	void
*@使用说明	
*/
void Factory_Mqtt_Cmd_Test_Settings_Respone(const char *msgId, int authStatus)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    Custom_Assert_Check_RequireString_Goto((root && data && msgId), quit, "cJSON_CreateObject error");
    Factory_Mqtt_Cmd_Respone_Load_Base(root, msgId, 0, FACTORY_COMMAND_TEST_SETTINGS_CODE);
    Entity_Device_Info_t *dev_info = Entity_Get_Dev_Info();
    cJSON_AddStringToObject(data, "version", dev_info->Dev_Version);
    cJSON_AddStringToObject(data, "mcuVersion", dev_info->Sub_Version);
    cJSON_AddNumberToObject(data, "authStatus", authStatus);
    unsigned char wifi_mac[6]={0};
	Entity_Get_Wifi_Mac(wifi_mac);
    char wifi_mac_str[32] = {0};
    Mac_Addr_Format_String(wifi_mac, wifi_mac_str);
    cJSON_AddStringToObject(data, "wifiMac", wifi_mac_str);

    cJSON_AddItemToObject(root, "data", data);
    Factory_Mqtt_Cmd_Publish(root, QOS1_LEAST_ONCE, 0);
quit:
    cJSON_Delete(root);
}


/**
*@名称 		Factory_Mqtt_Cmd_Test_Settings_Parse
*@功能 		厂测设置测试参数
*@参数 		cJSON *root
*@返回值 	void
*@使用说明	
*/
void Factory_Mqtt_Cmd_Test_Settings_Parse(cJSON *root)
{
    cJSON *data_json = cJSON_GetObjectItem(root, "data");
    cJSON *cjson_id = cJSON_GetObjectItem(root, "id");
    if(!data_json || !cjson_id)
    {
        ENTITY_LOGE("%s no data\r\n", __func__);
        return;
    }
    cJSON *wifiRssiThreshold = cJSON_GetObjectItem(data_json, "wifiRssiThreshold");
    cJSON *wifiTimeout = cJSON_GetObjectItem(data_json, "wifiTimeout");
    if(!wifiRssiThreshold || !wifiTimeout)
    {
        ENTITY_LOGE("%s no item\r\n", __func__);
        return;
    }
    Factory_Mqtt_Msg_Cbs_t *cbs = Get_Factory_Mqtt_Msg_Cbs();
    if(cbs->Set_Test_Info_Callback)
    {
        Factory_Test_Set_Info_t info=
        {
            .Wifi_Rssi_Threshold = wifiRssiThreshold->valueint,
            .Wifi_Scan_Timeout = wifiTimeout->valueint,
        };
        cbs->Set_Test_Info_Callback(cjson_id->valuestring, &info);//获取厂测设置测试参数的回调处理
    }
    
}

/**
*@名称 		Factory_Mqtt_Cmd_Test_Testing_Parse
*@功能 		开始自动测试
*@参数 		cJSON *root
*@返回值 	void
*@使用说明	
*/
void Factory_Mqtt_Cmd_Test_Testing_Parse(cJSON *root)
{
    cJSON *cjson_id = cJSON_GetObjectItem(root, "id");
    Factory_Mqtt_Msg_Cbs_t *cbs = Get_Factory_Mqtt_Msg_Cbs();
    if(cbs->Test_Start_Callback)
    {
        cbs->Test_Start_Callback(cjson_id && cjson_id->valuestring ? cjson_id->valuestring : NULL);
    }
}

/**
*@名称 		Factory_Mqtt_Cmd_Test_Property_Set_Parse
*@功能 		设置属性
*@参数 		cJSON *root
*@返回值 	void
*@使用说明	
*/
void Factory_Mqtt_Cmd_Test_Property_Set_Parse(cJSON *root)
{
    cJSON *cjson_id = cJSON_GetObjectItem(root, "id");
    cJSON *data_item = cJSON_GetObjectItem(root, "data");
    if(!data_item || !cjson_id)
    {
        ENTITY_LOGE("%s no data\r\n", __func__);
        return;
    }
    cJSON *properties_item = cJSON_GetObjectItem(data_item, "properties");
    if (!properties_item) 
    {
        ENTITY_LOGE("%s-%d no properties_item\r\n", __func__, __LINE__);
        return;
    }
    
    // 解析manual_testing
    cJSON *manual_testing_item = cJSON_GetObjectItem(properties_item, "manual_testing");
    if (!cJSON_IsString(manual_testing_item) || (!manual_testing_item->valuestring)) 
    {
        ENTITY_LOGE("%s-%d no manual_testing_item\r\n", __func__, __LINE__);
        return;
    }
      
    Factory_Mqtt_Msg_Cbs_t *cbs = Get_Factory_Mqtt_Msg_Cbs();
    if(cbs->Manual_Test_Callback)
    {
        cbs->Manual_Test_Callback(manual_testing_item->valuestring);
    }
}

/**
*@名称 		Factory_Mqtt_Cmd_Test_Burn_Respone
*@功能 		厂测烧录三元组相关的命令应答
*@参数 		const char *msgId, int result, Factory_Test_Burn_Info_t* info
*@返回值 	void
*@使用说明	
*/
void Factory_Mqtt_Cmd_Test_Burn_Respone(const char *msgId, int result, Factory_Test_Burn_Info_t* info)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    Custom_Assert_Check_RequireString_Goto((root && data && msgId), quit, "cJSON_CreateObject error");
    Factory_Mqtt_Cmd_Respone_Load_Base(root, msgId, 0, FACTORY_COMMAND_TEST_BURN_CODE);
    if(0 == result)
    {
        cJSON_AddStringToObject(data, "result", "success");
        cJSON_AddStringToObject(data, "msg", "success");
        cJSON_AddStringToObject(data, "uuid", info->Uuid);
        cJSON_AddStringToObject(data, "secret", info->Secret);
        cJSON_AddStringToObject(data, "mac", info->Mac);
        cJSON_AddStringToObject(data, "pid", info->Pid);
    }
    else
    {
        cJSON_AddStringToObject(data, "result", "fail");
        cJSON_AddStringToObject(data, "msg", "fail");
    }

    cJSON_AddItemToObject(root, "data", data);
    Factory_Mqtt_Cmd_Publish(root, QOS1_LEAST_ONCE, 0);
quit:
    cJSON_Delete(root);
}

/**
*@名称 		Factory_Mqtt_Cmd_Test_Burn_Parse
*@功能 		烧录三元组相关
*@参数 		cJSON *root
*@返回值 	void
*@使用说明	
*/
void Factory_Mqtt_Cmd_Test_Burn_Parse(cJSON *root)
{
    cJSON *data_json = cJSON_GetObjectItem(root, "data");
    cJSON *cjson_id = cJSON_GetObjectItem(root, "id");
    if(!data_json || !cjson_id)
    {
        ENTITY_LOGE("%s no id\r\n", __func__);
        return;
    }
    cJSON *uuid_json = cJSON_GetObjectItem(data_json, "uuid");
    POINTER_SAFETY_CHECK_RETURN_NONE(uuid_json);
    cJSON *secret_json = cJSON_GetObjectItem(data_json, "secret");
    POINTER_SAFETY_CHECK_RETURN_NONE(secret_json);
    cJSON *mac_json = cJSON_GetObjectItem(data_json, "mac");
    POINTER_SAFETY_CHECK_RETURN_NONE(mac_json);
    cJSON *pid_json = cJSON_GetObjectItem(data_json, "pid");
    POINTER_SAFETY_CHECK_RETURN_NONE(pid_json);
    //apsid 和 appwd 是可选项
    cJSON *apsid_json = cJSON_GetObjectItem(data_json, "apsid");
    cJSON *appwd_json = cJSON_GetObjectItem(data_json, "appwd");

    Factory_Mqtt_Msg_Cbs_t *cbs = Get_Factory_Mqtt_Msg_Cbs();
    if(cbs->Set_Burn_Info_Callback)
    {
        Factory_Test_Burn_Info_t info=
        {
            .Uuid = uuid_json->valuestring,
            .Secret = secret_json->valuestring,
            .Mac = mac_json->valuestring,
            .Pid = pid_json->valuestring,
            .Ap_Ssid = (apsid_json && apsid_json->valuestring) ? apsid_json->valuestring : "",
            .Ap_Pwd = (appwd_json && appwd_json->valuestring) ? appwd_json->valuestring : "",
            .Is_Mqtt_Data = 1,
        };
        cbs->Set_Burn_Info_Callback(cjson_id->valuestring, &info);
    }
}

/**
*@名称 		Factory_Mqtt_Msg_Cmd_parse_Process
*@功能 		厂测mqtt命令消息处理
*@参数 		char *msg
*@返回值 	void
*@使用说明	
*/
void Factory_Mqtt_Msg_Cmd_parse_Process(char *msg)
{
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
        return;
    }
    //Entity_Log_Long_String(ENTITY_LOG_LEVEL_DEBUG, "cloud--->device mqtt cmd", msg, strlen(msg));
    ENTITY_LOGD("factory test cloud--->device mqtt cmd msg len=%d, %s\r\n", strlen(msg), msg);
   
    char *code = cJSON_GetObjectItem(root, "code")->valuestring;
    unsigned char cmd_code = Array_Index_Of(code, Factory_Mqtt_Cmd_Code_List, FACTORY_COMMAND_TEST_CODE_INDEX_NUM);
        
    switch (cmd_code)
    {
        case FACTORY_COMMAND_TEST_SETTINGS_CODE_INDEX://厂测设置测试参数
            Factory_Mqtt_Cmd_Test_Settings_Parse(root);
            break;
        case FACTORY_COMMAND_TEST_TESTING_CODE_INDEX: //开始厂测
            Factory_Mqtt_Cmd_Test_Testing_Parse(root);
            break;
        case FACTORY_COMMAND_TEST_PROPERTY_SET_CODE_INDEX:
            Factory_Mqtt_Cmd_Test_Property_Set_Parse(root);
            break;
        case FACTORY_COMMAND_TEST_TESTED_CODE_INDEX:
            break;
        case FACTORY_COMMAND_TEST_BURN_CODE_INDEX: //烧录三元组相关
            Factory_Mqtt_Cmd_Test_Burn_Parse(root);
            break;
        case FACTORY_COMMAND_TEST_ERASE_CODE_INDEX: 
            break;
        default:
            ENTITY_LOGD("not find command");
            break;
    }
    cJSON_Delete(root);
}



/**
*@名称 		Factory_Mqtt_Msg_Event_Respone_Parse_Process
*@功能 		mqtt事件应答消息处理
*@参数 		char *msg
*@返回值 	void
*@使用说明	设备上报事件后，云端回复的数据处理
*/
void Factory_Mqtt_Msg_Event_Respone_Parse_Process(char *msg)
{
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
        return;
    }
    Entity_Log_Long_String(ENTITY_LOG_LEVEL_DEBUG, "cloud--->device respone mqtt event", msg, strlen(msg));
    //ENTITY_LOGD("cloud--->device respone mqtt event msg len=%d, %s", strlen(msg), msg);

    int resCode = cJSON_GetObjectItem(root, "res")->valueint;
    if (resCode != 0)
    {
        ENTITY_LOGE("resCode :%d", resCode);
        return;
    }
    
    char *code = cJSON_GetObjectItem(root, "code")->valuestring;
    unsigned char cmd_code = Array_Index_Of(code, Factory_Mqtt_Cmd_Code_List, FACTORY_COMMAND_TEST_CODE_INDEX_NUM);
    ENTITY_LOGD("cmd_code:%d\r\n",cmd_code);
    switch (cmd_code)
    {
        case FACTORY_COMMAND_TEST_SETTINGS_CODE_INDEX://厂测设置测试参数
            
            break;
        case FACTORY_COMMAND_TEST_TESTING_CODE_INDEX: //开始厂测
            
            break;
        case FACTORY_COMMAND_TEST_TESTED_CODE_INDEX:
            break;
        case FACTORY_COMMAND_TEST_BURN_CODE_INDEX: //烧录三元组相关
            
            break;
        case FACTORY_COMMAND_TEST_ERASE_CODE_INDEX: 
            break;
        
        default:
            break;
    }
    cJSON_Delete(root);
}



/**
*@名称 		Factory_Mqtt_Cmd_Publish
*@功能 		命令发布
*@参数 		cJSON *root, 
*@参数 		Mqtt_Qos_Type_e qos, 
*@参数 		int retain
*@返回值 	void
*@使用说明	将cjson转成无模式字符串后发布
*/
static void Factory_Mqtt_Cmd_Publish(cJSON *root, Mqtt_Qos_Type_e qos, int retain)
{
    char *json = cJSON_PrintUnformatted(root);
    if (json)
    {
        ENTITY_LOGD("cmd publish json:%s", json);
        Factory_Mqtt_Topic_Publish(TOPIC_TYPE_CMD_PUBLISH, json, strlen(json), qos, retain);
        cJSON_free(json);
    }
}

/**
*@名称 		Factory_Mqtt_Cmd_Respone_Load_Base
*@功能 		加载命令应答基础内容
*@参数 		cJSON *root, 
*@参数 		const char *msgId
*@参数 		int res
*@参数 		const char *code
*@返回值 	void
*@使用说明	将cjson转成无模式字符串后发布
*/
void Factory_Mqtt_Cmd_Respone_Load_Base(cJSON *root,const char *msgId, int res, const char *code)
{
    unsigned int timestamp = Entity_Get_Time_Stamp();
    cJSON_AddNumberToObject(root, "res", res);
    cJSON_AddStringToObject(root, "id", msgId);
    cJSON_AddNumberToObject(root, "ts", timestamp);
    cJSON_AddStringToObject(root, "code", code);
}


/**
*@名称 		Factory_Mqtt_Event_Publish
*@功能 		事件发布
*@参数 		cJSON *root, 
*@参数 		Mqtt_Qos_Type_e qos, 服务质量级别
*@参数 		int retain 是否是保留消息
*@返回值 	void
*@使用说明	将cjson转成无模式字符串后发布
*/
void Factory_Mqtt_Event_Publish(cJSON *root, Mqtt_Qos_Type_e qos, int retain)
{
    char *json = cJSON_PrintUnformatted(root);
    if (json)
    {
        ENTITY_LOGD("event publish json:%s", json);
        Factory_Mqtt_Topic_Publish(TOPIC_TYPE_EVENT_PUBLISH, json, strlen(json), qos, retain);
        cJSON_free(json);
    }
}

/**
*@名称 		Factory_Mqtt_Event_Report_Load_Base
*@功能 		加载事件上报基础内容
*@参数 		cJSON *root, 
*@参数 		const char *msgId
*@参数 		int res
*@参数 		const char *code
*@返回值 	void
*@使用说明	
*/
void Factory_Mqtt_Event_Report_Load_Base(cJSON *root,const char *msgId, int ack, const char *code)
{
    unsigned int timestamp = Entity_Get_Time_Stamp();
    cJSON_AddStringToObject(root, "id", msgId);
    cJSON_AddNumberToObject(root, "ts", timestamp);
    cJSON_AddStringToObject(root, "code", code);
    cJSON_AddNumberToObject(root, "ack", ack);
}


/**
*@名称 		Factory_Mqtt_Event_Device_Info_Report
*@功能 		厂测设备信息上报
*@参数      void
*@返回值 	void
*@使用说明	
*/
void Factory_Mqtt_Event_Device_Info_Report(void)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    char *msgId = Random_MsgId();
    Custom_Assert_Check_RequireString_Goto((root && data && msgId), quit, "cJSON_CreateObject error");
    Factory_Mqtt_Event_Report_Load_Base(root, msgId, 0, ENTITY_EVENT_INFO_CODE);
    Entity_Device_Info_t *dev_info = Entity_Get_Dev_Info();
    cJSON_AddNumberToObject(data, "bindStatus", 0);
    cJSON_AddStringToObject(data, "version", dev_info->Dev_Version);
    cJSON_AddStringToObject(data, "mcuVersion", dev_info->Sub_Version);
    unsigned char wifi_mac[6]={0};
	Entity_Get_Wifi_Mac(wifi_mac);
    char wifi_mac_str[32] = {0};
    Mac_Addr_Format_String(wifi_mac, wifi_mac_str);
    cJSON_AddStringToObject(root, "wifiMac", wifi_mac_str);
    cJSON_AddItemToObject(root, "data", data);

    Factory_Mqtt_Event_Publish(root, QOS1_LEAST_ONCE, 0);
quit:
    FREE_MEMORY(msgId);
    cJSON_Delete(root);
}

/**
*@名称 		Factory_Mqtt_Test_Result_Report
*@功能 		厂测结果上报
*@参数      unsigned char step, unsigned char result, int rssi
*@返回值 	void
*@使用说明	
*/
void Factory_Mqtt_Test_Result_Report(unsigned char step, unsigned char result, int rssi)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    char *msgId = Random_MsgId();
    Custom_Assert_Check_RequireString_Goto((root && data && msgId), quit, "cJSON_CreateObject error");
    Factory_Mqtt_Event_Report_Load_Base(root, msgId, 0, FACTORY_COMMAND_TEST_TESTED_CODE);
    cJSON_AddNumberToObject(data, "setup", step);
    switch(result)
    {
    case FACTORY_TEST_RESULT_OK:
        cJSON_AddNumberToObject(data, "rssi", rssi);
        cJSON_AddStringToObject(data, "result", "success");
        cJSON_AddStringToObject(data, "msg", "success");
        break;
    case FACTORY_TEST_RESULT_WEAK_RSSI:
        cJSON_AddNumberToObject(data, "rssi", rssi);
        __attribute__((fallthrough));
    case FACTORY_TEST_RESULT_TIMEOUT:
    default:
        cJSON_AddStringToObject(data, "result", "fail");
        cJSON_AddStringToObject(data, "msg", "fail");
        break;
    }
    cJSON_AddItemToObject(root, "data", data);

    Factory_Mqtt_Event_Publish(root, QOS1_LEAST_ONCE, 0);
quit:
    FREE_MEMORY(msgId);
    cJSON_Delete(root);
}


/**
*@名称 		Factory_Mqtt_Test_Settings_Event_Report
*@功能 		厂测设置测试参数的事件上报
*@参数 		const char *msgId, int authStatus
*@返回值 	void
*@使用说明	
*/
void Factory_Mqtt_Test_Settings_Event_Report(int authStatus)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    char *msgId = Random_MsgId();
    Custom_Assert_Check_RequireString_Goto((root && data && msgId), quit, "cJSON_CreateObject error");
    Factory_Mqtt_Event_Report_Load_Base(root, msgId, 0, FACTORY_COMMAND_TEST_SETTINGS_CODE);
    Entity_Device_Info_t *dev_info = Entity_Get_Dev_Info();
    cJSON_AddStringToObject(data, "version", dev_info->Dev_Version);
    cJSON_AddStringToObject(data, "mcuVersion", dev_info->Sub_Version);
    cJSON_AddNumberToObject(data, "authStatus", authStatus);
    unsigned char wifi_mac[6]={0};
	Entity_Get_Wifi_Mac(wifi_mac);
    char wifi_mac_str[32] = {0};
    Mac_Addr_Format_String(wifi_mac, wifi_mac_str);
    cJSON_AddStringToObject(data, "wifiMac", wifi_mac_str);

    cJSON_AddItemToObject(root, "data", data);
    Factory_Mqtt_Event_Publish(root, QOS1_LEAST_ONCE, 0);
quit:
    cJSON_Delete(root);
}
/**
*@名称 		Factory_Mqtt_Event_Get_Time_Request
*@功能 		事件请求-获取云端时间
*@参数      unsigned char ack
*@返回值 	void
*@使用说明	
*/
void Factory_Mqtt_Event_Get_Time_Request(unsigned char ack)
{
    cJSON *root = cJSON_CreateObject();
    char *msgId = Random_MsgId();
    Custom_Assert_Check_RequireString_Goto((root && msgId), quit, "cJSON_CreateObject error");
    Factory_Mqtt_Event_Report_Load_Base(root, msgId, ack, ENTITY_EVENT_TIME_CODE);
    Factory_Mqtt_Event_Publish(root, QOS1_LEAST_ONCE, 0);
quit:
    FREE_MEMORY(msgId);
    cJSON_Delete(root);
}
