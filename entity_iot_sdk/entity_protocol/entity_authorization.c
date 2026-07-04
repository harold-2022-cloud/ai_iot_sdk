//entity_authorization.c
#include "entity_authorization.h"

#include "entity_iot_func.h"
#include "entity_log.h"
#include "com_utils.h"
#include "cJSON.h"
#include "entity_dev_info.h"
#include "entity_wifi.h"
#include "com_crc.h"

#if (ENTITY_AUTH_INTERFACE_USE==ENTITY_AUTH_INTERFACE_UART)
#include "entity_uart.h"
#endif

#ifdef AUTH_REC_DATA_USE_MBEDTLS_AES
#include "com_mbedtls.h"
#endif

#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>

#define ENTITY_AUTH_TASK_PROI           3
#define ENTITY_AUTH_TASK_STACK_SIZE     8192
static void * Auth_Thread_Id;

#ifdef AUTH_REC_DATA_USE_MBEDTLS_AES
ENTITY_PSRAM_BSS static unsigned char Aes_Dec_Buf[AUTH_RX_BUF_SIZE];// 数据解密空间
static unsigned int Aes_Dec_Len;
#endif

static Entity_Auth_Cbs_t *Entity_Auth_Cbs=NULL;

/**
*@名称 		Register_Entity_Auth_Cbs
*@功能 		注册授权相关操作接口
*@参数 		Entity_Auth_Cbs_t *cbs
*@返回值 	void
*@使用说明	
*/
void Register_Entity_Auth_Cbs(Entity_Auth_Cbs_t *cbs)
{
    Entity_Auth_Cbs = cbs;
}

/**
*@名称 		Get_Auth_Register_Unique
*@功能 		获取授权唯一码
*@参数 		cJSON *data
*@返回值 	void
*@使用说明	
*/
char *Get_Auth_Register_Unique(cJSON *data)
{
    if (data == NULL)
    {
        ENTITY_LOGE("[%s][%d]get_register_unique err!", __func__, __LINE__);
        return NULL;
    }
    const unsigned char unique_code_len = 50;
    char *unique_code = (char *)Entity_Mem_Malloc(unique_code_len);
    if (unique_code == NULL)
    {
        ENTITY_LOGE("[%s][%d]unique_code malloc err!", __func__, __LINE__);
        return NULL;
    }
    memset(unique_code, 0, unique_code_len);

    cJSON *regis_status_item = cJSON_GetObjectItem(data, "regis_status");
    if (!regis_status_item)
    {
        ENTITY_LOGE("[%s][%d] regis_status missing", __func__, __LINE__);
        Entity_Mem_Free(unique_code);
        return NULL;
    }
    int regis_status = regis_status_item->valueint;
    if (regis_status)
    {
        cJSON *regis_code_item = cJSON_GetObjectItem(data, "regisCode");
        if (!regis_code_item || !cJSON_IsString(regis_code_item) || !regis_code_item->valuestring)
        {
            ENTITY_LOGE("[%s][%d] regisCode missing or invalid", __func__, __LINE__);
            Entity_Mem_Free(unique_code);
            return NULL;
        }
        strncpy(unique_code, regis_code_item->valuestring, unique_code_len - 1);
        unique_code[unique_code_len - 1] = '\0';
    }
    else
    {
        unsigned char mac_buf[6] = {0};
        Entity_Get_Wifi_Mac(mac_buf);
        Hex_Array_To_String(mac_buf, sizeof(mac_buf), unique_code);
    }
    return unique_code;
}

/**
*@名称 		Get_Auth_Start_Json_Str
*@功能 		获取启动授权CJOSN字符串
*@参数 		char *unique_code, char *pid
*@返回值 	char *
*@使用说明	
*/
char *Get_Auth_Start_Json_Str(char *unique_code, char *pid)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL)
    {
        ENTITY_LOGE("[%s][%d]cJSON_CreateObject err!", __func__, __LINE__);
        return NULL;
    }
    cJSON_AddStringToObject(root, "auth_status", "start");
    cJSON_AddStringToObject(root, "unique", unique_code);
    cJSON_AddStringToObject(root, "pid", pid);
    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}

/**
*@名称 		Get_Auth_Start_Json_Str
*@功能 		获取数据传输CJOSN字符串
*@参数 		unsigned char status
*@返回值 	char *
*@使用说明	
*/
char *Get_Auth_Transfer_Json_Str(unsigned char status)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL)
    {
        ENTITY_LOGE("[%s][%d]cJSON_CreateObject err!", __func__, __LINE__);
        return NULL;
    }

    cJSON_AddStringToObject(root, "auth_status", "transfer");
    cJSON_AddStringToObject(root, "result", status ? "ok" : "error");

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    return json;
}

/**
*@名称 		Entity_Auth_Recv_Data_Process
*@功能 		设备授权接收数据处理
*@参数 		unsigned char *payload, unsigned short payload_len, unsigned cmd_type
*@返回值 	void
*@使用说明	
*/
void Entity_Auth_Recv_Data_Process(unsigned char *payload, unsigned short payload_len, unsigned cmd_type)
{
    static char *unique_code = NULL;
    if(cmd_type != ENTITY_AUTH_CMD)
        return;
    char auth_str[ENTITY_AUTH_DATA_LEN_MAX] = {0};
    Hex_Array_Append_To_String(auth_str, payload, payload_len);
    //转换为JSON数据
    cJSON *root = cJSON_Parse(auth_str);
    if (root == NULL)
    {
        ENTITY_LOGE("[%s][%d]cJSON_Parse err!", __func__, __LINE__);
        return;
    }
    //获取授权状态字段
    cJSON *auth_status_item = cJSON_GetObjectItem(root, "auth_status");
    if (!auth_status_item || !cJSON_IsString(auth_status_item) || !auth_status_item->valuestring)
    {
        ENTITY_LOGE("[%s][%d] auth_status missing or invalid", __func__, __LINE__);
        cJSON_Delete(root);
        return;
    }
    char *auth_status = auth_status_item->valuestring;

    Entity_Device_Info_t *dev_info = Entity_Get_Dev_Info();
    Entity_Triple_Info_t *triple_info = Entity_Get_Triple_Info();
    //启动传输
    if (0 == strcmp(auth_status, "start"))
    {
        unique_code = Get_Auth_Register_Unique(root);
        if (unique_code == NULL)
        {
            cJSON_Delete(root);
            ENTITY_LOGE("[%s][%d]unique_code err!", __func__, __LINE__);
            return;
        }
        
        char *json = Get_Auth_Start_Json_Str(unique_code, dev_info->Pid);
        if (json == NULL)
        {
            cJSON_Delete(root);
            Entity_Mem_Free(unique_code);
            ENTITY_LOGE("[%s][%d]json err!", __func__, __LINE__);
            return;
        }
        if(Entity_Auth_Cbs->Write_Bytes)
            Entity_Auth_Cbs->Write_Bytes((void *)json, strlen(json));

        Entity_Mem_Free(json);
    }
    else if (0 == strcmp(auth_status, "transfer"))//数据传输
    {
        cJSON *uuid_item   = cJSON_GetObjectItem(root, "uuid");
        cJSON *secret_item = cJSON_GetObjectItem(root, "secret");
        cJSON *mac_item    = cJSON_GetObjectItem(root, "mac");
        if (!uuid_item   || !cJSON_IsString(uuid_item)   || !uuid_item->valuestring   ||
            !secret_item || !cJSON_IsString(secret_item) || !secret_item->valuestring ||
            !mac_item    || !cJSON_IsString(mac_item)    || !mac_item->valuestring)
        {
            ENTITY_LOGE("[%s][%d] uuid/secret/mac missing or invalid", __func__, __LINE__);
            cJSON_Delete(root);
            return;
        }
        char *uuid   = uuid_item->valuestring;
        char *secret = secret_item->valuestring;
        char *mac    = mac_item->valuestring;
        if (strlen(uuid)   >= ENTITY_DEVICE_ID_MAX_SIZE     ||
            strlen(secret) >= ENTITY_DEVICE_SECRET_MAX_SIZE ||
            strlen(mac)    >= ENTITY_MAC_MAX_SIZE)
        {
            ENTITY_LOGE("[%s][%d] uuid/secret/mac too long", __func__, __LINE__);
            cJSON_Delete(root);
            return;
        }
        unsigned char update_result = true;

        char secret_mask[32];
        Utils_Mask_Secret(secret, secret_mask, sizeof(secret_mask));
        ENTITY_LOGI("Uuid:[%s] Mac:[%s] SecretMask[%s]", uuid, mac, secret_mask);
        Entity_Triple_Info_t triple_write = {0};

        strncpy(triple_write.Uuid,          uuid,          ENTITY_DEVICE_ID_MAX_SIZE - 1);
        triple_write.Uuid[ENTITY_DEVICE_ID_MAX_SIZE - 1] = '\0';
        strncpy(triple_write.Secret,        secret,        ENTITY_DEVICE_SECRET_MAX_SIZE - 1);
        triple_write.Secret[ENTITY_DEVICE_SECRET_MAX_SIZE - 1] = '\0';
        strncpy(triple_write.Mac,           mac,           ENTITY_MAC_MAX_SIZE - 1);
        triple_write.Mac[ENTITY_MAC_MAX_SIZE - 1] = '\0';
        strncpy(triple_write.Register_Code, unique_code,   ENTITY_AUTH_REGISTER_CODE_SIZE_MAX - 1);
        triple_write.Register_Code[ENTITY_AUTH_REGISTER_CODE_SIZE_MAX - 1] = '\0';
        memcpy(triple_write.Pid,            dev_info->Pid, ENTITY_PRODUCT_ID_MAX_SIZE - 1);
        triple_write.Pid[ENTITY_PRODUCT_ID_MAX_SIZE - 1] = '\0';
        memcpy(triple_info, &triple_write, sizeof(Entity_Triple_Info_t));
        Entity_Save_Triple_Info_To_Flash();
        Entity_Read_Triple_Info_From_Flash();

        if (memcmp(triple_info, &triple_write, sizeof(Entity_Triple_Info_t)) != 0)
        {
            ENTITY_LOGE("[%s][%d]set flash err!", __func__, __LINE__);
            update_result = false;
        }
        Utils_Mask_Secret(triple_info->Secret, secret_mask, sizeof(secret_mask));
        ENTITY_LOGI("DEV_PARAM_GET Uuid:[%s] Mac:[%s] SecretMask[%s]", triple_info->Uuid, triple_info->Mac, secret_mask);

        char *json = Get_Auth_Transfer_Json_Str(update_result);
        if (json == NULL)
        {
            cJSON_Delete(root);
            ENTITY_LOGE("[%s][%d]json err!", __func__, __LINE__);
            return;
        }
        if(Entity_Auth_Cbs->Write_Bytes)
            Entity_Auth_Cbs->Write_Bytes((void *)json, strlen(json));

        Entity_Mem_Free(json);
        Entity_Sleep_Ms(100);//
        Entity_System_Reset();//系统重启
    }
    cJSON_Delete(root);
}

/**
*@名称 		Entity_Auth_Protocol_Parse_Handle
*@功能 		设备授权协议解析
*@参数 		unsigned char *pdata, unsigned int len
*@返回值 	int
*@使用说明	
*/
int Entity_Auth_Protocol_Parse_Handle(unsigned char *pdata, unsigned int len)
{
    unsigned short offset = 0;
    unsigned short payload_len;
    unsigned char check_sum=0;
    unsigned char cmd_type;
    unsigned char *payload;
    while(len-offset >= AUTH_PROTOCOL_HEAD_LEN)//剩余长度要大于等于最小协议长度
    {
        //数据头1校验
        if(pdata[offset+AUTH_HEAD_FIRST_INDEX] != AUTH_HEAD_FIRST_VALUE)
        {
            offset++;
            continue;
        }
        //数据头2校验
        if(pdata[offset+AUTH_HEAD_SECOND_INDEX] != AUTH_HEAD_SECOND_VALUE)
        {
            offset++;
            continue;
        }
        //协议版本校验
        if(pdata[offset+AUTH_PROTOCOL_VERSION_INDEX] != AUTH_PROTOCOL_VERSION_VALUE)
        {
            offset += 2;
            continue;
        }
        //负载数据长度校验
        payload_len = pdata[offset+AUTH_LENGTH_HIGH_INDEX]*256+pdata[offset+AUTH_LENGTH_LOW_INDEX];
        if(payload_len+AUTH_PROTOCOL_HEAD_LEN > len)
        {
            offset += 3;
            continue;
        }
        //累加和校验
        check_sum = Check_Sum(pdata+offset, payload_len+AUTH_PROTOCOL_HEAD_LEN-1);
        if(check_sum != pdata[offset+payload_len+AUTH_PROTOCOL_HEAD_LEN-1])
        {
            ENTITY_LOGE("check sum error, crc:0x%x, but data:0x%x\r\n", pdata[offset+payload_len+AUTH_PROTOCOL_HEAD_LEN-1], check_sum);
            offset += 3;
            continue;
        }
        cmd_type = pdata[offset+AUTH_CMD_TYPE_INDEX];
        payload = pdata+offset+AUTH_DATA_START_INDEX;
        Entity_Auth_Recv_Data_Process(payload, payload_len, cmd_type);
        offset += payload_len+AUTH_PROTOCOL_HEAD_LEN;
    }
    return offset;
}

/**
*@名称 		Entity_Auth_Task
*@功能 		设备授权任务
*@参数 		void
*@返回值 	int
*@使用说明	
*/
void Entity_Auth_Task(void *arg)
{
    int ret = 0;

    while (1)
    {
        // 优先用信号量阻塞等待，无 Wait_Data 时退化为 10ms 轮询
        if (Entity_Auth_Cbs->Wait_Data)
            Entity_Auth_Cbs->Wait_Data(0);
        else
            Entity_Sleep_Ms(10);

        if(Entity_Auth_Cbs->Read_Bytes)
        {
            ret = Entity_Auth_Cbs->Read_Bytes(Entity_Auth_Cbs->Rx_Buf, &Entity_Auth_Cbs->Rx_Len);
            if(ret == 0)
            {
                ENTITY_LOGD("read len:%d\r\n", Entity_Auth_Cbs->Rx_Len);
                if (Entity_Auth_Cbs->Rx_Len > 0)
                {
                #ifdef AUTH_REC_DATA_USE_MBEDTLS_AES
                    Aes_Dec_Len = Mbedtls_Aes_Ecb(MBEDTLS_AES_DECRYPT, 0, (const unsigned char*)AUTH_TRANFER_AES_KEY, AUTH_TRANFER_AES_KEY_BITS, Entity_Auth_Cbs->Rx_Buf, Entity_Auth_Cbs->Rx_Len, Aes_Dec_Buf);
                    Entity_Log_Dump_Hex_To_String(ENTITY_LOG_LEVEL_DEBUG, "read data:", Aes_Dec_Buf, Aes_Dec_Len);
                    Entity_Auth_Protocol_Parse_Handle(Aes_Dec_Buf, Aes_Dec_Len);
                #else
                    Entity_Log_Dump_Hex_To_String(ENTITY_LOG_LEVEL_DEBUG, "read data:", Entity_Auth_Cbs->Rx_Buf, Entity_Auth_Cbs->Rx_Len);
                    Entity_Auth_Protocol_Parse_Handle(Entity_Auth_Cbs->Rx_Buf, Entity_Auth_Cbs->Rx_Len);
                #endif
                }
            }
        }
    }
    Entity_Pthread_Delete(&Auth_Thread_Id);
}

/**
*@名称 		Entity_Auth_Init
*@功能 		设备授权初始化
*@参数 		void
*@返回值 	int
*@使用说明	
*/
int Entity_Auth_Init(void)
{
    if(Entity_Auth_Cbs == NULL)
    {
#if (ENTITY_AUTH_INTERFACE_USE==ENTITY_AUTH_INTERFACE_UART)
        Entity_Uart_Auth_Init();
#elif (ENTITY_AUTH_INTERFACE_USE==ENTITY_AUTH_INTERFACE_UDP)
        Entity_Udp_Auth_Init();
#endif
        ENTITY_LOGD("%s, Entity_Auth_Cbs not init\r\n", __func__);
    }
    int ret = Entity_Pthread_Create(&Auth_Thread_Id, "Entity_Auth_Task", ENTITY_AUTH_TASK_STACK_SIZE, ENTITY_AUTH_TASK_PROI, Entity_Auth_Task, NULL);
    if(ret != 0) 
    {
        ENTITY_LOGE(" %s task Create Failed!", __func__);
        return -1;
    }
    ENTITY_LOGD(" %s task Create success!", __func__);
    return 0;
}

    







