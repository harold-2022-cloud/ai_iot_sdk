//entity_config_net.h
#pragma once

//当前网络模式
typedef enum 
{
    ENTITY_NET_MODE_NONE,     //未配网且未连网的待机状态
    ENTITY_NET_MODE_BLE_CONFIG,//蓝牙配网模式
    ENTITY_NET_MODE_CONFIG_TRY_CONNECT,//在配网拿到WIFI信息后尝试连接网络
    ENTITY_NET_MODE_STA_START, //配网后启动WIFI STA连网
    ENTITY_NET_MODE_STA_STOP, //关闭STA
    ENTITY_NET_MODE_BLE_BIND,//蓝牙绑定模式
}Entity_Net_Mode_e;

typedef enum 
{
    ENTITY_NOT_IN_CONFIG_STATUS_MASK   =(1<<7),//非配网状态位掩码
    ENTITY_HAVE_BIND_MASK              =(1<<6),//已绑定标志位掩码
    ENTITY_WIFI_CONNECTED_MASK         =(1<<5),//已连网标志位掩码，以获取到IP判断
    ENTITY_COMMON_BIN_MASK             =(1<<4),//是否通用固件位掩码
    ENTITY_EXT_PROTOCOL_MASK           =(1<<1),//是否使用聚合协议位掩码
    ENTITY_REQUST_CONNECT_MASK         =(1<<0),//请求连接标志位掩码
}Entity_Config_Net_Flag_Bit_Mask_e;


//WIFI配网相关参数
typedef struct
{
    unsigned char Flag_Wifi_Need_Reconnect; //wifi需要重连标志
    unsigned char Flag_Wifi_Info_Vaild;     //wifi信息有效标志 配网时获取到IP时置1
    unsigned char Config_Net_Type;          //0-WIFI 1-ble
    unsigned char Flag_Config_Net_Success;  //配网成功标志
    unsigned char Flag_Config_Net_Progress; //正在配网过程中标志 进入配网置1，获取IP地址清0，关闭蓝牙广播
    unsigned char Flag_Have_Got_Ip;         //是否已获取到IP
    unsigned char Flag_Have_Get_Config_Net_Info; //
    unsigned char Entity_Config_Net_Flag;     //蓝牙配网扫描应答数据中的FLAG
}Wifi_Account_Param_t;



//启动配网定时器
void Entity_Config_Net_Timer_Start(void);

//停止配网定时器
void Entity_Config_Net_Timer_Stop(void);

//获取WIFI相关信息
Wifi_Account_Param_t *Get_Wifi_Account_Param(void);

//设置配网标志位
void Set_Flag_Config_Net_Progress(unsigned char value);

//获取配网标志位
unsigned char Get_Flag_Config_Net_Progress(void);

//设置当前网络模式
void Entity_Set_Net_Mode(Entity_Net_Mode_e mode);

//设置当前网络模式消息发送
void Entity_Set_Net_Mode_Msg_Send(Entity_Net_Mode_e mode);
