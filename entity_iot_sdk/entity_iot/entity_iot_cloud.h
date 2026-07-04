//entity_iot_cloud.h
#pragma once


#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "cJSON.h"


#define ENTITY_PRODUCT_ID_MAX_SIZE         (33)            //产品PID最大长度
#define ENTITY_PRODUCT_SECRET_MAX_SIZE     (65)           //产品授权码最大长度
#define ENTITY_DEVICE_ID_MAX_SIZE          (33)            //设备UUID最大长度
#define ENTITY_DEVICE_SECRET_MAX_SIZE      (65)            //设备授权码最大长度
#define ENTITY_MAC_MAX_SIZE                (33)            //MAC地址最大长度

#define ENTITY_WIFI_SSID_MAX_SIZE          (65)            //热点名称最大长度
#define ENTITY_WIFI_KEY_MAX_SIZE           (65)            //热点密码最大长度

#define ENTITY_BIND_ID_MAX_SIZE            (65)            //bind id 最大长度
#define ENTITY_USER_ID_MAX_SIZE            (33)            //user id 最大长度
#define ENTITY_ASSET_ID_MAX_SIZE           (20)            /* 资产ID最大长度 */
#define ENTITY_COUNTRY_MAX_SIZE            (4)             //国家码 最大长度
#define ENTITY_TIME_ZONE_MAX_SIZE          (65)             //用户时区字符串 最大长度

#define ENTITY_DEV_VERSION_MAX_SIZE        (10)            //版本号最大长度

#define ENTITY_MQTT_TOPIC_MAX_LEN        (128)    //mqtt topic 最大长度
#define ENTITY_MQTT_USER_NAME_MAX_LEN    (96)    //mqtt用户名字符串最大长度 
#define ENTITY_MQTT_PASSWORD_MAX_LEN     (96)    // mqtt密码字符串最大长度 
#define ENTITY_MQTT_CLIENT_ID_MAX_LEN    (64)    //mqtt client字符创最大长度 
#define ENTITY_MQTT_HOST_MAX_LEN         (65)    //mqtt地址最大长度
#define ENTITY_MQTT_MSG_SEND_MAX_LEN 	   (8192)  //MQTT发送消息最大长度
#define ENTITY_MQTT_MSG_RECV_MAX_LEN 	   (8192)   //MQTT接收消息最大长度

#define ENTITY_AUTH_REGISTER_CODE_SIZE_MAX  (49)

#define ENTITY_IOT_THING_MODEL_MAX_SIZE   (3096-12)    

//设备重置类型
typedef enum
{
    ENTITY_RESET_NOT_CLEAR_DATA,       // 重置设备但不清除数据
    ENTITY_RESET_AND_CLEAR_DATA,       // 重置设备且清除数据
}Entity_Reset_Type_e;

/* IPC 实时流类型 */
typedef enum
{
    IPC_RTSPS = 0,  
    IPC_RTMPS,
}Ipc_Push_Type_e;

/* 物模型枚举 */
typedef enum
{
    MODEL_TYPE_COMPLETE = 0, //完整的
    MODEL_TYPE_SIMPLE,       //简单的
    MODEL_TYPE_MINI,         //迷你的
}Model_Type_e;

typedef enum
{
    QOS0_MOST_ONCE, //至多一次 不重发
    QOS1_LEAST_ONCE,//至少一次 重发
    QOS2_EXACTLY_ONCE,//确保只有一次
}Mqtt_Qos_Type_e;

//主题类型，通过此查找主题内容
typedef enum
{
    TOPIC_TYPE_EVENT_PUBLISH = 0,   //事件发布主题类型
    TOPIC_TYPE_EVENT_SUBSCRIBE,     //事件订阅主题类型
    TOPIC_TYPE_CMD_PUBLISH,         //命令发布主题类型
    TOPIC_TYPE_CMD_SUBSCRIBE,       //命令订阅主题类型
    TOPIC_TYPE_MAX,
}Topic_Type_e;

//设备状态 
typedef enum
{
    DEV_UNAUTHORIZED_STATE,         // 默认设备未授权  
    DEV_UNPROVISION_STATE,          // 设备未入网状态
    DEV_PROVISIONING_STATE,         //配网中状态
    DEV_PROVISION_SUCCESS_STATE,    //配网成功
    DEV_PROVISION_FAILD_STATE,      //配失败
    DEV_WIFI_PROVISION_STATE,       // 设备WIFI入网状态
    DEV_SIGMESH_PROVISION_STATE,    // 设备SIGMESH入网状态
    DEV_BLE_PROVISION_STATE,        // 设备BLE入网状态
    DEV_WIFI_CONNECTING_STATE,       //WIFI正在连接状态
    DEV_WIFI_CONNECTED_STATE,       //WIFI已连接状态
    DEV_WIFI_DISCONNECT_STATE,      //WIFI断开连接状态
    DEV_WIFI_CLOUD_CONNECT_STATE,         // 设备WIFI云端已连接
    DEV_WIFI_CLOUD_DISCONNECT_STATE,      // 设备WIFI云端已断开
    DEV_SIGMESH_CLOUD_CONNECT_STATE,      // 设备SIGMESH云端已连接
    DEV_SIGMESH_CLOUD_DIS_STATE,          // 设备SIGMESH云端已断开
    DEV_BLE_CLOUD_CONNECT_STATE,          // 设备BLE云端已连接
    DEV_BLE_CLOUD_DIS_STATE,              // 设备BLE云端已断开
    DEV_OTA_START_STATE,
    DEV_OTA_SUCCESS_STATE,
    DEV_OTA_FAILD_STATE,
    DEV_ENTER_FACTORY_TEST_STATE,             //进入厂测
    DEV_EXIT_FACTORY_TEST_STATE,             //退出厂测
}Entity_Dev_State_e;

//配网绑定类型
typedef enum
{
    UNBIND_TYPE,
    WIFI_BIND_TYPE,
    BLE_BIND_TYPE,
}Entity_Bind_Type_e;



//三元组信息
typedef struct
{
    char Uuid[ENTITY_DEVICE_ID_MAX_SIZE];        //设备UUID
    char Secret[ENTITY_DEVICE_SECRET_MAX_SIZE];  //设备授权码
    char Mac[ENTITY_MAC_MAX_SIZE];               //设备MAC地址，一般是指WIFI MAC
    char Pid[ENTITY_PRODUCT_ID_MAX_SIZE];        //产品ID,暂未用到，是以代码为准
    char Register_Code[ENTITY_AUTH_REGISTER_CODE_SIZE_MAX];
}Entity_Triple_Info_t;

//MQTT信息
typedef struct
{
    char Mqtt_Host[ENTITY_MQTT_HOST_MAX_LEN];      //mqtt地址  
    unsigned short Mqtt_Port;                   //mqtt端口号
}Entity_Mqtt_Info_t;

//WIFI信息
typedef struct
{
    char Ssid[ENTITY_WIFI_SSID_MAX_SIZE];       //热点名称
    char Key[ENTITY_WIFI_KEY_MAX_SIZE];         //热点密码
}Entity_Wifi_Info_t;


typedef enum  
{
	ENTITY_MQTT_INVALID = 0,
	ENTITY_MQTT_DIRECT,          /*MQTT direct, no TLS used*/
	ENTITY_MQTT_TLS_DIRECT,	   /*MQTT direct,  TLS used*/
	ENTITY_MQTT_TLS_GUIDER       /*MQTT message protected by TLS*/
}Entity_Mqtt_Link_Type_e;

typedef struct 
{
    Entity_Mqtt_Link_Type_e Link_Type;
    unsigned short Mqtt_Port;
    char Mqtt_Host[ENTITY_MQTT_HOST_MAX_LEN];
    char Client_Id[ENTITY_MQTT_CLIENT_ID_MAX_LEN];
    char User_Name[ENTITY_MQTT_USER_NAME_MAX_LEN];
    char Password[ENTITY_MQTT_PASSWORD_MAX_LEN];
}Entity_Mqtt_Client_Info_t;

//配网信息
typedef struct
{
    Entity_Wifi_Info_t Wifi_Info;
    Entity_Mqtt_Info_t Mqtt_Info;
    char Country_Code[ENTITY_COUNTRY_MAX_SIZE];//国家码 
    char User_Tz_Str[ENTITY_TIME_ZONE_MAX_SIZE];      //用户所在时区 "Asia/Shanghai"
    char Bind_Id[ENTITY_BIND_ID_MAX_SIZE];  //
    char User_Id[ENTITY_USER_ID_MAX_SIZE];      // ipc配网绑定才有
}Entity_Config_Net_Info_t;

//设备信息
typedef struct
{
    unsigned char Need_Clean_Data;              //清除数据
    char Pid[ENTITY_PRODUCT_ID_MAX_SIZE];
    char Dev_Version[ENTITY_DEV_VERSION_MAX_SIZE]; //设备软件版本号
    char Sub_Version[ENTITY_DEV_VERSION_MAX_SIZE]; //子设备或MCU版本号
    cJSON *Thing_Model_Properties;              //物模型CJSON指针
    char Product_Secret[ENTITY_PRODUCT_SECRET_MAX_SIZE];
}Entity_Device_Info_t;

//设备相关回调
typedef struct 
{
    void (*State_Callback)(unsigned char state);   //状态回调
}Entity_Dev_Cbs_t;