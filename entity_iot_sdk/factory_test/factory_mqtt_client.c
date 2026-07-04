//factory_mqtt_client.c
#include "factory_mqtt_client.h"

#include "factory_mqtt_parse.h"
#include "factory_test.h"

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

#define FACTORY_MQTT_CMD_TIMEOUT_MS        10000
#define FACTORY_MQTT_YIELD_TIMEOUT_MS      250
#define FACTORY_MQTT_KEEPALIVE_S           60

#define FACTORY_MQTT_TASK_PRIO             3
#define FACTORY_MQTT_TASK_STACK       8192


static Entity_Mqtt_Context_t Factory_Client_Instance = {0};
static char Factory_Mqtt_Host[ENTITY_MQTT_HOST_MAX_LEN];
static unsigned short Factory_Mqtt_Port;
static Factory_Mqtt_Msg_Cbs_t Factory_Mqtt_Msg_Cbs;

static void * Mqtt_Client_Thread_Id;
static uint8_t Flag_Entity_Mqtt_Factory_Task_Runing = 0;  //任务�?��运�??��?
static uint8_t Flag_Entity_Mqtt_Factory_Task_Over = 0;    //任务已�??��?�?



/**
*@?�称 		Get_Factory_Mqtt_Msg_Cbs
*@?�能 		?��??��?MQTT消息处�??��?
*@?�数 		void
*@返�???	Factory_Mqtt_Msg_Cbs_t *
*@使用说�?	
*/
Factory_Mqtt_Msg_Cbs_t *Get_Factory_Mqtt_Msg_Cbs(void)
{
    return &Factory_Mqtt_Msg_Cbs;
}


/**
*@?�称 		Factory_Mqtt_Generate_Client_Config
*@?�能 		?��??��?客户端�?置信??*@?�数 		Entity_Mqtt_Auth_t *mqtt_auth
*@返�???	void
*@使用说�?	
*/
static void Factory_Mqtt_Generate_Client_Config(Entity_Mqtt_Auth_t *mqtt_auth)
{
    //?��?设�?信息
    Entity_Device_Info_t *dev_info = Entity_Get_Dev_Info();
    char *device_id_str = Get_Device_Id_String();

    snprintf(mqtt_auth->Client_Id, sizeof(mqtt_auth->Client_Id)-1, "rlink_%s_%s", dev_info->Pid, device_id_str);
    snprintf(mqtt_auth->Username, sizeof(mqtt_auth->Username)-1, "%s", device_id_str);
    snprintf(mqtt_auth->Password, sizeof(mqtt_auth->Password)-1, "%s", FACTORY_TEST_MQTT_PASSWD);
    char password_mask[32];
    Utils_Mask_Secret(mqtt_auth->Password, password_mask, sizeof(password_mask));

    ENTITY_LOGI("client_id: %s\n", mqtt_auth->Client_Id);
    ENTITY_LOGI("username: %s\n", mqtt_auth->Username);
    ENTITY_LOGI("password_mask: %s\n", password_mask);
}


/**
*@?�称 		Factory_Mqtt_Connected_Callback
*@?�能 		MQTT客户端�??��??��?�?*@?�数 		Entity_Mqtt_Context_t* context, void* user_data
*@返�???	void
*@使用说�?	客户端�?上MQTT且订?�全?��??��?进此?��?
*/
static void Factory_Mqtt_Connected_Callback(Entity_Mqtt_Context_t* context, void* user_data)
{
    //Factory_Mqtt_Event_Get_Time_Request(1);
    Factory_Mqtt_Event_Device_Info_Report();
}

/**
*@?�称 		Factory_Mqtt_Disconnected_Callback
*@?�能 		MQTT客户端断开连接?��?
*@?�数 		Entity_Mqtt_Context_t* context, void* user_data
*@返�???	void
*@使用说�?	
*/
static void Factory_Mqtt_Disconnected_Callback(Entity_Mqtt_Context_t* context, void* user_data)
{
    ENTITY_LOGI("----------now mqtt client disconnected-----------\r\n");
    if(Factory_Mqtt_Msg_Cbs.Could_Status_Callback)
        Factory_Mqtt_Msg_Cbs.Could_Status_Callback(DEV_WIFI_CLOUD_DISCONNECT_STATE);
}


/**
*@?�称 		Factory_Mqtt_Recv_Message_Callback
*@?�能 		MQTT客户端接?�到消息?��?
*@?�数 		Entity_Mqtt_Context_t* context, void* user_data, const void* vmsg
*@返�???	void
*@使用说�?	
*/
static void Factory_Mqtt_Recv_Message_Callback(Entity_Mqtt_Context_t* context, void* user_data, const void* vmsg)
{
    Entity_Mqtt_Message_t *entity_msg = (Entity_Mqtt_Message_t *)vmsg;
    if (memcmp(entity_msg->Topic, Entity_Mqtt_Get_Topic(TOPIC_TYPE_EVENT_SUBSCRIBE), strlen(entity_msg->Topic)) == 0)
    {
        Entity_App_Msg_Queue_Send(ENTITY_MSG_TYPE_FACTORY_MQTT_EVENT, (const void*)entity_msg->Payload, entity_msg->Len);
    }
    else if(memcmp(entity_msg->Topic, Entity_Mqtt_Get_Topic(TOPIC_TYPE_CMD_SUBSCRIBE), strlen(entity_msg->Topic)) == 0)
    {
        Entity_App_Msg_Queue_Send(ENTITY_MSG_TYPE_FACTORY_MQTT_COMMAND, (const void*)entity_msg->Payload, entity_msg->Len);
    }
    else
    {
        char buf[100]={0};
        snprintf(buf, sizeof(buf), "%s", entity_msg->Topic);
        ENTITY_LOGE("%s !!!!!!!!not support topic:%s\n", __func__, buf);
    }
}


/**
*@?�称 		Factory_Mqtt_Client_Task
*@?�能 		?��?MQTT客户端任??*@?�数 		void *arg
*@返�???	void
*@使用说�?	
*/
void Factory_Mqtt_Client_Task(void *arg)
{
    int ret = OPRT_OK;
    Flag_Entity_Mqtt_Factory_Task_Over = 0;
    Entity_Mqtt_Context_t *entity_context = (Entity_Mqtt_Context_t *)&Factory_Client_Instance;
    memset(entity_context, 0, sizeof(Entity_Mqtt_Context_t));
    
    Entity_Device_Info_t *dev_info = Entity_Get_Dev_Info();
    char *device_id_str = Get_Device_Id_String();
    Entity_Mqtt_Topic_Init(dev_info->Pid, device_id_str);//?��??�未?��?，�?以没?�UUID
	Factory_Mqtt_Generate_Client_Config(&entity_context->Mqtt_Auth);
	Entity_Mqtt_Config_t entity_config = {0};
	entity_config.Tcp_Connect_Params.Host = Factory_Mqtt_Host;
	entity_config.Tcp_Connect_Params.Port = Factory_Mqtt_Port;
	entity_config.Tcp_Connect_Params.Timeout_Ms = FACTORY_MQTT_CMD_TIMEOUT_MS;    //建�?连接?�接?��?个�??��??��??�时?��??��?网�?波动?�弱网环境�?超时?�间不能太短
	entity_config.Device_Pid = dev_info->Pid;
	entity_config.Device_Id = device_id_str;
	entity_config.Device_Secret = NULL;
	entity_config.Keepalive = FACTORY_MQTT_KEEPALIVE_S;
	entity_config.Connected_Cb = Factory_Mqtt_Connected_Callback,
	entity_config.Disconnect_Cb = Factory_Mqtt_Disconnected_Callback,
    entity_config.Recv_Messages_Cb = Factory_Mqtt_Recv_Message_Callback,

	ret = Entity_Mqtt_Init(entity_context, &entity_config);
    if(ret != OPRT_OK){
        Flag_Entity_Mqtt_Factory_Task_Over = 1;
        return;
    }
    while(1)
    {
        Entity_Mqtt_Loop(entity_context);
        if(entity_context->Prohibit_Connect && !entity_context->Is_Connected){ //已断连�?禁止?��?，�??�?�任??        
            ENTITY_LOGE("factory mqtt client thread exit\r\n");
            Flag_Entity_Mqtt_Factory_Task_Over = 1;
            break;
        }
    }
    Entity_Pthread_Delete(&Mqtt_Client_Thread_Id);
}


/**
*@?�称 		Factory_Test_Mqtt_Client_Task_Start
*@?�能 		?�建?��?MQTT任务
*@?�数 		Factory_Mqtt_Msg_Cbs_t *cbs
*@返�???	int
*@使用说�?	
*/
int Factory_Test_Mqtt_Client_Task_Start(char *mqtt_host, unsigned short mqtt_port, Factory_Mqtt_Msg_Cbs_t *cbs)
{
    if(Flag_Entity_Mqtt_Factory_Task_Runing == 0)
    {
        Factory_Mqtt_Msg_Cbs = *cbs;
        strncpy(Factory_Mqtt_Host, mqtt_host, sizeof(Factory_Mqtt_Host)-1);
        Factory_Mqtt_Port = mqtt_port;
        int ret = Entity_Pthread_Create(&Mqtt_Client_Thread_Id, "Factory_Test_Mqtt task", FACTORY_MQTT_TASK_STACK, FACTORY_MQTT_TASK_PRIO, Factory_Mqtt_Client_Task, NULL);
        if(ret != 0) 
        {
            ENTITY_LOGE(" %s Failed!", __func__);
            return -1;
        }
        ENTITY_LOGD(" %s  success!", __func__);
        Flag_Entity_Mqtt_Factory_Task_Runing = 1;
        return 0;
    }
    else
    {
        ENTITY_LOGE("%s have run!\r\n", __func__);
        return 0;
    }
}

/**
*@?�称 		Factory_Test_Mqtt_Client_Task_Stop
*@?�能 		?�除?��?MQTT任务
*@?�数 		void
*@返�???	int
*@使用说�?	
*/
int Factory_Test_Mqtt_Client_Task_Stop(void)
{
    if(Flag_Entity_Mqtt_Factory_Task_Runing)
    {
        Flag_Entity_Mqtt_Factory_Task_Runing = 0;
        ENTITY_LOGI("%s\r\n", __func__);
        Entity_Mqtt_Context_t *entity_context = (Entity_Mqtt_Context_t *)&Factory_Client_Instance;
        Entity_Mqtt_Manu_Disconnect(entity_context);
        while(!Flag_Entity_Mqtt_Factory_Task_Over)//等�?任务结�?
            Entity_Sleep_Ms(10);
        Entity_Mqtt_Deinit(entity_context);
        ENTITY_LOGI("%s success", __func__);
    }
    return 0;
} 

/**
*@?�称 		Factory_Mqtt_Topic_Subscribe
*@?�能 		订�?主�?
*@?�数 		Topic_Type_e type_e, int qos
*@返�???	int                
*@使用说�?	
*/
int Factory_Mqtt_Topic_Subscribe(Topic_Type_e type_e, int qos)
{
    return Entity_Mqtt_Topic_Subscribe(&Factory_Client_Instance, type_e, qos);
}


/**
*@?�称 		Factory_Mqtt_Topic_Publish
*@?�能 		?��?主�?
*@?�数 		Topic_Type_e type_e, const char *data, int len, int qos, int retained
*@返�???	int           
*@使用说�?	
*/
int Factory_Mqtt_Topic_Publish(Topic_Type_e type_e, const char *data, int len, int qos, int retained)
{
    return Entity_Mqtt_Topic_Publish(&Factory_Client_Instance, type_e, data, len, qos, retained);
}













