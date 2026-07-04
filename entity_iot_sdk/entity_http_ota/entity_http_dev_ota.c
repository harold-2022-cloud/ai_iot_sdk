//entity_http_dev_ota.c
#include "entity_http_dev_ota.h"

#include "entity_http_ota.h"
#include "entity_mqtt_event_report.h"
#include "entity_log.h"
#include "entity_iot_func.h"
#include "entity_param_check.h"
#include "entity_error_code.h"
#include "entity_dev_info.h"

/**
*@名称 		Dev_Ota_Start_Callback
*@功能 		启动设备OTA任务的回调
*@参数 		void* ota_info
*@返回值 	int
*@使用说明	
*/
static int Dev_Ota_Start_Callback(void* ota_info)
{
    ENTITY_LOGI("OTA设备启动阶段入口：准备切换设备状态并初始化Flash\r\n");
    Entity_Set_Dev_Status(DEV_OTA_START_STATE);
    int ret = Entity_Ota_Flash_Init();//OTA FLASH初始化
    if(ret != 0)
    {
        ENTITY_LOGE("OTA设备启动阶段失败：Flash初始化失败 ret=%d\r\n", ret);
        return ret;
    }
    ENTITY_LOGI("OTA设备启动阶段完成：Flash初始化成功\r\n");
    return 0;
}
 
/**
*@名称 		Dev_Ota_Progress_Percent_Report_Callback
*@功能 		OTA进度上报回调
*@参数 		void* ota_info
*@返回值 	void
*@使用说明	
*/
static void Dev_Ota_Progress_Percent_Report_Callback(void* ota_info)
{
    Entity_Http_Ota_Info_t *http_ota_info = (Entity_Http_Ota_Info_t *)ota_info;
    if((http_ota_info->Ota_Progress.Progress_Percent % 10 == 0) ||
            http_ota_info->Ota_Progress.Progress_Percent == 100)
    {
        ENTITY_LOGI("OTA下载阶段完成：progress=%d downloaded=%u/%u\r\n",
                http_ota_info->Ota_Progress.Progress_Percent,
                http_ota_info->Ota_Progress.Have_Down_Len,
                http_ota_info->Ota_Param.File_Size);
    }
    //if(http_ota_info->Ota_Progress.Progress_Percent % 10 == 0)
    {
        Entity_Mqtt_Event_Ota_Downloading_Report(0, NULL, NULL, http_ota_info->Ota_Progress.Progress_Percent, 0);
    }
}

/**
*@名称 		Dev_Ota_Fetch_Yield_Callback
*@功能 		OTA 从HTTP获取到数据回调
*@参数 		void* ota_info, unsigned char *buf, unsigned int len
*@返回值 	int
*@使用说明	
*/
static int Dev_Ota_Fetch_Yield_Callback(void* ota_info, unsigned char *buf, unsigned int len)
{
    int ret;
    if(len == 0)
    {
        ENTITY_LOGE("OTA写入阶段失败：收到空数据块\r\n");
        return -1;
    }
    Entity_Http_Ota_Info_t *http_ota_info = (Entity_Http_Ota_Info_t *)ota_info;
    ret = Entity_Ota_Flash_Process_Data(buf, len, http_ota_info->Ota_Param.File_Size);
    if(ret != 0)
    {
        ENTITY_LOGE("OTA写入阶段失败：Flash写入失败 ret=%d len=%u\r\n", ret, len);
        return ret;
    }
    return 0;
}

/**
*@名称 		Dev_Ota_Failed_Callback
*@功能 		OTA 失败回调
*@参数 		void* ota_info
*@返回值 	void
*@使用说明	
*/
static void Dev_Ota_Failed_Callback(void* ota_info)
{
    Entity_Http_Ota_Info_t *http_ota_info = (Entity_Http_Ota_Info_t *)ota_info;
    ENTITY_LOGE("OTA设备失败阶段入口：error_code=%d\r\n", http_ota_info->Ota_Progress.error_code);
    Entity_Mqtt_Event_Ota_Fail_Report(http_ota_info->Ota_Progress.error_code, NULL, NULL, 0);
    Entity_Set_Dev_Status(DEV_OTA_FAILD_STATE);
    ENTITY_LOGI("OTA设备失败阶段完成：已上报失败并切换设备状态\r\n");
}

/**
*@名称 		Dev_Ota_Success_Callback
*@功能 		OTA 成功 回调
*@参数 		void* ota_info
*@返回值 	void
*@使用说明	
*/
static void Dev_Ota_Success_Callback(void* ota_info)
{
    ENTITY_LOGI("OTA设备成功阶段入口：准备上报烧录状态并重启\r\n");
    Entity_Set_Dev_Status(DEV_OTA_SUCCESS_STATE);
    Entity_Mqtt_Event_Ota_Burning_Report(0, NULL, NULL, 0);
    Entity_Sleep_Ms(3000);
    ENTITY_LOGI("OTA设备成功阶段完成：即将重启进入新固件\r\n");
    Entity_System_Reset();
#if 0
    Entity_Http_Ota_Info_t *http_ota_info = (Entity_Http_Ota_Info_t *)ota_info;
    Entity_Device_Info_t *dev_info = Entity_Get_Dev_Info();
    char *mcu_version = strlen(dev_info->Sub_Version) ? dev_info->Sub_Version : NULL;
    Entity_Mqtt_Event_Device_Info_Report(1, dev_info->Dev_Version, mcu_version, NULL, 1);//上报设备信息
#endif
}

/**
*@名称 		Dev_Ota_End_Callback
*@功能 		OTA 结束回调
*@参数 		void* ota_info
*@返回值 	void
*@使用说明	
*/
static void Dev_Ota_End_Callback(void* ota_info)
{
    ENTITY_LOGI("OTA设备结束阶段入口：OTA任务收尾\r\n");
#if 0
    Entity_Http_Ota_Info_t *http_ota_info = (Entity_Http_Ota_Info_t *)ota_info;
    Entity_Device_Info_t *dev_info = Entity_Get_Dev_Info();
    char *mcu_version = strlen(dev_info->Sub_Version) ? dev_info->Sub_Version : NULL;
    Entity_Mqtt_Event_Ota_Version_Report(0, NULL, NULL, http_ota_info->Ota_Param.New_Version, mcu_version, 0);
#endif
    ENTITY_LOGI("OTA设备结束阶段完成：OTA任务收尾完成\r\n");
}

/**
*@名称 		Entity_Dev_Ota_Start
*@功能 		启动设备OTA任务
*@参数 		Entity_Http_Ota_Param_t *ota_param
*@返回值 	int
*@使用说明	
*/
int Entity_Dev_Ota_Start(Entity_Http_Ota_Param_t *ota_param)
{
    Entity_Http_Ota_Cbs_t ota_cbs=
    {
        .Ota_Start_Callback = Dev_Ota_Start_Callback,
        .Ota_Progress_Percent_Report_Callback = Dev_Ota_Progress_Percent_Report_Callback,
        .Ota_Fetch_Yield_Callback = Dev_Ota_Fetch_Yield_Callback,
        .Ota_Failed_Callback = Dev_Ota_Failed_Callback,
        .Ota_Success_Callback = Dev_Ota_Success_Callback,
        .Ota_End_Callback = Dev_Ota_End_Callback,
    };
    return Entity_Http_Ota_Task_Start(ota_param, &ota_cbs);
}

   










