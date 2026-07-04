//factory_mqtt_client.h
#pragma once

#include "entity_iot_func.h"
#include "entity_iot_cloud.h"

typedef enum
{
    FACTORY_MQTT_CONNCET_STEP,
    FACTORY_MQTT_SUB_CMD_TOPIC_STEP,
    FACTORY_MQTT_SUB_EVENT_TOPIC_STEP,
    FACTORY_MQTT_TIME_REPORT_STEP,
    FACTORY_MQTT_INFO_REPORT_STEP,
    FACTORY_MQTT_MODEL_REPORT_STEP,

    FACTORY_MQTT_IDLE_STEP,
}Factory_Mqtt_Connect_Step_e;


typedef struct 
{
    void (*Could_Status_Callback)(unsigned char status);
    void (*Set_Test_Info_Callback)(char *msg_id, void *info);
    void (*Test_Start_Callback)(char *msg_id);
    void (*Set_Burn_Info_Callback)(char *msg_id, void *info); 
    void (*Manual_Test_Callback)(char *code);  
}Factory_Mqtt_Msg_Cbs_t;

//创建厂测MQTT任务
int Factory_Test_Mqtt_Client_Task_Start(char *mqtt_host, unsigned short mqtt_port, Factory_Mqtt_Msg_Cbs_t *cbs);

//删除厂测MQTT任务
int Factory_Test_Mqtt_Client_Task_Stop(void);

//获取厂测MQTT消息处理回调
Factory_Mqtt_Msg_Cbs_t *Get_Factory_Mqtt_Msg_Cbs(void);

//订阅主题
int Factory_Mqtt_Topic_Subscribe(Topic_Type_e type_e, int qos);

//发布主题
int Factory_Mqtt_Topic_Publish(Topic_Type_e type_e, const char *data, int len, int qos, int retained);

