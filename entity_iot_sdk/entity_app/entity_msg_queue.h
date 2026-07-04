//entity_msg_queue.h
#pragma once

typedef struct 
{
    unsigned char Type; 
    unsigned int Data_Len;
    void *Data;
}Entity_Msg_Data_t;

typedef enum
{
    ENTITY_MSG_TYPE_BLE,
    ENTITY_MSG_TYPE_MQTT_EVENT,
    ENTITY_MSG_TYPE_MQTT_COMMAND,
    ENTITY_MSG_TYPE_MQTT_PUBLISH,
    ENTITY_MSG_TYPE_SET_NET_MODE,
    ENTITY_MSG_TYPE_FACTORY_MQTT_EVENT,
    ENTITY_MSG_TYPE_FACTORY_MQTT_COMMAND,
    ENTITY_MSG_TYPE_REGISTER_MQTT_EVENT,
}Entity_Msg_Type_e;


//消息队列处理初始化
int Entity_App_Msg_Queue_Init(void);

//发送消息
int Entity_App_Msg_Queue_Send(unsigned char msg_type, const void *pdata, unsigned int data_len);






