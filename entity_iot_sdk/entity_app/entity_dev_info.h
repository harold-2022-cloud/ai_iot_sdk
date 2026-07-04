//entity_dev_info.h
#pragma once

#include "entity_iot_func.h"
#include "cJSON.h"
#include "entity_iot_cloud.h"



#define MAGIC_HEADER_VALUE           0X11223355 //AI

//三元组信息
typedef struct
{
    unsigned int Magic_Header;               //数据头校验值
    unsigned int Flag_Vaild;                 //三元组有效标志
    Entity_Triple_Info_t Triple_Info;
    char Reserve[64];                       //预留字节，根据实际情况调整
}Entity_Dev_Triple_Info_t;

//配网信息
typedef struct
{
    unsigned int Magic_Header;               //数据头校验值
    unsigned int Flag_Bind;                //已绑定标志
    unsigned int Bind_Type;                //绑定类型 0:wifi, 1: ble
    unsigned int Flag_Wifi_Info_Vaild;     //wifi信息有效标志 配网时获取到IP时置1
    Entity_Config_Net_Info_t Config_Net_Info; //配网信息
    unsigned char Flag_Clean_Data;          //解绑时是否清除数据标志，bind上报时需要传入，收到应答后需要清除
    char Reserve[63];                       //预留字节，根据实际情况调整
}Entity_Dev_Config_Net_Info_t;

typedef struct 
{
    unsigned int Magic_Header;               //数据头校验值
    unsigned int Have_Model;
    unsigned int Len;
    char Content[ENTITY_IOT_THING_MODEL_MAX_SIZE];
}Entity_Dev_Thing_Model_Info_t;


typedef struct {
    Entity_Dev_Triple_Info_t Dev_Triple_Info;
    Entity_Dev_Config_Net_Info_t Dev_Config_Net_Info;
    Entity_Device_Info_t Dev_Info;
    Entity_Dev_Thing_Model_Info_t Dev_Thing_Model_Info;
    unsigned int Dev_State;
}Entity_App_Param_t;


typedef struct 
{
    int (*Flash_Write_Triple_Cb)(unsigned char *pdata, unsigned int len);
    int (*Flash_Read_Triple_Cb)(unsigned char *pdata, unsigned int len);
    int (*Flash_Write_Config_Net_Cb)(unsigned char *pdata, unsigned int len);
    int (*Flash_Read_Config_Net_Cb)(unsigned char *pdata, unsigned int len);
    int (*Flash_Write_Thing_Model_Info_Cb)(unsigned char *pdata, unsigned int len);
    int (*Flash_Read_Thing_Model_Info_Cb)(unsigned char *pdata, unsigned int len);
    int (*Flash_Read_Key_Value_Cb)(const char *key, unsigned char *pdata, unsigned int len);
}Entity_Flash_Cbs_t;


//初始化设备相关回调接口
void Entity_Dev_Cbs_Init(Entity_Dev_Cbs_t *cbs);

//获取初始化设备相关回调接口
Entity_Dev_Cbs_t *Get_Entity_Dev_Cbs(void);

//设置设备状态
void Entity_Set_Dev_Status(unsigned char status);

//初始化FLASH操作接口
void Entity_Flash_Cbs_Init(Entity_Flash_Cbs_t *cbs);

//按 key 读取 NVS/Flash 键值（通用 KV 读；未绑定返回 -1）
int Entity_Flash_Read_Key_Value(const char *key, unsigned char *pdata, unsigned int len);

//获取应用程序参数
Entity_App_Param_t *Get_Entity_App_Param(void);

//获取三元组信息
Entity_Triple_Info_t *Entity_Get_Triple_Info(void);

//获取配网信息
Entity_Config_Net_Info_t *Entity_Get_Config_Net_Info(void);

//获取设备配网信息
Entity_Dev_Config_Net_Info_t *Entity_Get_Dev_Config_Net_Info(void);

//获取物模型信息
Entity_Dev_Thing_Model_Info_t *Get_Dev_Thing_Model_Info(void);

//获取设备保存的物模型信息接口CJSON
cJSON* Entity_Mqtt_Get_Dev_Thing_Model_Cjson(void);

//获取设备信息
Entity_Device_Info_t *Entity_Get_Dev_Info(void);

//获取WIFI信息
Entity_Wifi_Info_t *Entity_Get_Wifi_Info(void);

//获取MQTT信息
Entity_Mqtt_Info_t *Entity_Get_Mqtt_Info(void);

//将FLASH中三元组信息恢复出厂设置
int Entity_Reset_Triple_Info_To_Flash(void);

//保存三元 组信息到FLASH
int Entity_Save_Triple_Info_To_Flash(void);

//从FLASH中读三元组信息
int Entity_Read_Triple_Info_From_Flash(void);

//将FLASH中配网信息恢复出厂设置
int Entity_Reset_Config_Net_Info_To_Flash(void);

//保存配网信息到FLASH
int Entity_Save_Config_Net_Info_To_Flash(void);

//从FLASH中读配网信息
int Entity_Read_Config_Net_Info_From_Flash(void);

//将FLASH中物模型信息恢复出厂设置
void Entity_Reset_Thing_Model_To_Flash(void);

//保存物模型信息到FLASH
int Entity_Save_Thing_Model_Info_To_Flash(void);

//从FLASH中读物模型信息
int Entity_Read_Thing_Model_Info_From_Flash(void);

//初始化设备信息
int Entity_Load_Dev_Info(const char *pid, const char *product_secret, const char *dev_version, const char *sub_version, Entity_Triple_Info_t *test_triple);

//获取设备当前状态
unsigned char Get_Entity_Dev_State(void);














