//factory_test.c
#include "factory_test.h"

#include "factory_mqtt_parse.h"
#include "factory_mqtt_client.h"
#include "factory_test_peripheral.h"
#include "factory_dynamic_register.h"

#include "entity_udp.h"
#include "entity_log.h"
#include "entity_dev_info.h"
#include "com_utils.h"
#include "cJSON.h"
#include "entity_wifi.h"
#include "entity_iot_func.h"
#include "entity_config_net.h"
#include "entity_io_control.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>


#define FACTORY_TEST_AUTH_CHECK_TASK_PROI           3
#define FACTORY_TEST_AUTH_CHECK_TASK_STACK_SIZE     4096

static void *Factory_Test_Auth_Check_Thread_Id;
static unsigned char Flag_Factory_Test_Auth_Check_Runing;
static __attribute__((unused)) unsigned char Flag_Factory_Test_Auth_Check_Exit;

static unsigned char Flag_Factory_Test_Start;

static Udp_Boradcast_Task_Info_t *Udp_Boradcast_Task_Info;
static Udp_Server_Task_Info_t *Udp_Server_Task_Info;


static Factory_Test_Udp_Cbs_t Factory_Test_Udp_Cbs;

static unsigned char Flag_Burn;
static unsigned char Flag_Recv_Testing_Info;
static Factory_Test_Set_Info_t Factory_Test_Set_Info;
static char *Device_Id_String;

static unsigned char Flag_Wifi_Scan_Success;
static Factory_Register_Type_e Factory_Register_Type;
static char Factory_Test_Wifi_Ssid[ENTITY_WIFI_SSID_LEN_MAX+2];


char* Entity_Generate_Test_Response_Json_String(const char* type, const char* msgId, const int code, const char* result_msg);

/**
*@名称 		Get_Device_Id_String
*@功能 		获取设备唯一ID
*@参数 		void
*@返回值 	char *
*@使用说明	
*/
char *Get_Device_Id_String(void)
{
    unsigned char mac[6]={0};
    if(Device_Id_String == NULL)
    {
        Device_Id_String = Entity_Mem_Calloc(1, 20);
        if(Device_Id_String == NULL)
        {
            ENTITY_LOGE("malloc faild\r\n");
            return NULL;
        }
        Entity_Get_Wifi_Mac(mac);
        sprintf(Device_Id_String, "%02x%02x%02x%02x%02x%02x", mac[0], mac[1],mac[2],mac[3],mac[4],mac[5]);
    }
    return Device_Id_String;
}


/**
*@名称 		Factory_Could_Status_Callback
*@功能 		云端连接状态改变回调
*@参数 		unsigned char status
*@返回值 	void
*@使用说明	
*/
static void Factory_Could_Status_Callback(unsigned char status)
{
    if(status == DEV_WIFI_CLOUD_CONNECT_STATE)
    {
        ENTITY_LOGI("cloud connect\r\n");
    }
    else
    {
        ENTITY_LOGI("cloud disconnect\r\n");
    }
}

/**
*@名称 		Factory_Set_Test_Info_Callback
*@功能 		设置测试信息回调
*@参数 		char *msg_id, void *info
*@返回值 	void
*@使用说明	
*/
static void Factory_Set_Test_Info_Callback(char *msg_id, void *info)
{
    Entity_Sleep_Ms(100);
    Factory_Mqtt_Test_Settings_Event_Report(Flag_Burn);
    Factory_Test_Set_Info = *((Factory_Test_Set_Info_t*)info);
    Flag_Recv_Testing_Info = 1;
}

/**
*@名称 		Factory_Test_Start_Callback
*@功能 		启动测试
*@参数 		char *msg_id
*@返回值 	void
*@使用说明	
*/
static void Factory_Test_Start_Callback(char *msg_id)
{
     if(!Flag_Recv_Testing_Info)
     {
        ENTITY_LOGE("settings info is null.\r\n");
        return;
    }
     //测试能进行，表明按键和无线网络都是正常的，直接上报测试结果
    unsigned char rssi_level, rssi_quality;
    int rssi;
    rssi = Entity_Wifi_Load_Signal_Level_Quality(&rssi_level, &rssi_quality);
    Factory_Mqtt_Test_Result_Report(FACTORY_TEST_STEP_CODE_BUTTON, FACTORY_TEST_RESULT_OK, 0);
    if(rssi < Factory_Test_Set_Info.Wifi_Rssi_Threshold)
        Factory_Mqtt_Test_Result_Report(FACTORY_TEST_STEP_CODE_WIFI, FACTORY_TEST_RESULT_WEAK_RSSI, rssi);
    else
        Factory_Mqtt_Test_Result_Report(FACTORY_TEST_STEP_CODE_WIFI, FACTORY_TEST_RESULT_OK, rssi);

#ifdef FACTORY_SIMPLE_TEST_EN
    Factory_Mqtt_Test_Result_Report(FACTORY_TEST_STEP_CODE_NFC, FACTORY_TEST_RESULT_OK, 0);//NFC直接上报成功
    Factory_Mqtt_Test_Result_Report(FACTORY_TEST_STEP_CODE_GYRO, FACTORY_TEST_RESULT_OK, 0);//陀螺仪直接上报成功   
#else
    //启动其它测试项
#endif
}



/**
*@名称 		Factory_Manual_Test_Callback
*@功能 		启动手动测试
*@参数 		char *code 测试项目
*@返回值 	void
*@使用说明	
*/
static void Factory_Manual_Test_Callback(char *code)
{
    ENTITY_LOGI("Factory_Manual_Test:%s\r\n", code);
    if(strcmp(code, "mic") == 0)
    {
        Factory_Manual_Test_Mic();//手动测试MIC
    }
    else if(strcmp(code, "speaker") == 0)
    {
        Factory_Manual_Test_Speaker();//测试喇叭播放音频
    }
    else if(strcmp(code, "ledIndicator") == 0)
    {
        Factory_Manual_Test_Led_Indicator();//手动测试LED
    }
    else if(strcmp(code, "lcdDisplay") == 0)
    {
        Factory_Manual_Test_Lcd_Display();//手动测试LCD显示
    }
    else if(strcmp(code, "motor") == 0)
    {
        Factory_Manual_Test_Motor();//手动测试马达
    }
}

/**
*@名称 		Factory_Set_Burn_Info_Callback
*@功能 		设置烧录试信息回调
*@参数 		char *msg_id, void *info
*@返回值 	void
*@使用说明	
*/
static void Factory_Set_Burn_Info_Callback(char *msg_id, void *info)
{
    Factory_Test_Burn_Info_t *burn_info = (Factory_Test_Burn_Info_t*)info;
    char secret_mask[32];
    char ap_pwd_mask[32];
    Utils_Mask_Secret(burn_info->Secret, secret_mask, sizeof(secret_mask));
    Utils_Mask_Secret(burn_info->Ap_Pwd, ap_pwd_mask, sizeof(ap_pwd_mask));
    ENTITY_LOGI("is_mqtt_data:%d, pid: %s, uuid: %s, seceret_mask:%s, mac:%s, apsid:%s, appwd_mask:%s", burn_info->Is_Mqtt_Data, burn_info->Pid, burn_info->Uuid, secret_mask, \
                            burn_info->Mac, burn_info->Ap_Ssid, ap_pwd_mask);

    Entity_Device_Info_t *dev_info = Entity_Get_Dev_Info();                        
    if(strcmp(dev_info->Pid, burn_info->Pid) != 0)
    {
        ENTITY_LOGE("pid is not the same, local_pid:%s, info_pid:%s", dev_info->Pid, burn_info->Pid); 
        goto set_fail;
    }
    //获取三元组指针并设置参数，保存到FLASH
    Entity_Triple_Info_t *triple_info = Entity_Get_Triple_Info();
    //如果已有三元组，应先删除再下发。
    if(strlen(triple_info->Uuid) && strlen(triple_info->Secret) && strlen(triple_info->Mac))
    {
        goto set_fail;
    }
    strncpy(triple_info->Uuid, burn_info->Uuid, sizeof(triple_info->Uuid)-1);
    strncpy(triple_info->Secret, burn_info->Secret, sizeof(triple_info->Secret)-1);
    strncpy(triple_info->Mac, burn_info->Mac, sizeof(triple_info->Mac)-1);
    Entity_Save_Triple_Info_To_Flash();
    if(burn_info->Is_Mqtt_Data)
        Factory_Mqtt_Cmd_Test_Burn_Respone(msg_id, 0, burn_info);    
    else
    {
        char* response_str = Entity_Generate_Test_Response_Json_String(TEST_MSG_TYPE_BURN, msg_id, 0, "success");
        if(Udp_Server_Task_Info->Send_Bytes)
            Udp_Server_Task_Info->Send_Bytes(response_str, strlen(response_str));
        FREE_MEMORY(response_str);
    }   
    Flag_Burn = 1;
    Entity_Factory_Test_Deinit();
    Entity_System_Reset();//厂测完成是否立即重启
    return;
set_fail:
    if(burn_info->Is_Mqtt_Data)
        Factory_Mqtt_Cmd_Test_Burn_Respone(msg_id, -1, burn_info);       
    else
    {
        char* response_str = Entity_Generate_Test_Response_Json_String(TEST_MSG_TYPE_BURN, msg_id, -1, "fail");
        if(Udp_Server_Task_Info->Send_Bytes)
            Udp_Server_Task_Info->Send_Bytes(response_str, strlen(response_str));
        FREE_MEMORY(response_str);
    }
}


/**
*@名称 		Udp_Testing_Set_Callback
*@功能 		UDP监听接收到的设置命令处理回调
*@参数 		unsigned char status
*@返回值 	int
*@使用说明	
*/
static int Udp_Testing_Set_Callback(char* mqtt_host, const uint16_t port)
{
    Factory_Mqtt_Msg_Cbs_t Factory_Mqtt_Msg_Cbs=
    {
        .Could_Status_Callback = Factory_Could_Status_Callback,
        .Set_Test_Info_Callback = Factory_Set_Test_Info_Callback,
        .Test_Start_Callback = Factory_Test_Start_Callback,
        .Set_Burn_Info_Callback = Factory_Set_Burn_Info_Callback,
        .Manual_Test_Callback = Factory_Manual_Test_Callback,
    };
    Factory_Test_Mqtt_Client_Task_Start(mqtt_host, port, &Factory_Mqtt_Msg_Cbs);//连接厂测MQTT服务
    return 0;
}   

/**
*@名称 		Udp_Testing_Ota_Callback
*@功能 		UDP监听接收到的OTA命令处理回调
*@参数 		const int type, const int file_size, const char* md5, const char* url, const char* version
*@返回值 	int
*@使用说明	
*/
static int Udp_Testing_Ota_Callback(const int type, const int file_size, const char* md5, const char* url, const char* version)
{
    
    return 0;
}  

/**
*@名称 		Entity_Generate_Test_Udp_Broadcast_Json_String
*@功能 		根据设备信息生成厂测UDP广播数据
*@参数 		unsigned short server_port UDP服务端监听端口
*@返回值 	char *
*@使用说明	
*/
char *Entity_Generate_Test_Udp_Broadcast_Json_String(unsigned short server_port)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    if(!root || !data)
        return NULL;

    cJSON_AddStringToObject(root, "type", TEST_MSG_TYPE_BROADCAST);
    unsigned int stamp = Entity_Get_Time_Stamp();
    char timestamp_str[20] = {0}; 
    snprintf(timestamp_str, sizeof(timestamp_str), "%d", stamp);
    cJSON_AddStringToObject(root, "msgId", timestamp_str);
    cJSON_AddNumberToObject(root, "ts", stamp);
    cJSON_AddItemToObject(root, "data", data);

    Entity_Device_Info_t *dev_info = Entity_Get_Dev_Info();
    cJSON_AddStringToObject(data, "pid", dev_info->Pid);
    cJSON_AddStringToObject(data, "deviceId", Get_Device_Id_String());
    cJSON_AddStringToObject(data, "version", dev_info->Dev_Version);
    cJSON_AddStringToObject(data, "mcuVersion", dev_info->Sub_Version);
    cJSON_AddNumberToObject(data, "udpPort", server_port);//UDP服务端监听的端口
    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_str;
}

/**
*@名称 		Entity_Generate_Test_Response_Json_String
*@功能 		生成厂测UDP服务监听应答数据
*@参数 		const char* type, const char* msgId, const int code, const char* result_msg
*@返回值 	char *
*@使用说明	
*/
char* Entity_Generate_Test_Response_Json_String(const char* type, const char* msgId, const int code, const char* result_msg)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    if(!root || !data)
        return NULL;

    unsigned int stamp = Entity_Get_Time_Stamp();
    
    cJSON_AddStringToObject(root, "type", type);
    cJSON_AddStringToObject(root, "msgId", msgId);
    cJSON_AddNumberToObject(root, "ts", stamp);
    cJSON_AddItemToObject(root, "data", data);

    cJSON_AddStringToObject(data, "deviceId", Get_Device_Id_String());
    cJSON_AddNumberToObject(data, "code", code);
    cJSON_AddStringToObject(data, "msg", result_msg);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    return json_str;
}

/**
*@名称 		Entity_Generate_Test_Ota_Report_Json_String
*@功能 		生成厂测UDP服务监听OTA进度上报数据
*@参数 		const int firmware_type, const char* version, const char* state, const int percent, const int code, const char* msg
*@返回值 	char *
*@使用说明	
*/
char* Entity_Generate_Test_Ota_Report_Json_String(const int firmware_type, const char* version, const char* state, const int percent, const int code, const char* msg)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    if(!root || !data)
        return NULL;
    cJSON_AddStringToObject(root, "type", TEST_MSG_TYPE_OTA_REPORT);

    unsigned int stamp = Entity_Get_Time_Stamp();
    char timestamp_str[20] = {0}; 
    snprintf(timestamp_str, sizeof(timestamp_str), "%d", stamp);

    cJSON_AddStringToObject(root, "msgId", timestamp_str);
    cJSON_AddNumberToObject(root, "ts", stamp);
    cJSON_AddItemToObject(root, "data", data);

    cJSON_AddStringToObject(data, "deviceId", Get_Device_Id_String());
    cJSON_AddNumberToObject(data, "firmwareType", firmware_type);
    cJSON_AddStringToObject(data, "version", version);
    cJSON_AddStringToObject(data, "state", state);
    cJSON_AddNumberToObject(data, "percent", percent);
    cJSON_AddNumberToObject(data, "code", code);
    cJSON_AddStringToObject(data, "msg", msg);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    return json_str;
}


/**
*@名称 		Factory_Test_Udp_Message_Parse
*@功能 		厂测UDP服务监听接收数据解析
*@参数 		const char *json_str
*@返回值 	int
*@使用说明	
*/
int Factory_Test_Udp_Message_Parse(const char *json_str)
{
    int result = 0;
    ENTITY_LOGD("recv msg data:%s\n", json_str);
    cJSON *root = cJSON_Parse(json_str);
    if(NULL == root)
    {
        ENTITY_LOGE("%s Error parsing JSON\r\n", __func__);
        result = -1;
        goto exit;
    }
    cJSON *type = cJSON_GetObjectItem(root, "type");
    if(NULL == type)
    {
        ENTITY_LOGE("json not found type item\r\n");
        result = -1;
        goto exit;
    }
    if(0 == strcmp(type->valuestring, TEST_MSG_TYPE_SET))//设置厂测MQTT数据
    {
        cJSON *msgId = cJSON_GetObjectItem(root, "msgId");
        cJSON *data = cJSON_GetObjectItem(root, "data");
        cJSON *mq = cJSON_GetObjectItem(data, "mq");
        cJSON *port = cJSON_GetObjectItem(data, "port");

        if((NULL == mq) || (NULL == port) || (NULL == msgId))
        {
            ENTITY_LOGE("json not found mq or port item\r\n");
            result = -1;
            goto exit;
        }
        ENTITY_LOGD("recv msg type: %s, mq: %s, port: %d", type->valuestring, mq->valuestring, port->valueint);
        
        char* response_str = Entity_Generate_Test_Response_Json_String(TEST_MSG_TYPE_SET, msgId->valuestring, 0, "success");
        unsigned int sendlen = -1;
        if(Udp_Server_Task_Info->Send_Bytes)
            sendlen = Udp_Server_Task_Info->Send_Bytes(response_str, strlen(response_str));
        FREE_MEMORY(response_str);
        if (-1 == sendlen)
        {
            ENTITY_LOGE("Failed to send lan udp response:%s\r\n", strerror(errno));
        } 
        else 
        {
            ENTITY_LOGD("send udp response ok.\r\n");
            if(Factory_Test_Udp_Cbs.Thing_Testing_Set_Callback)
                Factory_Test_Udp_Cbs.Thing_Testing_Set_Callback(mq->valuestring, port->valueint);
        }
    }
    else if(0 == strcmp(type->valuestring, TEST_MSG_TYPE_OTA))//OTA测试
    {
        cJSON *msgId = cJSON_GetObjectItem(root, "msgId");
        cJSON *data = cJSON_GetObjectItem(root, "data");

        cJSON *firmwareType = cJSON_GetObjectItem(data, "firmwareType");
        cJSON *file_size = cJSON_GetObjectItem(data, "file_size");
        cJSON *md5sum = cJSON_GetObjectItem(data, "md5sum");
        cJSON *url = cJSON_GetObjectItem(data, "url");
        cJSON *version = cJSON_GetObjectItem(data, "version");

        if((NULL == firmwareType) || (NULL == file_size) || (NULL == md5sum) || (NULL == url) || (NULL == version))
        {
            ENTITY_LOGE("json not found item\r\n");
            result = -1;
            goto exit;
        }

        ENTITY_LOGD("recv msg type:%s, firmwareType:%d, file_size:%d, md5sum:%s, url:%s, version:%s\r\n", 
                                type->valuestring, firmwareType->valueint, file_size->valueint, 
                                md5sum->valuestring, url->valuestring, version->valuestring);

        char* response_str = Entity_Generate_Test_Response_Json_String(TEST_MSG_TYPE_OTA_RESPONSE, msgId->valuestring, 0, "success");
        unsigned int sendlen = -1;
        if(Udp_Server_Task_Info->Send_Bytes)
            sendlen = Udp_Server_Task_Info->Send_Bytes(response_str, strlen(response_str));
        FREE_MEMORY(response_str);
        if (-1 == sendlen)
        {
            ENTITY_LOGE("Failed to send lan udp response:%s\r\n", strerror(errno));
        } 
        else 
        {
            ENTITY_LOGD("send udp response ok.\r\n");
            if(Factory_Test_Udp_Cbs.Thing_Testing_Ota_Callback) 
            {
                Factory_Test_Udp_Cbs.Thing_Testing_Ota_Callback(firmwareType->valueint, file_size->valueint, md5sum->valuestring, url->valuestring, version->valuestring);
            }
        }
    }
    else if(0 == strcmp(type->valuestring, TEST_MSG_TYPE_BURN))//烧录三元组
    {
        cJSON *cjson_id = cJSON_GetObjectItem(root, "msgId");
        cJSON *data_json = cJSON_GetObjectItem(root, "data");
        cJSON *uuid_json = cJSON_GetObjectItem(data_json, "uuid");
        cJSON *secret_json = cJSON_GetObjectItem(data_json, "secret");
        cJSON *mac_json = cJSON_GetObjectItem(data_json, "mac");
        cJSON *pid_json = cJSON_GetObjectItem(data_json, "pid");

         if((NULL == cjson_id) || (NULL == data_json) || (NULL == uuid_json) || (NULL == secret_json) \
            || (NULL == mac_json) || (NULL == pid_json))
        {
            ENTITY_LOGE("json not found item\r\n");
            result = -1;
            goto exit;
        }
     
        //apsid 和 appwd 是可选项
        cJSON *apsid_json = cJSON_GetObjectItem(data_json, "apsid");
        cJSON *appwd_json = cJSON_GetObjectItem(data_json, "appwd");

        if(Factory_Test_Udp_Cbs.Thing_Testing_Burn_Callback)
        {
            Factory_Test_Burn_Info_t info=
            {
                .Uuid = uuid_json->valuestring,
                .Secret = secret_json->valuestring,
                .Mac = mac_json->valuestring,
                .Pid = pid_json->valuestring,
                .Ap_Ssid = (apsid_json && apsid_json->valuestring) ? apsid_json->valuestring : "",
                .Ap_Pwd = (appwd_json && appwd_json->valuestring) ? appwd_json->valuestring : "",
                .Is_Mqtt_Data = 0,
            };
            Factory_Test_Udp_Cbs.Thing_Testing_Burn_Callback(cjson_id->valuestring, &info);
        }
    }
    else
    {
        ENTITY_LOGD("unknown msg type:%s\r\n", type->valuestring);
        result = -1;
    }

exit:
    cJSON_Delete(root);
    return result;
}



/**
*@名称 		Entity_Factory_Test_Init
*@功能 		厂测初始化
*@参数 		void
*@返回值 	int
*@使用说明	
*/
int Entity_Factory_Test_Init(void)
{
    if(Flag_Factory_Test_Start)
    {
        ENTITY_LOGI("%s have done\r\n", __func__);
        return 0;
    }
    ENTITY_LOGI("%s\r\n", __func__);    
    Factory_Test_Udp_Cbs.Thing_Testing_Set_Callback = Udp_Testing_Set_Callback;
    Factory_Test_Udp_Cbs.Thing_Testing_Ota_Callback = Udp_Testing_Ota_Callback;
   
    Factory_Test_Udp_Cbs.Thing_Testing_Burn_Callback = Factory_Set_Burn_Info_Callback; //支持UDP直接设置三元组
    

    if(Udp_Boradcast_Task_Info == NULL)
        Udp_Boradcast_Task_Info = (Udp_Boradcast_Task_Info_t*)Entity_Mem_Calloc(1, sizeof(Udp_Boradcast_Task_Info_t));
    if(Udp_Server_Task_Info == NULL)
        Udp_Server_Task_Info = (Udp_Server_Task_Info_t*)Entity_Mem_Calloc(1, sizeof(Udp_Server_Task_Info_t));
    
    if(!Udp_Boradcast_Task_Info || !Udp_Server_Task_Info)
    {
        ENTITY_LOGE("%s malloc faild\r\n", __func__);
        return -1;
    }
    //UDP广播
    Udp_Boradcast_Task_Info->Boardcast_Addr = INADDR_BROADCAST;
    Udp_Boradcast_Task_Info->Boardcast_Port = TEST_UDP_BROADCAST_PORT;
    char *context = Entity_Generate_Test_Udp_Broadcast_Json_String(TEST_UDP_SERVER_PORT);
    if(context == NULL)
        return -1;
    unsigned int context_len = strlen(context);
    Udp_Boradcast_Task_Info->Context = Entity_Mem_Calloc(1, context_len+1);
    if(Udp_Boradcast_Task_Info->Context == NULL)
        return -1;
    strcpy(Udp_Boradcast_Task_Info->Context, context);
    Udp_Boradcast_Task_Info->Context_Len = context_len;
    if(Entity_Udp_Broadcast_Task_Start(Udp_Boradcast_Task_Info) != 0)
        return -1;

    //UDP服务端监听
    Udp_Server_Task_Info->Server_Port = TEST_UDP_SERVER_PORT;
    Udp_Server_Task_Info->Recv_Callback = Factory_Test_Udp_Message_Parse;
    if(Entity_Udp_Server_Task_Start(Udp_Server_Task_Info) != 0)
    {
        Entity_Udp_Broadcast_Task_Stop(Udp_Boradcast_Task_Info);
        return -1;
    }
        
    Flag_Factory_Test_Start = 1;
    return 0;
}



/**
*@名称 		Entity_Factory_Test_Deinit
*@功能 		厂测反初始化
*@参数 		void
*@返回值 	int
*@使用说明	
*/
int Entity_Factory_Test_Deinit(void)
{
    Entity_Udp_Broadcast_Task_Stop(Udp_Boradcast_Task_Info);
    Entity_Udp_Server_Task_Stop(Udp_Server_Task_Info);
    Factory_Test_Mqtt_Client_Task_Stop();
    Entity_Wifi_Sta_Disconnect();
    return 0;
}

static unsigned char Flag_Check_Button_Start;
static void *button_thread_handle= NULL;
/**
*@名称 		Factory_Test_Start_Check_Button_Task
*@功能 		任务
*@参数 		void *arg
*@返回值 	void
*@使用说明	
*/
static void Factory_Test_Start_Check_Button_Task(void *arg)
{
    unsigned char have_check_num=0;
    unsigned char key_num = Entity_Get_Key_Num();
    ENTITY_LOGI("%s key_num:%d\r\n", __func__, key_num);
    
    while(1)
    {
        if(Entity_Get_Key_Value(have_check_num))
        {
            Entity_Sleep_Ms(10);
            if(Entity_Get_Key_Value(have_check_num))
            {
                have_check_num++;
                ENTITY_LOGI("%s have check key num:%d\r\n", __func__, have_check_num);
                if(have_check_num >= key_num)
                {
                    break;
                }
            }
        }
        Entity_Sleep_Ms(100);
    }
    Entity_Factory_Test_Init();//启动厂测
    ENTITY_LOGI("%s task exit\r\n", __func__);
    Entity_Pthread_Delete(&button_thread_handle);
}

/**
*@名称 		Factory_Test_Start_Check_Button
*@功能 		测试按键
*@参数 		void
*@返回值 	void
*@使用说明	
*/
void Factory_Test_Start_Check_Button(void)
{
    ENTITY_LOGI("%s\r\n", __func__);
    int ret;
    if(Flag_Check_Button_Start)
    {
        ENTITY_LOGI("%s check button task is runing \r\n", __func__);
        return ;
    }
    ret = Entity_Pthread_Create(&button_thread_handle, "check button task", 4*1024, 4, Factory_Test_Start_Check_Button_Task, NULL);
    if(ret != 0) 
    {
        ENTITY_LOGE(" %s task Create Failed!", __func__);
        return ;
    }
    ENTITY_LOGI(" %s task Create success!", __func__);
}


/**
*@名称 		Dynamic_Register_Set_Burn_Info_Callback
*@功能 		设置烧录试信息回调
*@参数 		char *msg_id, void *info
*@返回值 	void
*@使用说明	
*/
static void Dynamic_Register_Set_Burn_Info_Callback(char *msg_id, void *info)
{
    Factory_Test_Burn_Info_t *burn_info = (Factory_Test_Burn_Info_t*)info;
    char secret_mask[32];
    char ap_pwd_mask[32];
    Utils_Mask_Secret(burn_info->Secret, secret_mask, sizeof(secret_mask));
    Utils_Mask_Secret(burn_info->Ap_Pwd, ap_pwd_mask, sizeof(ap_pwd_mask));
    ENTITY_LOGI("is_mqtt_data:%d, pid: %s, uuid: %s, seceret_mask:%s, mac:%s, apsid:%s, appwd_mask:%s", burn_info->Is_Mqtt_Data, burn_info->Pid, burn_info->Uuid, secret_mask, \
                            burn_info->Mac, burn_info->Ap_Ssid, ap_pwd_mask);

    Entity_Device_Info_t *dev_info = Entity_Get_Dev_Info();                        
    if(strcmp(dev_info->Pid, burn_info->Pid) != 0)
    {
        ENTITY_LOGE("pid is not the same, local_pid:%s, info_pid:%s", dev_info->Pid, burn_info->Pid); 
        return;
    }
    ENTITY_LOGI(" %s set triple success!", __func__);
    //获取三元组指针并设置参数，保存到FLASH
    Entity_Triple_Info_t *triple_info = Entity_Get_Triple_Info();
    strncpy(triple_info->Uuid, burn_info->Uuid, sizeof(triple_info->Uuid)-1);
    strncpy(triple_info->Secret, burn_info->Secret, sizeof(triple_info->Secret)-1);
    strncpy(triple_info->Mac, burn_info->Mac, sizeof(triple_info->Mac)-1);
    Entity_Save_Triple_Info_To_Flash();
    Flag_Burn = 1;
    Register_Test_Mqtt_Client_Task_Stop();
    Entity_Wifi_Sta_Disconnect();
    Entity_System_Reset();//厂测完成是否立即重启
}

/**
*@名称 		Factory_Test_Wifi_Event_Callback
*@功能 		WIFI事件回调
*@参数 		void	
*@返回值 	void
*@使用说明	
*/
void Factory_Test_Wifi_Event_Callback(int event)
{
    switch (event) 
    {
        case ENTITY_WIFI_EVT_SCAN_DONE://扫描完成事件
            ENTITY_LOGI("[%s]WiFi: Scan done\r\n", __func__);
            break;
        case ENTITY_WIFI_EVT_STA_CONNECTED://WIFI已连接事件
            ENTITY_LOGI("\r\n-------------------------------[%s]WiFi: Connected---------------------------------\r\n\r\n", __func__);
            break;
        case ENTITY_WIFI_EVT_STA_DISCONNECTED://WIFI已断连
            ENTITY_LOGI("\r\n------------------------------[%s]WiFi: Disconnected-------------------------------\r\n\r\n", __func__);
            break;
        case ENTITY_WIFI_EVT_WPS_TIMEOUT://超时事件
            ENTITY_LOGI("[%s]WiFi: wps is timeout\r\n", __func__);
            break;
        case ENTITY_WIFI_EVT_IP_CHANGE://IP地址更新
            ENTITY_LOGI("\r\n------------------------------[%s]WiFi: IP changed----------------------------------\r\n\r\n", __func__);
            if(Factory_Register_Type == DYNAMIC_REGISTER_TYPE)
            {
                Register_Mqtt_Msg_Cbs_t Register_Mqtt_Msg_Cbs=
                {
                    .Set_Burn_Info_Callback = Dynamic_Register_Set_Burn_Info_Callback,
                };
                Register_Test_Mqtt_Client_Task_Start(DEFAULT_MQTT_HOST, DEFAULT_MQTT_PORT, &Register_Mqtt_Msg_Cbs);
            }
            else
            {
                Factory_Test_Start_Check_Button();//轮流按下设备的按键后进入厂测流程
            }
            break;
        default:
            break;
    }
}

/**
*@名称 		Factory_Test_Wifi_Config
*@功能 		启动厂测默认WIFI
*@参数 		void	
*@返回值 	void
*@使用说明	
*/
void Factory_Test_Wifi_Config(void)
{
    ENTITY_LOGI("%s\r\n", __FUNCTION__);
    //Entity_Wifi_Init(ENTITY_WIFI_STA_MODE);//WIFI初始化
    Entity_Wifi_Cbs_t *wifi_cbs = Get_Entity_Wifi_Cbs();
    if(wifi_cbs->Register_Wifi_Event_App_Cb)
        wifi_cbs->Register_Wifi_Event_App_Cb(Factory_Test_Wifi_Event_Callback);
    //Entity_Wifi_Config_Net_Info(FACTORY_TEST_WIFI_SSID, FACTORY_TEST_WIFI_PWD);
    Entity_Wifi_Config_Net_Info(Factory_Test_Wifi_Ssid, FACTORY_REGISTER_WIFI_PWD);
    Entity_Wifi_Sta_Conncet();
    Entity_Set_Dev_Status(DEV_ENTER_FACTORY_TEST_STATE);//进入厂测状态
}


/**
*@名称 		Factory_Wifi_Scan_Result_Process
*@功能 	    WIFI扫描结果处理
*@参数 		void *result
*@返回值 	void 
*@使用说明	
*/
void Factory_Wifi_Scan_Result_Process(void *result)
{
    Entity_Wifi_Sta_Scan_Result_t *scan_result = (Entity_Wifi_Sta_Scan_Result_t*)result;
    uint32_t scan_num;

    if (scan_result == NULL)
    {
        ENTITY_LOGE("[WiFi扫描] 失败: 厂测扫描结果为空\r\n");
        return;
    }

    scan_num = scan_result->Num;
    if (scan_num > ENTITY_WIFI_SCAN_AP_NUM_MAX)
    {
        ENTITY_LOGE("[WiFi扫描] 失败: 厂测扫描AP数量越界, num:%u max:%u\r\n",
                  scan_num,
                  (unsigned int)ENTITY_WIFI_SCAN_AP_NUM_MAX);
        scan_num = ENTITY_WIFI_SCAN_AP_NUM_MAX;
    }

    ENTITY_LOGI("[WiFi扫描] 入口: 厂测处理扫描结果, AP数量:%u\r\n", scan_num);
    for (int i = 0; i < (int)scan_num; i++)
    {
        ENTITY_LOGI("ap[%d]--->ssid:%s, rssi:%d, security:%d\r\n", i, scan_result->Ap_Infos[i].Ssid, \
                    scan_result->Ap_Infos[i].Rssi, scan_result->Ap_Infos[i].Auth == ENTITY_WIFI_SECURITY_OPEN ? 0 : 1);

        if(scan_result->Ap_Infos[i].Rssi >= FACTORY_VAILD_WIFI_RSSI)//必须达到信号强度要求
        { 
             //扫描到动态注册的热点    
            if(strncmp(scan_result->Ap_Infos[i].Ssid, FACTORY_DYNAMIC_REGISTER_WIFI_SSID, strlen(FACTORY_DYNAMIC_REGISTER_WIFI_SSID)) == 0) 
            {
                if(Get_Entity_Dev_State()==DEV_UNAUTHORIZED_STATE)//设备在未授权三元组状态才能动态注册     
                {
                    Factory_Register_Type = DYNAMIC_REGISTER_TYPE;
                    snprintf(Factory_Test_Wifi_Ssid, sizeof(Factory_Test_Wifi_Ssid)-1, "%s", scan_result->Ap_Infos[i].Ssid);
                    Flag_Wifi_Scan_Success = 1;
                    ENTITY_LOGI("[WiFi扫描] 完成: 厂测扫描找到动态注册热点, ssid:%s\r\n", Factory_Test_Wifi_Ssid);
                    return;
                }
            }
            //扫描到正常厂测授权的热点 
            else if(strncmp(scan_result->Ap_Infos[i].Ssid, FACTORY_NORMAL_REGISTER_WIFI_SSID, strlen(FACTORY_NORMAL_REGISTER_WIFI_SSID)) == 0)
            {
                Factory_Register_Type = NORMAL_REGISTER_TYPE;
                snprintf(Factory_Test_Wifi_Ssid, sizeof(Factory_Test_Wifi_Ssid)-1, "%s", scan_result->Ap_Infos[i].Ssid);
                Flag_Wifi_Scan_Success = 1;
                ENTITY_LOGI("[WiFi扫描] 完成: 厂测扫描找到正常授权热点, ssid:%s\r\n", Factory_Test_Wifi_Ssid);
                return;
            }
        }
    }

    ENTITY_LOGI("[WiFi扫描] 完成: 厂测扫描未找到可用授权热点, AP数量:%u\r\n", scan_num);
}

/**
*@名称 		Factory_Test_Auth_Check_Task
*@功能 		获取三元组任务
*@参数 		void *arg
*@返回值 	void *
*@使用说明	
*/
void *Factory_Test_Auth_Check_Task(void *arg)
{
    Entity_Wifi_Init(ENTITY_WIFI_STA_MODE);//WIFI初始化
    while(1)
    {
        if(Flag_Wifi_Scan_Success)
        {
            Factory_Test_Wifi_Config();
            goto exit;
        }
        Entity_Wifi_Sta_Scan_Start(Factory_Wifi_Scan_Result_Process);//启动WIFI扫描
        Entity_Sleep_Ms(3000);
    }
exit:
    Entity_Pthread_Delete(&Factory_Test_Auth_Check_Thread_Id);
    return NULL;
}

/**
*@名称 		Factory_Test_Auth_Check_Task_Start
*@功能 		启动任务扫描WIFI热点，决定是进入动态授权还是厂测模式
*@参数 		void	
*@返回值 	void
*@使用说明	
*/
void Factory_Test_Auth_Check_Task_Start(void)
{
    int ret;
    if(Flag_Factory_Test_Auth_Check_Runing == 0)
    {
        ret = Entity_Pthread_Create(&Factory_Test_Auth_Check_Thread_Id, "factory auth check task", FACTORY_TEST_AUTH_CHECK_TASK_STACK_SIZE, FACTORY_TEST_AUTH_CHECK_TASK_PROI, Factory_Test_Auth_Check_Task, NULL);
        if(ret != 0) 
        {
            ENTITY_LOGE(" %s task Create Failed!", __func__);
            return ;
        }
        ENTITY_LOGI(" %s task Create success!", __func__);
        Flag_Factory_Test_Auth_Check_Runing = 1;
    }
}

/**
*@名称 		Factory_Test_Auth_Check_Task_Stop
*@功能 		停止任务扫描WIFI热点
*@参数 		void	
*@返回值 	void
*@使用说明	
*/
void Factory_Test_Auth_Check_Task_Stop(void)
{
    if(Flag_Factory_Test_Auth_Check_Runing)
    {
        Entity_Pthread_Delete(&Factory_Test_Auth_Check_Thread_Id);
        Flag_Factory_Test_Auth_Check_Runing = 0;
    }
}
