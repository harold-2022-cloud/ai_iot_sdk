//entity_mqtt_client.h
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "mi_mqtt_client.h"

#include "entity_iot_cloud.h"

//犀云MQTT工作步骤
typedef enum
{
    ENTITY_MQTT_IDLE_STATE,
    ENTITY_MQTT_CONNCET_STATE,            //连接中
    ENTITY_MQTT_SUBSCRIBING_STATE,        //等待订阅应答中
    ENTITY_MQTT_SUBSCRIBE_COMPLETE_STATE, //订阅完成 
    ENTITY_MQTT_YIELD_STATE,              //轮询
    ENTITY_MQTT_RECONNECT_STATE,          //重连
}Entity_Mqtt_Process_State_e;

typedef struct Entity_Mqtt_Context Entity_Mqtt_Context_t;

//犀云MQTT配置信息
typedef struct {
    Tls_Connect_Params_t Tls_Connect_Params;       
    Tcp_Connect_Params_t Tcp_Connect_Params;
    uint16_t        Keepalive;          //保活周期， 单位秒 
    
    const char*    Device_Pid;          //产品 PID
    const char*    Device_Id;           //设备ID UUID
    const char*    Device_Secret;       //密钥
    void*          User_Data;
    void           (*Connected_Cb)(Entity_Mqtt_Context_t* context, void* user_data);
    void           (*Disconnect_Cb)(Entity_Mqtt_Context_t* context, void* user_data);
    void           (*Recv_Messages_Cb)(Entity_Mqtt_Context_t* context, void* user_data, const void* vmsg);
} Entity_Mqtt_Config_t;

//MQTT认证信息
typedef struct {
    char Client_Id[ENTITY_MQTT_CLIENT_ID_MAX_LEN];
    char Username[ENTITY_MQTT_USER_NAME_MAX_LEN];
    char Password[ENTITY_MQTT_PASSWORD_MAX_LEN];
} Entity_Mqtt_Auth_t;


//犀云MQTT客户端信息
struct Entity_Mqtt_Context{
    void* Mqtt_Client;                      //MQTT客户端 
    Entity_Mqtt_Config_t Config;              //犀云MQTT配置信息
    Entity_Mqtt_Auth_t Mqtt_Auth;             //MQTT认证信息
    void* User_Data;                        //用户数据
    uint32_t Msgid_Inc;
    uint16_t Last_Subscribe_Id;             //最后的订阅ID
    uint8_t State;
    bool Prohibit_Connect;                  //禁止连接
    bool Is_Connected;
};

//MQTT消息结构体
typedef struct 
{
    uint16_t Msgid;
    const char *Topic;
    const char *Payload;
    int Len;
    int Qos;
}Entity_Mqtt_Message_t;



//犀云MQTT客户端初始化
int Entity_Mqtt_Init(Entity_Mqtt_Context_t* context, const Entity_Mqtt_Config_t* config);

//犀云MQTT客户端反初始化
int Entity_Mqtt_Deinit(Entity_Mqtt_Context_t* context);

//犀云MQTT客户端TOPIC初始化
void Entity_Mqtt_Topic_Init(const char* pid, const char* uuid);

//动态注册TOPIC初始化
void Entity_Register_Mqtt_Topic_Init(const char* pid, const char* uuid);

//根据主题类型查找主题内容
char *Entity_Mqtt_Get_Topic(Topic_Type_e type_e);

//犀云MQTT客户端主动断开连接
int Entity_Mqtt_Manu_Disconnect(Entity_Mqtt_Context_t* context);

//犀云MQTT客户端主动触发重连（不禁止后续自动连接）
int Entity_Mqtt_Force_Reconnect(Entity_Mqtt_Context_t* context, const char *reason);

/* 犀云MQTT客户端工作状态机
 * 返回值：需要在鎖外 sleep 的毫秒數（0 = 不需要 sleep）。
 * 所有退避延遲由調用方（entity_mqtt_app.c）在持鎖結束後執行，
 * 確保 Publish 端不因 MQTT Task sleep 而被阻塞超過 1s。*/
int Entity_Mqtt_Loop(Entity_Mqtt_Context_t* context);

//犀云MQTT客户端是否已连接
bool Entity_Mqtt_Is_Connected(Entity_Mqtt_Context_t* context);

/* 重置 keepalive 計時器（在 Agora RTC 入會前調用） */
void Entity_Mqtt_Reset_Keepalive(Entity_Mqtt_Context_t *context);

//犀云MQTT客户端订阅主题
int Entity_Mqtt_Topic_Subscribe(Entity_Mqtt_Context_t* context,Topic_Type_e type_e, int qos);

//犀云MQTT客户端取消订阅主题
int Entity_Mqtt_Topic_Unsubscribe(Entity_Mqtt_Context_t* context, Topic_Type_e type_e, int qos);

//犀云MQTT客户端发布主题
int Entity_Mqtt_Topic_Publish(Entity_Mqtt_Context_t* context, Topic_Type_e type_e, const char *data, int len, int qos, int retained);
