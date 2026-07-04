//factory_mqtt_parse.h
#pragma once

#include "factory_test.h"


//产测
#define FACTORY_COMMAND_TEST_SETTINGS_CODE         "settings"
#define FACTORY_COMMAND_TEST_TESTING_CODE          "testing"
#define FACTORY_COMMAND_TEST_TESTED_CODE           "tested"
#define FACTORY_COMMAND_TEST_BURN_CODE             "burn"
#define FACTORY_COMMAND_TEST_ERASE_CODE            "erase"
#define FACTORY_COMMAND_TEST_PROPERTY_SET_CODE     "property_set"

typedef enum{
    FACTORY_TEST_STEP_CODE_BUTTON = 8,            //按键 测试
    FACTORY_TEST_STEP_CODE_WIFI = 9,             //WiFi 测试
    FACTORY_TEST_STEP_CODE_NFC = 10,              //NFC 测试
    FACTORY_TEST_STEP_CODE_GYRO = 11,           //陀螺仪 测试
}Factory_Test_Step_Code_e;

typedef enum{
    FACTORY_TEST_RESULT_OK,   //测试 OK
    FACTORY_TEST_RESULT_WEAK_RSSI,   //测试信号比较弱，不符合要求
    FACTORY_TEST_RESULT_TIMEOUT,     //测试超时
}Factory_Test_Result_e;


//厂测mqtt命令消息处理
void Factory_Mqtt_Msg_Cmd_parse_Process(char *msg);

//mqtt事件应答消息处理
void Factory_Mqtt_Msg_Event_Respone_Parse_Process(char *msg);

//厂测设置测试参数的命令应答
void Factory_Mqtt_Cmd_Test_Settings_Respone(const char *msgId, int authStatus);

//厂测烧录三元组相关的命令应答
void Factory_Mqtt_Cmd_Test_Burn_Respone(const char *msgId, int result, Factory_Test_Burn_Info_t* info);

//厂测设备信息上报
void Factory_Mqtt_Event_Device_Info_Report(void);

//厂测结果上报
void Factory_Mqtt_Test_Result_Report(unsigned char step, unsigned char result, int rssi);

//厂测设置测试参数的事件上报
void Factory_Mqtt_Test_Settings_Event_Report(int authStatus);

//事件请求-获取云端时间
void Factory_Mqtt_Event_Get_Time_Request(unsigned char ack);