//entity_http_ota.h
#pragma once


#include "entity_http_download.h"

#define OTA_BUF_LEN        4097

typedef enum {
    ENTITY_HTTP_OTA_STATE_UNINIT = 0, /* un-inited */
    ENTITY_HTTP_OTA_STATE_INITED,       /* inited */
    ENTITY_HTTP_OTA_STATE_FETCHING,     /* fetching */
    ENTITY_HTTP_OTA_STATE_FETCHED,      /* fetched */
    ENTITY_HTTP_OTA_STATE_DISCONNECTED  /* disconnected */
}Entity_Http_Ota_State_e; 

typedef enum 
{
    ENTITY_HTTP_OTA_ERR_NONE            = 0,
    ENTITY_HTTP_OTA_ERR_FAIL            = -1,
    ENTITY_HTTP_OTA_ERR_INVALID_PARAM   = -2,
    ENTITY_HTTP_OTA_ERR_INVALID_STATE   = -3,
    ENTITY_HTTP_OTA_ERR_MD5_MISMATCH    = -4,
    ENTITY_HTTP_OTA_ERR_STR_TOO_LONG    = ENTITY_HTTP_OTA_ERR_MD5_MISMATCH,
    ENTITY_HTTP_OTA_ERR_FETCH_FAILED    = -5,
    ENTITY_HTTP_OTA_ERR_FETCH_NOT_EXIST = -6,
    ENTITY_HTTP_OTA_ERR_FETCH_AUTH_FAIL = -7,
    ENTITY_HTTP_OTA_ERR_FETCH_TIMEOUT   = -8,
    ENTITY_HTTP_OTA_ERR_NOMEM           = -9,
    ENTITY_HTTP_OTA_ERR_OSC_FAILED      = -10,
    ENTITY_HTTP_OTA_ERR_REPORT_VERSION  = -11,
}Entity_Http_Ota_Error_Code_e;

//OTA回调
typedef struct 
{   
    int (*Ota_Start_Callback)(void* ota_info);
    void (*Ota_Progress_Percent_Report_Callback)(void* ota_info);
    int (*Ota_Fetch_Yield_Callback)(void* ota_info, unsigned char *buf, unsigned int len);
    void (*Ota_Failed_Callback)(void* ota_info);
    void (*Ota_Success_Callback)(void* ota_info);
    void (*Ota_End_Callback)(void* ota_info);
}Entity_Http_Ota_Cbs_t;

//OTA参数
typedef struct 
{
    char *Mssage_Id;
    char *New_Version;
    char *Url;
    char *Md5_Sum;          //md5值固定16字节，用字符表示就是32字节，此缓存多1字节，方便复制打印
    unsigned int File_Size;
    void *Private_Data;//私有数据
    unsigned int Private_Len;//私有数据长度
}Entity_Http_Ota_Param_t;

//OTA进度信息
typedef struct 
{
    void *Md5;      
    unsigned int Total_Down_Len;
    unsigned int Have_Down_Len;
    unsigned int Remain_Down_Len;
    unsigned int Current_Down_Len;
    unsigned int Content_Length;
    int Progress_Percent;
    int Last_Percent;
    int error_code;
}Entity_Http_Ota_Progress_t;

//OTA相关信息
typedef struct 
{
    Entity_Http_Ota_Param_t Ota_Param;
    Entity_Http_Ota_Cbs_t Ota_Cbs;
    Entity_Http_Ota_Progress_t Ota_Progress;
    Entity_Http_Ota_State_e Ota_State;
    Entity_Http_Download_t *Http_Download_Handle;
}Entity_Http_Ota_Info_t;


typedef struct 
{
    int (*Ota_Flash_Init)(void);
    int (*Ota_Flash_Process_Data)(unsigned char *buf, uint16_t len, uint32_t total);
    int (*Ota_Flash_Check_Crc)(uint32_t in_crc);
    int (*Ota_Flash_Deinit)(void);
    int (*Ota_Flash_Complete)(void);
}Entity_Ota_Flash_Func_t;


//获取OTA FLASH操作接口
void Entity_Ota_Flash_Func_Init(Entity_Ota_Flash_Func_t *func);

int Entity_Ota_Flash_Init(void);

int Entity_Ota_Flash_Process_Data(unsigned char *buf, uint16_t len, uint32_t total);

int Entity_Ota_Flash_Deinit(void);

int Entity_Ota_Flash_Complete(void);

//启动HTTP OTA任务
int Entity_Http_Ota_Task_Start(Entity_Http_Ota_Param_t *ota_param, Entity_Http_Ota_Cbs_t *ota_cbs);

//退出HTTP OTA任务
void Entity_Http_Ota_Task_Exit(void);

//等待退出HTTP OTA任务
int Wait_Entity_Http_Ota_Task_Exit(unsigned int timeout_ms);


unsigned char Entity_Http_Ota_Task_Runing(void);











