//factory_dynamic_register.c
#include "factory_dynamic_register.h"


#include "entity_log.h"
#include "entity_wifi.h"
#include "entity_iot_func.h"
#include "entity_dev_info.h"
#include "entity_mqtt_client.h"
#include "entity_mqtt_event_report.h"
#include "entity_msg_queue.h"
#include "entity_config_net.h"
#include "entity_http_ota.h"
#include "entity_mqtt_v2_define.h"
#include "entity_error_code.h"

#include "com_utils.h"
#include "com_mbedtls.h"
#include "cJSON.h"

#include <sys/time.h>
#include <string.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include "factory_test.h"

#define REGISTER_MQTT_CMD_TIMEOUT_MS        10000
#define REGISTER_MQTT_YIELD_TIMEOUT_MS      250
#define REGISTER_MQTT_KEEPALIVE_S           60

#define REGISTER_MQTT_TASK_PRIO             3
#define REGISTER_MQTT_TASK_STACK            8192


static Entity_Mqtt_Context_t Register_Client_Instance = {0};
static char Register_Mqtt_Host[ENTITY_MQTT_HOST_MAX_LEN];
static unsigned short Register_Mqtt_Port;


static void * Register_Client_Thread_Id;
static uint8_t Flag_Entity_Mqtt_Register_Task_Runing = 0;  //任务�?��运�??��?
static uint8_t Flag_Entity_Mqtt_Register_Task_Over = 0;    //任务已�??��?�?
static Register_Mqtt_Msg_Cbs_t Register_Mqtt_Msg_Cbs;


void Register_Mqtt_Event_Device_Info_Report(void);

/**
*@?�称 		Get_Register_Mqtt_Msg_Cbs
*@?�能 		?��??�态注?�MQTT消息处�??��?
*@?�数 		void
*@返�???	Register_Mqtt_Msg_Cbs_t *
*@使用说�?	
*/
Register_Mqtt_Msg_Cbs_t *Get_Register_Mqtt_Msg_Cbs(void)
{
    return &Register_Mqtt_Msg_Cbs;
}

/**
*@?�称 		Register_Mqtt_Generate_Client_Config
*@?�能 		?��?注�?客户端�?置信??*@?�数 		Entity_Mqtt_Auth_t *mqtt_auth
*@返�???	void
*@使用说�?	
*/
static void Register_Mqtt_Generate_Client_Config(Entity_Mqtt_Auth_t *mqtt_auth)
{
    //?��?设�?信息
    Entity_Device_Info_t *dev_info = Entity_Get_Dev_Info();
    char *device_id_str = Get_Device_Id_String();
    unsigned char hmac_sha1[20] = {0};
    snprintf(mqtt_auth->Client_Id, sizeof(mqtt_auth->Client_Id)-1, "register_%s_%s", dev_info->Pid, device_id_str);
    snprintf(mqtt_auth->Username, sizeof(mqtt_auth->Username)-1,"%s|signMethod=hmacSha1,ts=0", dev_info->Pid);
    snprintf(mqtt_auth->Password, sizeof(mqtt_auth->Password)-1,"uuid=%s,ts=0", dev_info->Pid);
    char password_mask[32];
    ENTITY_LOGI("client_id: %s\n", mqtt_auth->Client_Id);
    ENTITY_LOGI("username: %s\n", mqtt_auth->Username);
    Utils_Mask_Secret(mqtt_auth->Password, password_mask, sizeof(password_mask));
    ENTITY_LOGI("password_mask: %s\n", password_mask);
    Mbedtls_Hmac(MBEDTLS_MD_SHA1, (unsigned char *)dev_info->Product_Secret, \
        strlen(dev_info->Product_Secret), (unsigned char *)mqtt_auth->Password, strlen(mqtt_auth->Password), hmac_sha1);
    Hex_Array_To_String(hmac_sha1, sizeof(hmac_sha1), mqtt_auth->Password);
    Utils_Mask_Secret(mqtt_auth->Password, password_mask, sizeof(password_mask));
    ENTITY_LOGI("hmac_sha1 password_mask: %s\r\n", password_mask);
}


/**
*@?�称 		Register_Mqtt_Connected_Callback
*@?�能 		?�态注?�MQTT客户端�??��??��?�?*@?�数 		Entity_Mqtt_Context_t* context, void* user_data
*@返�???	void
*@使用说�?	客户端�?上MQTT且订?�全?��??��?进此?��?
*/
static void Register_Mqtt_Connected_Callback(Entity_Mqtt_Context_t* context, void* user_data)
{
    Register_Mqtt_Event_Device_Info_Report();
}

/**
*@?�称 		Register_Mqtt_Disconnected_Callback
*@?�能 		?�态注?�MQTT客户端断开连接?��?
*@?�数 		Entity_Mqtt_Context_t* context, void* user_data
*@返�???	void
*@使用说�?	
*/
static void Register_Mqtt_Disconnected_Callback(Entity_Mqtt_Context_t* context, void* user_data)
{
    ENTITY_LOGI("----------now mqtt client disconnected-----------\r\n");
    
}


/**
*@?�称 		Register_Mqtt_Recv_Message_Callback
*@?�能 		?�态注?�MQTT客户端接?�到消息?��?
*@?�数 		Entity_Mqtt_Context_t* context, void* user_data, const void* vmsg
*@返�???	void
*@使用说�?	
*/
static void Register_Mqtt_Recv_Message_Callback(Entity_Mqtt_Context_t* context, void* user_data, const void* vmsg)
{
    Entity_Mqtt_Message_t *entity_msg = (Entity_Mqtt_Message_t *)vmsg;
    if (memcmp(entity_msg->Topic, Entity_Mqtt_Get_Topic(TOPIC_TYPE_EVENT_SUBSCRIBE), strlen(entity_msg->Topic)) == 0)
    {
        Entity_App_Msg_Queue_Send(ENTITY_MSG_TYPE_REGISTER_MQTT_EVENT, (const void*)entity_msg->Payload, entity_msg->Len);
    }
    else
    {
        char buf[100]={0};
        snprintf(buf, sizeof(buf), "%s", entity_msg->Topic);
        ENTITY_LOGE("%s !!!!!!!!not support topic:%s\n", __func__, buf);
    }
}


/**
*@?�称 		Register_Mqtt_Client_Task
*@?�能 		?�态注?�MQTT客户端任??*@?�数 		void *arg
*@返�???	void
*@使用说�?	
*/
void Register_Mqtt_Client_Task(void *arg)
{
    int ret = OPRT_OK;
    Flag_Entity_Mqtt_Register_Task_Over = 0;
    Entity_Mqtt_Context_t *entity_context = (Entity_Mqtt_Context_t *)&Register_Client_Instance;
    memset(entity_context, 0, sizeof(Entity_Mqtt_Context_t));
    
    Entity_Device_Info_t *dev_info = Entity_Get_Dev_Info();
    char *device_id_str = Get_Device_Id_String();
    Entity_Register_Mqtt_Topic_Init(dev_info->Pid, device_id_str);//?�态注?�TOPIC?��???    Entity_Config_Net_Info_t *config_net_info = Entity_Get_Config_Net_Info();
	Register_Mqtt_Generate_Client_Config(&entity_context->Mqtt_Auth);
	Entity_Mqtt_Config_t entity_config = {0};
	entity_config.Tcp_Connect_Params.Host = Register_Mqtt_Host;
	entity_config.Tcp_Connect_Params.Port = Register_Mqtt_Port;
	entity_config.Tcp_Connect_Params.Timeout_Ms = REGISTER_MQTT_CMD_TIMEOUT_MS;    //建�?连接?�接?��?个�??��??��??�时?��??��?网�?波动?�弱网环境�?超时?�间不能太短
	entity_config.Device_Pid = dev_info->Pid;
	entity_config.Device_Id = device_id_str;
	entity_config.Device_Secret = NULL;
	entity_config.Keepalive = REGISTER_MQTT_KEEPALIVE_S;
	entity_config.Connected_Cb = Register_Mqtt_Connected_Callback,
	entity_config.Disconnect_Cb = Register_Mqtt_Disconnected_Callback,
    entity_config.Recv_Messages_Cb = Register_Mqtt_Recv_Message_Callback,

	ret = Entity_Mqtt_Init(entity_context, &entity_config);
    if(ret != OPRT_OK){
        Flag_Entity_Mqtt_Register_Task_Over = 1;
        return;
    }
    while(1)
    {
        Entity_Mqtt_Loop(entity_context);
        if(entity_context->Prohibit_Connect && !entity_context->Is_Connected){ //已断连�?禁止?��?，�??�?�任??        
            ENTITY_LOGE("Register mqtt client thread exit\r\n");
            Flag_Entity_Mqtt_Register_Task_Over = 1;
            break;
        }
    }
    Entity_Pthread_Delete(&Register_Client_Thread_Id);
}


/**
*@?�称 		Register_Test_Mqtt_Client_Task_Start
*@?�能 		?�态注?�MQTT任务
*@?�数 		char *mqtt_host, unsigned short mqtt_port, Register_Mqtt_Msg_Cbs_t *cbs
*@返�???	int
*@使用说�?	
*/
int Register_Test_Mqtt_Client_Task_Start(char *mqtt_host, unsigned short mqtt_port, Register_Mqtt_Msg_Cbs_t *cbs)
{
    if(Flag_Entity_Mqtt_Register_Task_Runing == 0)
    {
        Register_Mqtt_Msg_Cbs = *cbs;
        strncpy(Register_Mqtt_Host, mqtt_host, sizeof(Register_Mqtt_Host)-1);
        Register_Mqtt_Port = mqtt_port;
        int ret = Entity_Pthread_Create(&Register_Client_Thread_Id, "Register_Test_Mqtt task", REGISTER_MQTT_TASK_STACK, REGISTER_MQTT_TASK_PRIO, Register_Mqtt_Client_Task, NULL);
        if(ret != 0) 
        {
            ENTITY_LOGE(" %s Failed!", __func__);
            return -1;
        }
        ENTITY_LOGD(" %s  success!", __func__);
        Flag_Entity_Mqtt_Register_Task_Runing = 1;
        return 0;
    }
    else
    {
        ENTITY_LOGE("%s have run!\r\n", __func__);
        return 0;
    }
}


/**
*@?�称 		Register_Test_Mqtt_Client_Task_Stop
*@?�能 		?�除?�态注?�MQTT任务
*@?�数 		void
*@返�???	int
*@使用说�?	
*/
int Register_Test_Mqtt_Client_Task_Stop(void)
{
    if(Flag_Entity_Mqtt_Register_Task_Runing)
    {
        Flag_Entity_Mqtt_Register_Task_Runing = 0;
        ENTITY_LOGI("%s\r\n", __func__);
        Entity_Mqtt_Context_t *entity_context = (Entity_Mqtt_Context_t *)&Register_Client_Instance;
        Entity_Mqtt_Manu_Disconnect(entity_context);
        while(!Flag_Entity_Mqtt_Register_Task_Over)//等�?任务结�?
            Entity_Sleep_Ms(10);
        Entity_Mqtt_Deinit(entity_context);
        ENTITY_LOGI("%s success", __func__);
    }
    return 0;
} 


/**
*@?�称 		Register_Mqtt_Topic_Subscribe
*@?�能 		?�态注?�订?�主�?*@?�数 		Topic_Type_e type_e, int qos
*@返�???	int                
*@使用说�?	
*/
int Register_Mqtt_Topic_Subscribe(Topic_Type_e type_e, int qos)
{
    return Entity_Mqtt_Topic_Subscribe(&Register_Client_Instance, type_e, qos);
}


/**
*@?�称 		Register_Mqtt_Topic_Publish
*@?�能 		?�态注?��?布主�?*@?�数 		cJSON *root, 
*@?�数 		Mqtt_Qos_Type_e qos, ?�务质�?级别
*@?�数 		int retain ?�否?��??��???*@返�???	void
*@使用说�?	将cjson转�??�模式�?符串?��?�?*/
void Register_Mqtt_Topic_Publish(cJSON *root, Mqtt_Qos_Type_e qos, int retain)
{
    char *json = cJSON_PrintUnformatted(root);
    if (json)
    {
        ENTITY_LOGD("event publish json:%s", json);
        Entity_Mqtt_Topic_Publish(&Register_Client_Instance, TOPIC_TYPE_EVENT_PUBLISH, json, strlen(json), qos, retain);
        cJSON_free(json);
    }
}

/**
*@?�称 		Register_Mqtt_Event_Device_Info_Report
*@?�能 		?�态注?�请求�??��?
*@?�数      void
*@返�???	void
*@使用说�?	
*/
void Register_Mqtt_Event_Device_Info_Report(void)
{
    cJSON *root = cJSON_CreateObject();
    char *msgId = Random_MsgId();
    Custom_Assert_Check_RequireString_Goto((root && msgId), quit, "cJSON_CreateObject error");
    unsigned int timestamp = Entity_Get_Time_Stamp();
    cJSON_AddStringToObject(root, "id", msgId);
    cJSON_AddNumberToObject(root, "ts", timestamp);
    cJSON_AddStringToObject(root, "code", "register");
    cJSON_AddNumberToObject(root, "ack", 1);
    Register_Mqtt_Topic_Publish(root, QOS1_LEAST_ONCE, 0);
quit:
    FREE_MEMORY(msgId);
    cJSON_Delete(root);
}

/**
*@?�称 		Register_Mqtt_Msg_Event_Respone_Parse_Process
*@?�能 		mqtt事件应�?消息处�?
*@?�数 		char *msg
*@返�???	void
*@使用说�?	设�?上报事件?��?云端?��??�数?��???*/
void Register_Mqtt_Msg_Event_Respone_Parse_Process(char *msg)
{
    Entity_Log_Long_String(ENTITY_LOG_LEVEL_DEBUG, "cloud--->device respone mqtt event", msg, strlen(msg));
    cJSON *root = cJSON_Parse(msg);
    if (!root)
    {
        ENTITY_LOGE("%s cJSON_CreateObject error", __func__);
        return;
    }
    
    int resCode = cJSON_GetObjectItem(root, "res")->valueint;
    if (resCode != 0)
    {
        ENTITY_LOGE("resCode :%d", resCode);
        goto quit;
    }
    cJSON *data_json = cJSON_GetObjectItem(root, "data");
    cJSON *cjson_id = cJSON_GetObjectItem(root, "id");
    if(!data_json || !cjson_id)
    {
        ENTITY_LOGE("%s no data\r\n", __func__);
        goto quit;
    }
    
    char *code = cJSON_GetObjectItem(root, "code")->valuestring;
    ENTITY_LOGD("code:%s\r\n", code);
    if(strcmp(code, "register") == 0)
    {
        cJSON *pid_json = cJSON_GetObjectItem(data_json, "pid");
        cJSON *uuid_json = cJSON_GetObjectItem(data_json, "uuid");
        cJSON *secret_json = cJSON_GetObjectItem(data_json, "secret");
        cJSON *mac_json = cJSON_GetObjectItem(data_json, "mac");
        if(!pid_json || !uuid_json || !secret_json || !mac_json)
        {
            ENTITY_LOGE("%s data error\r\n", __func__);
            goto quit;
        }

        Register_Mqtt_Msg_Cbs_t *cbs = Get_Register_Mqtt_Msg_Cbs();
        if(cbs->Set_Burn_Info_Callback)
        {
            Factory_Test_Burn_Info_t info=
            {
                .Uuid = uuid_json->valuestring,
                .Secret = secret_json->valuestring,
                .Mac = mac_json->valuestring,
                .Pid = pid_json->valuestring,
            };
            cbs->Set_Burn_Info_Callback(cjson_id->valuestring, &info);
        }
    }

quit:
    cJSON_Delete(root);
}












