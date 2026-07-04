//factory_test.h
#pragma once

#define FACTORY_SIMPLE_TEST_EN  //开启此宏将直接上报其它测试结果

//厂测默认连接的WIFI热点信息
#if 0
#define FACTORY_TEST_WIFI_SSID              "changsha123"
#define FACTORY_TEST_WIFI_PWD               "YYGYyygy147258"
#else
//#define FACTORY_TEST_WIFI_SSID              "factory_test"
//#define FACTORY_TEST_WIFI_PWD               "12345678"
#endif

#define FACTORY_DYNAMIC_REGISTER_WIFI_SSID      "!FT@IOT0#" //动态注册三元组时扫描到的SSID
#define FACTORY_NORMAL_REGISTER_WIFI_SSID       "!FT@IOT1#" //正常注册三元组时扫描到的SSID
#define FACTORY_REGISTER_WIFI_PWD               "31415926"  
#define FACTORY_VAILD_WIFI_RSSI                 (-60)       //厂测的热点的有效信号强度

#define DEFAULT_MQTT_HOST                       "mqtt.cetus-ai.com"
#define DEFAULT_MQTT_PORT                       2883

#define TEST_UDP_BROADCAST_PORT     40000   //厂测UDP广播端口
#define TEST_UDP_SERVER_PORT        40001   //厂测UDP服务端监听端口

#ifndef INADDR_BROADCAST
#define	INADDR_BROADCAST	        (0xffffffff)
#endif

//工具用的，设备的用户名是唯一码，密码与工具一致
#define FACTORY_TEST_MQTT_USE_NAME  "admin"
#define FACTORY_TEST_MQTT_PASSWD    "LBy0LmKyg17NWtJ8"

//厂测UDP通信协议
#define TEST_MSG_TYPE_BROADCAST     "thing.testing.broadcast"   
#define TEST_MSG_TYPE_SET           "thing.testing.set"
#define TEST_MSG_TYPE_OTA           "thing.testing.ota"
#define TEST_MSG_TYPE_OTA_RESPONSE  "thing.testing.ota_response"
#define TEST_MSG_TYPE_OTA_REPORT    "thing.testing.ota_report"

#define TEST_MSG_TYPE_BURN          "thing.testing.burn"//新增烧录三元组

//注册类型
typedef enum 
{
    DYNAMIC_REGISTER_TYPE,
    NORMAL_REGISTER_TYPE,
}Factory_Register_Type_e;

//厂测测试参数
typedef struct{
    int Wifi_Rssi_Threshold;//WIFI信号强度阈值
    int Wifi_Scan_Timeout;//wifi扫描超时时间 
}Factory_Test_Set_Info_t;

//需要烧录到设备的三元组信息
typedef struct{
    char* Pid;
    char* Uuid;
    char* Secret;
    char* Mac;
    char* Ap_Ssid;    //AP 配网热点名称前缀
    char* Ap_Pwd;    //AP 配网热点密码
    unsigned char Is_Mqtt_Data;
}Factory_Test_Burn_Info_t;

typedef struct 
{
    int (*Thing_Testing_Set_Callback)(char* mqtt, unsigned short port);
    int (*Thing_Testing_Ota_Callback)(const int type, const int file_size, const char* md5, const char* url, const char* version);
    void (*Thing_Testing_Burn_Callback)(char *msg_id, void *info);
}Factory_Test_Udp_Cbs_t;

//厂测初始化
int Entity_Factory_Test_Init(void);

//厂测反初始化
int Entity_Factory_Test_Deinit(void);

//启动厂测默认WIFI
void Factory_Test_Wifi_Config(void);

//获取设备唯一ID
char *Get_Device_Id_String(void);

//启动任务厂测或获取三元组
void Factory_Test_Auth_Check_Task_Start(void);

//停止任务厂测或获取三元组
void Factory_Test_Auth_Check_Task_Stop(void);







