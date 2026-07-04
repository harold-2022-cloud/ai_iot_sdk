//entity_http_ota.c
#include "entity_http_ota.h"

#include "entity_log.h"
#include "entity_iot_func.h"
#include "entity_param_check.h"
#include "entity_error_code.h"

#include "com_utils.h"
#include "com_mbedtls.h"

#include <string.h>

static Entity_Ota_Flash_Func_t Entity_Ota_Flash_Func;


#define ENTITY_HTTP_OTA_TASK_STACK_SIZE       12288 
#define ENTITY_HTTP_OTA_TASK_PROI             3 

static Entity_Http_Ota_Info_t Entity_Http_Ota_Info;
static unsigned char Ota_Task_Cancel=0;
static unsigned char Ota_Task_Start=0;
static void *Http_Ota_Thread_Id;

static void *Ota_Timeout_Timer_Handle=0;//OTA超时检测定时器
#define OTA_TIMEOUT_PERIOD        300000     //时基定时器周期300S
#define OTA_TIMEOUT_TIMER_TYPE   ENTITY_TIMER_ONESHOT_TYPE

/**
*@名称 		Ota_Timeout_Callback
*@功能 		OTA超时定时器回调
*@参数 		void *arg 
*@返回值 	void
*@使用说明  
*/
static void Ota_Timeout_Callback(void *arg1, void *arg2)
{
    ENTITY_LOGE("OTA超时阶段失败：下载超过%u ms，准备取消任务\r\n", OTA_TIMEOUT_PERIOD);
    Entity_Http_Ota_Info.Ota_Progress.error_code = ENTITY_HTTP_OTA_ERR_FETCH_TIMEOUT;
    Entity_Http_Ota_Task_Exit();
}

/**
*@名称     Entity_Ota_Timeout_Timer_Create
*@功能     创建OTA超时定时器
*@参数     void 
*@返回值   int
*@使用说明  
*/
static int Entity_Ota_Timeout_Timer_Create(void)
{
    if(Ota_Timeout_Timer_Handle == 0)
    {
        //创建单次定时器
        Ota_Timeout_Timer_Handle = Entity_Timer_Create(OTA_TIMEOUT_TIMER_TYPE, OTA_TIMEOUT_PERIOD, Ota_Timeout_Callback, NULL);
        if(Ota_Timeout_Timer_Handle == NULL)
        {
            ENTITY_LOGE("%s, ota timer create faild\r\n", __func__);
            return -1;
        }  
        else
        {
            ENTITY_LOGD("%s, ota  timer create success\r\n", __func__);
            return 0;
        }    
    }
    return 0;
}
/**
*@名称     Entity_Ota_Timeout_Timer_Start
*@功能     启动OTA定时器
*@参数     void 
*@返回值   int
*@使用说明  
*/
static int Entity_Ota_Timeout_Timer_Start(void)
{
    if(Ota_Timeout_Timer_Handle == 0)
    {
        if(Entity_Ota_Timeout_Timer_Create())
            return -1;
        return Entity_Timer_Start(OTA_TIMEOUT_TIMER_TYPE, Ota_Timeout_Timer_Handle);
    } 
    else if(Entity_Timer_Is_Running(OTA_TIMEOUT_TIMER_TYPE, Ota_Timeout_Timer_Handle))
    {
        return Entity_Timer_Reload(OTA_TIMEOUT_TIMER_TYPE, Ota_Timeout_Timer_Handle);
    }
    else
    {
        return Entity_Timer_Start(OTA_TIMEOUT_TIMER_TYPE, Ota_Timeout_Timer_Handle);
    }
}

/**
*@名称        Entity_Ota_Timeout_Timer_Stop
*@功能        停止OTA定时器
*@参数        void 
*@返回值   void
*@使用说明  
*/
static void Entity_Ota_Timeout_Timer_Stop(void)
{
    if(Ota_Timeout_Timer_Handle)
        Entity_Timer_Stop(OTA_TIMEOUT_TIMER_TYPE, Ota_Timeout_Timer_Handle);
}

/**
*@名称        Entity_Ota_Timeout_Timer_Delete
*@功能        删除OTA定时器
*@参数        void 
*@返回值   void
*@使用说明  
*/
static __attribute__((unused)) void Entity_Ota_Timeout_Timer_Delete(void)
{
    if(Ota_Timeout_Timer_Handle)
        Entity_Timer_Delete(OTA_TIMEOUT_TIMER_TYPE, Ota_Timeout_Timer_Handle);
}


/**
*@名称 		Entity_Ota_Flash_Func_Init
*@功能 		获取OTA FLASH操作接口
*@参数 		Entity_Ota_Flash_Func_t *func
*@返回值 	void
*@使用说明	
*/
void Entity_Ota_Flash_Func_Init(Entity_Ota_Flash_Func_t *func)
{
    Entity_Ota_Flash_Func = *func;
}

/**
*@名称 		Entity_Ota_Flash_Init
*@功能 		
*@参数 		void
*@返回值 	int
*@使用说明	
*/
int Entity_Ota_Flash_Init(void)
{
    return Entity_Ota_Flash_Func.Ota_Flash_Init();
}

/**
*@名称 		Entity_Ota_Flash_Process_Data
*@功能 		
*@参数 		unsigned char *buf, uint16_t len, uint32_t total
*@返回值 	int
*@使用说明	
*/
int Entity_Ota_Flash_Process_Data(unsigned char *buf, uint16_t len, uint32_t total)
{
    return Entity_Ota_Flash_Func.Ota_Flash_Process_Data(buf, len, total);
}

/**
*@名称 		Entity_Ota_Flash_Deinit
*@功能 		
*@参数 		void
*@返回值 	int
*@使用说明	
*/
int Entity_Ota_Flash_Deinit(void)
{
    return Entity_Ota_Flash_Func.Ota_Flash_Deinit();
}

/**
*@名称 		Entity_Ota_Flash_Complete
*@功能 		
*@参数 		void
*@返回值 	int
*@使用说明	
*/
int Entity_Ota_Flash_Complete(void)
{
    if(!Entity_Ota_Flash_Func.Ota_Flash_Complete)
    {
        ENTITY_LOGE("OTA烧录阶段失败：Flash完成回调未注册\r\n");
        return -1;
    }
    return Entity_Ota_Flash_Func.Ota_Flash_Complete();
}
   


/**
*@名称 		Entity_Http_Ota_Init
*@功能 		初始化HTTP下载相关参数
*@参数 		Entity_Http_Ota_Param_t *ota_param, Entity_Http_Ota_Cbs_t *ota_cbs
*@返回值 	int
*@使用说明	
*/
int Entity_Http_Ota_Init(Entity_Http_Ota_Param_t *ota_param, Entity_Http_Ota_Cbs_t *ota_cbs)
{
    if(!ota_param || !ota_param->Url || !ota_param->New_Version || ota_param->File_Size == 0)
    {
        ENTITY_LOGE("OTA初始化阶段失败：参数非法 url=%p version=%p file_size=%u\r\n",
                ota_param ? (void *)ota_param->Url : NULL,
                ota_param ? (void *)ota_param->New_Version : NULL,
                ota_param ? ota_param->File_Size : 0);
        return -1;
    }
    ENTITY_LOGI("OTA初始化阶段入口：url=%s new_version=%s md5=%s file_size=%u\r\n", ota_param->Url,
            ota_param->New_Version, ota_param->Md5_Sum ? ota_param->Md5_Sum : "", ota_param->File_Size);

    Entity_Http_Ota_Info.Ota_Param.Url = Utils_Strdup(ota_param->Url);
    POINTER_SAFETY_CHECK_GOTO_LABEL(Entity_Http_Ota_Info.Ota_Param.Url, exit);
    Entity_Http_Ota_Info.Ota_Param.Mssage_Id = Utils_Strdup(ota_param->Mssage_Id);
    POINTER_SAFETY_CHECK_GOTO_LABEL(Entity_Http_Ota_Info.Ota_Param.Mssage_Id, exit);
    Entity_Http_Ota_Info.Ota_Param.New_Version = Utils_Strdup(ota_param->New_Version);
    POINTER_SAFETY_CHECK_GOTO_LABEL(Entity_Http_Ota_Info.Ota_Param.New_Version, exit);
    if(ota_param->Md5_Sum)
    {
        Entity_Http_Ota_Info.Ota_Param.Md5_Sum = Utils_Strdup(ota_param->Md5_Sum);
        POINTER_SAFETY_CHECK_GOTO_LABEL(Entity_Http_Ota_Info.Ota_Param.Md5_Sum, exit);
    }
    else
    {
        Entity_Http_Ota_Info.Ota_Param.Md5_Sum = NULL;
    }
    if(ota_param->Private_Data && ota_param->Private_Len>0)
    {
        Entity_Http_Ota_Info.Ota_Param.Private_Data = Entity_Mem_Malloc(ota_param->Private_Len);
        POINTER_SAFETY_CHECK_GOTO_LABEL(Entity_Http_Ota_Info.Ota_Param.Private_Data, exit);
        memcpy(Entity_Http_Ota_Info.Ota_Param.Private_Data, ota_param->Private_Data, ota_param->Private_Len);
        Entity_Http_Ota_Info.Ota_Param.Private_Len = ota_param->Private_Len;
    }
    else
    {
        Entity_Http_Ota_Info.Ota_Param.Private_Data = NULL;
    }
    
    Entity_Http_Ota_Info.Ota_Param.File_Size = ota_param->File_Size;

    if(ota_cbs)
        Entity_Http_Ota_Info.Ota_Cbs = *ota_cbs;
    else
        memset(&Entity_Http_Ota_Info.Ota_Cbs, 0, sizeof(Entity_Http_Ota_Info.Ota_Cbs));

    Entity_Http_Ota_Info.Ota_Progress.Md5 = Mbedtls_Md5_Init();
    if(!Entity_Http_Ota_Info.Ota_Progress.Md5)
    {
        Entity_Http_Ota_Info.Ota_Progress.error_code = ENTITY_HTTP_OTA_ERR_NOMEM;
        ENTITY_LOGE("OTA初始化阶段失败：MD5上下文分配失败\r\n");
        goto exit;
    }
    Entity_Http_Ota_Info.Ota_State = ENTITY_HTTP_OTA_STATE_INITED;
    ENTITY_LOGI("OTA初始化阶段完成：new_version=%s file_size=%u\r\n",
            Entity_Http_Ota_Info.Ota_Param.New_Version, Entity_Http_Ota_Info.Ota_Param.File_Size);
    return 0;
exit:
    Entity_Mem_Free(Entity_Http_Ota_Info.Ota_Param.Url);
    Entity_Mem_Free(Entity_Http_Ota_Info.Ota_Param.Mssage_Id);
    Entity_Mem_Free(Entity_Http_Ota_Info.Ota_Param.New_Version);
    Entity_Mem_Free(Entity_Http_Ota_Info.Ota_Param.Md5_Sum);
    Entity_Mem_Free(Entity_Http_Ota_Info.Ota_Param.Private_Data);
    Entity_Http_Ota_Info.Ota_State = ENTITY_HTTP_OTA_STATE_UNINIT;
    ENTITY_LOGE("OTA初始化阶段失败：内存分配失败\r\n");
    return -2;
}


/**
*@名称 		Entity_Http_Ota_Deinit
*@功能 		反初始化HTTP下载相关参数
*@参数 		void
*@返回值 	void
*@使用说明	
*/
void Entity_Http_Ota_Deinit(void)
{
    Entity_Http_Download_Deinit(Entity_Http_Ota_Info.Http_Download_Handle);
    Entity_Mem_Free(Entity_Http_Ota_Info.Ota_Param.Url);
    Entity_Mem_Free(Entity_Http_Ota_Info.Ota_Param.Mssage_Id);
    Entity_Mem_Free(Entity_Http_Ota_Info.Ota_Param.New_Version);
    Entity_Mem_Free(Entity_Http_Ota_Info.Ota_Param.Md5_Sum);
    Entity_Mem_Free(Entity_Http_Ota_Info.Ota_Param.Private_Data);
    Mbedtls_Md5_Deinit(Entity_Http_Ota_Info.Ota_Progress.Md5);

    memset(&Entity_Http_Ota_Info, 0, sizeof(Entity_Http_Ota_Info));
    Entity_Http_Ota_Info.Ota_State = ENTITY_HTTP_OTA_STATE_UNINIT;

}

/**
*@名称 		Entity_Http_Ota_Get_FetchOffset
*@功能 		获取开始下载偏移量
*@参数 		void
*@返回值 	void
*@使用说明	
*/
unsigned int Entity_Http_Ota_Get_Fetch_Offset(void)
{
    return 0;
}

/**
*@名称 		Entity_Http_Ota_Start_Download
*@功能 		启动HTTP下载
*@参数 		void
*@返回值 	void
*@使用说明	
*/
int Entity_Http_Ota_Start_Download(void)
{
    int ret=0;
    ENTITY_LOGI("%s\r\n", __func__);
    //Entity_Http_Download_Deinit(Entity_Http_Ota_Info.Http_Download_Handle);
    Entity_Http_Ota_Info.Ota_Progress.Have_Down_Len = Entity_Http_Ota_Get_Fetch_Offset();
    Entity_Http_Ota_Param_t *ota_param = &Entity_Http_Ota_Info.Ota_Param;
    Entity_Http_Ota_Info.Http_Download_Handle = Entity_Http_Download_Init(ota_param->Url, Entity_Http_Ota_Info.Ota_Progress.Have_Down_Len, ota_param->File_Size);
    MALLOC_POINTER_SAFETY_CHECK_RETURN_ERR(Entity_Http_Ota_Info.Http_Download_Handle, "http download init faild",  QCLOUD_ERR_FAILURE);
    ret = Entity_Http_Download_Connect(Entity_Http_Ota_Info.Http_Download_Handle, 0);
    if(ret != QCLOUD_RET_SUCCESS)
    {
        ENTITY_LOGE("%s http download conncet faild\r\n", __func__);
        Entity_Http_Ota_Info.Ota_State = ENTITY_HTTP_OTA_STATE_DISCONNECTED;
    }
    else
    {
        ENTITY_LOGI("%s http download conncet success\r\n", __func__);
    }
    return ret;
}

/**
*@名称 		Entity_Http_Ota_Fetch_Yield
*@功能 		获取数据
*@参数 		char *buf, uint32_t buf_len, uint32_t timeout_ms
*@返回值 	int
*@使用说明	
*/
int Entity_Http_Ota_Fetch_Yield(char *buf, uint32_t buf_len, uint32_t timeout_ms)
{
    int len=0;
    int ret=0;
    len = Entity_Http_Download_Fetch_Data(Entity_Http_Ota_Info.Http_Download_Handle, buf, buf_len, timeout_ms);
    if(len <= 0)
    {
        Entity_Http_Ota_Info.Ota_State = ENTITY_HTTP_OTA_STATE_FETCHED;
        Entity_Http_Ota_Info.Ota_Progress.error_code = ENTITY_HTTP_OTA_ERR_FETCH_FAILED;
        return -1;
    }
    else if(Entity_Http_Ota_Info.Ota_Progress.Have_Down_Len == 0)
    {
        //上报
    }
    //unsigned int copy_len = len > Entity_Http_Ota_Info.Ota_Progress.Remain_Down_Len ? Entity_Http_Ota_Info.Ota_Progress.Remain_Down_Len : len;
    unsigned int copy_len = len;
    Entity_Http_Ota_Info.Ota_Progress.Current_Down_Len = len;
    
    if(Entity_Http_Ota_Info.Ota_Cbs.Ota_Fetch_Yield_Callback)
    {
        ret = Entity_Http_Ota_Info.Ota_Cbs.Ota_Fetch_Yield_Callback(&Entity_Http_Ota_Info, (unsigned char*)buf, copy_len);
    }
    if(ret != 0)
    {
        Entity_Http_Ota_Info.Ota_Progress.error_code = ENTITY_HTTP_OTA_ERR_FETCH_FAILED;
        return ret;
    }
    Mbedtls_Md5_Update(Entity_Http_Ota_Info.Ota_Progress.Md5, buf, copy_len);
    Entity_Http_Ota_Info.Ota_Progress.Have_Down_Len += copy_len;
    Entity_Http_Ota_Info.Ota_Progress.Progress_Percent = Entity_Http_Ota_Info.Ota_Progress.Have_Down_Len * 100 / Entity_Http_Ota_Info.Ota_Param.File_Size;
    if(Entity_Http_Ota_Info.Ota_Progress.Progress_Percent != Entity_Http_Ota_Info.Ota_Progress.Last_Percent)
    {
        ENTITY_LOGI("Progress_Percent:%d\r\n", Entity_Http_Ota_Info.Ota_Progress.Progress_Percent);
        Entity_Http_Ota_Info.Ota_Progress.Last_Percent = Entity_Http_Ota_Info.Ota_Progress.Progress_Percent;
        if(Entity_Http_Ota_Info.Ota_Cbs.Ota_Progress_Percent_Report_Callback)
        {
            Entity_Http_Ota_Info.Ota_Cbs.Ota_Progress_Percent_Report_Callback(&Entity_Http_Ota_Info);
        }
    }
    if(Entity_Http_Ota_Info.Ota_Progress.Have_Down_Len >= Entity_Http_Ota_Info.Ota_Param.File_Size)
    {
        Entity_Http_Ota_Info.Ota_State = ENTITY_HTTP_OTA_STATE_FETCHED;
    }
    
    return ret;
}

/**
*@名称 		Entity_Http_Ota_Data_Check
*@功能 	    下载数据有效性检查
*@参数 		void
*@返回值 	int
*@使用说明	
*/
int Entity_Http_Ota_Data_Check(void)
{
    char md5_str[33] = {0};
    Mbedtls_Md5_Finish(Entity_Http_Ota_Info.Ota_Progress.Md5, md5_str);
    if(!Entity_Http_Ota_Info.Ota_Param.Md5_Sum)//没有MD5值不需要MD5校验
    {
        ENTITY_LOGI("OTA校验阶段完成：云端未下发MD5，跳过MD5校验\r\n");
        return 0;
    }
    ENTITY_LOGI("OTA校验阶段入口：cloud_md5=%s local_md5=%s\r\n", Entity_Http_Ota_Info.Ota_Param.Md5_Sum, md5_str);
    if(strcmp(Entity_Http_Ota_Info.Ota_Param.Md5_Sum, md5_str) == 0)//MD5校验正确
    {
        ENTITY_LOGI("OTA校验阶段完成：MD5校验通过\r\n");
        return 0;
    }
    Entity_Http_Ota_Info.Ota_Progress.error_code = ENTITY_HTTP_OTA_ERR_MD5_MISMATCH;
    ENTITY_LOGE("OTA校验阶段失败：MD5不匹配 cloud_md5=%s local_md5=%s\r\n",
            Entity_Http_Ota_Info.Ota_Param.Md5_Sum, md5_str);
    return -1;
}



/**
*@名称 		Entity_Http_Ota_Task
*@功能 		HTTP OTA任务
*@参数 		void
*@返回值 	void
*@使用说明	
*/
int Entity_Http_Ota_Task(void *param)
{
    int ret=0;
    ENTITY_LOGI("OTA下载阶段入口：file_size=%u url=%s\r\n",
            Entity_Http_Ota_Info.Ota_Param.File_Size,
            Entity_Http_Ota_Info.Ota_Param.Url ? Entity_Http_Ota_Info.Ota_Param.Url : "");
    unsigned char flag_fetch_success=0;
    unsigned char flag_ota_success=0;
    char *recv_buf = (char *)Entity_Mem_Malloc(OTA_BUF_LEN);
    if(!recv_buf)
    {
        ENTITY_LOGE("OTA下载阶段失败：接收缓存分配失败 size=%d\r\n", OTA_BUF_LEN);
        Entity_Http_Ota_Info.Ota_Progress.error_code = ENTITY_HTTP_OTA_ERR_NOMEM;
        goto ota_finish;
    }
	Entity_Http_Ota_Info.Ota_State = ENTITY_HTTP_OTA_STATE_FETCHING;

    while(!Ota_Task_Cancel)
    {
        if(Ota_Task_Cancel)
        {
            ENTITY_LOGE("OTA下载阶段失败：任务已取消\r\n");
            goto ota_cancel;
        } 

        ret = Entity_Http_Ota_Start_Download();
        if(ret != QCLOUD_RET_SUCCESS)
        {
            ENTITY_LOGE("OTA下载阶段失败：HTTP连接失败 ret=%d\r\n", ret);
            Entity_Http_Ota_Info.Ota_Progress.error_code = ENTITY_HTTP_OTA_ERR_FETCH_FAILED;
            goto ota_finish;
        }
        ENTITY_LOGI("OTA下载阶段完成：HTTP连接成功，开始接收固件\r\n");

        while(Entity_Http_Ota_Info.Ota_State != ENTITY_HTTP_OTA_STATE_FETCHED)
        {
            if(Ota_Task_Cancel)
            {
                ENTITY_LOGE("OTA下载阶段失败：下载过程中任务取消\r\n");
                goto ota_cancel;
            } 

            ret = Entity_Http_Ota_Fetch_Yield(recv_buf, OTA_BUF_LEN, 60);
            ENTITY_LOGI("ret:%d, file_size:%u, fetched size:%u\r\n",
                    ret,
                    Entity_Http_Ota_Info.Ota_Param.File_Size,
                    Entity_Http_Ota_Info.Ota_Progress.Have_Down_Len);
            if(ret < 0)//接收失败
            {
                if(Entity_Http_Ota_Info.Ota_Progress.error_code == ENTITY_HTTP_OTA_ERR_NONE)
                    Entity_Http_Ota_Info.Ota_Progress.error_code = ENTITY_HTTP_OTA_ERR_FETCH_FAILED;
                ENTITY_LOGE("OTA下载阶段失败：接收固件失败 ret=%d downloaded=%u/%u\r\n",
                        ret,
                        Entity_Http_Ota_Info.Ota_Progress.Have_Down_Len,
                        Entity_Http_Ota_Info.Ota_Param.File_Size);
                goto ota_finish;
            }
            if(Entity_Http_Ota_Info.Ota_State == ENTITY_HTTP_OTA_STATE_FETCHED)
            {
                flag_fetch_success = 1;
                ENTITY_LOGI("OTA下载阶段完成：固件下载完成 downloaded=%u/%u\r\n",
                        Entity_Http_Ota_Info.Ota_Progress.Have_Down_Len,
                        Entity_Http_Ota_Info.Ota_Param.File_Size);
                break;
            }
            Entity_Sleep_Ms(5);
        }
        ENTITY_LOGI("flag_fetch_success:%d\r\n", flag_fetch_success);
        if(flag_fetch_success)//成功拉取数据
        {
            if(Entity_Http_Ota_Data_Check() == 0)
            {
                flag_ota_success = 1;
                ENTITY_LOGI("OTA校验阶段完成：下载数据有效，准备完成Flash写入\r\n");
            }
            else
            {
                flag_ota_success = 0;
                ENTITY_LOGE("OTA校验阶段失败：下载数据校验未通过\r\n");
            }
        }
        break;

    }
ota_cancel:
    if(Entity_Http_Ota_Info.Ota_Progress.error_code == ENTITY_HTTP_OTA_ERR_NONE)
        Entity_Http_Ota_Info.Ota_Progress.error_code = ENTITY_HTTP_OTA_ERR_FAIL;
ota_finish:
    Ota_Task_Cancel = 0;
    Ota_Task_Start = 0;
    Entity_Ota_Timeout_Timer_Stop();//停止OTA超时检测定时器
    if(flag_ota_success)
    {
        ENTITY_LOGI("OTA烧录阶段入口：准备结束写入并设置启动分区\r\n");
        ret = Entity_Ota_Flash_Complete();
        if(ret != 0)
        {
            flag_ota_success = 0;
            Entity_Http_Ota_Info.Ota_Progress.error_code = ENTITY_HTTP_OTA_ERR_FETCH_FAILED;
            ENTITY_LOGE("OTA烧录阶段失败：完成写入或设置启动分区失败 ret=%d\r\n", ret);
        }
        else
        {
            ENTITY_LOGI("OTA烧录阶段完成：已设置新启动分区，准备上报成功并重启\r\n");
        }
    }
    if(flag_ota_success)
    {
        if(Entity_Http_Ota_Info.Ota_Cbs.Ota_Success_Callback)
        {
            Entity_Http_Ota_Info.Ota_Cbs.Ota_Success_Callback(&Entity_Http_Ota_Info);
        }
    }
    else
    {
        if(Entity_Http_Ota_Info.Ota_Progress.error_code == ENTITY_HTTP_OTA_ERR_NONE)
            Entity_Http_Ota_Info.Ota_Progress.error_code = ENTITY_HTTP_OTA_ERR_FAIL;
        if(Entity_Http_Ota_Info.Ota_Cbs.Ota_Failed_Callback)
        {
            Entity_Http_Ota_Info.Ota_Cbs.Ota_Failed_Callback(&Entity_Http_Ota_Info);
        }
    }

    if(Entity_Http_Ota_Info.Ota_Cbs.Ota_End_Callback)
        Entity_Http_Ota_Info.Ota_Cbs.Ota_End_Callback(&Entity_Http_Ota_Info);

    //退出OTA的处理
    if(!flag_ota_success)
    {
        Entity_Ota_Flash_Deinit();
    }
    if(recv_buf)
        Entity_Mem_Free(recv_buf);
    Entity_Http_Ota_Deinit();
    Entity_Pthread_Delete(&Http_Ota_Thread_Id);
    return 0;    
}

/**
*@名称 		Entity_Http_Ota_Task_Start
*@功能 		启动HTTP OTA任务
*@参数 		Entity_Http_Ota_Param_t *ota_param, Entity_Http_Ota_Cbs_t *ota_cbs
*@返回值 	void
*@使用说明	
*/
int Entity_Http_Ota_Task_Start(Entity_Http_Ota_Param_t *ota_param, Entity_Http_Ota_Cbs_t *ota_cbs)
{
    int ret=0;
    if(Ota_Task_Start)
    {
        ENTITY_LOGE("OTA启动阶段失败：OTA任务已经在运行\r\n");
        return -1;
    }
    ENTITY_LOGI("OTA启动阶段入口：version=%s file_size=%u url=%s\r\n",
            ota_param && ota_param->New_Version ? ota_param->New_Version : "",
            ota_param ? ota_param->File_Size : 0,
            ota_param && ota_param->Url ? ota_param->Url : "");
    ret = Entity_Http_Ota_Init(ota_param, ota_cbs);
    RET_VALUE_SUCCESS_CHECK_RETURN_ERR(ret, "Entity_Http_Ota_Init Failed!", -1);
    ret = Entity_Ota_Timeout_Timer_Start();
    if(ret != 0)
    {
        ENTITY_LOGE("OTA启动阶段失败：超时定时器启动失败 ret=%d\r\n", ret);
        Entity_Http_Ota_Info.Ota_Progress.error_code = ENTITY_HTTP_OTA_ERR_OSC_FAILED;
        if(Entity_Http_Ota_Info.Ota_Cbs.Ota_Failed_Callback)
        {
            Entity_Http_Ota_Info.Ota_Cbs.Ota_Failed_Callback(&Entity_Http_Ota_Info);
        }
        Entity_Http_Ota_Deinit();
        return -1;
    }

    if(Entity_Http_Ota_Info.Ota_Cbs.Ota_Start_Callback)
    {
        ret = Entity_Http_Ota_Info.Ota_Cbs.Ota_Start_Callback(&Entity_Http_Ota_Info);
        if(ret != 0)
        {
            ENTITY_LOGE("OTA启动阶段失败：业务启动回调失败 ret=%d\r\n", ret);
            Entity_Http_Ota_Info.Ota_Progress.error_code = ENTITY_HTTP_OTA_ERR_FAIL;
            if(Entity_Http_Ota_Info.Ota_Cbs.Ota_Failed_Callback)
            {
                Entity_Http_Ota_Info.Ota_Cbs.Ota_Failed_Callback(&Entity_Http_Ota_Info);
            }
            Entity_Ota_Flash_Deinit();
            Entity_Ota_Timeout_Timer_Stop();
            Entity_Http_Ota_Deinit();
            return -1;
        }
    }

    Ota_Task_Start = 1;
    Ota_Task_Cancel = 0;
    ret = Entity_Pthread_Create(&Http_Ota_Thread_Id, "http ota task", ENTITY_HTTP_OTA_TASK_STACK_SIZE, ENTITY_HTTP_OTA_TASK_PROI, 
                                    Entity_Http_Ota_Task, NULL);

    if(ret != 0)
    {
        ENTITY_LOGE("OTA启动阶段失败：创建下载任务失败 ret=%d\r\n", ret);
        Entity_Http_Ota_Info.Ota_Progress.error_code = ENTITY_HTTP_OTA_ERR_OSC_FAILED;
        if(Entity_Http_Ota_Info.Ota_Cbs.Ota_Failed_Callback)
        {
            Entity_Http_Ota_Info.Ota_Cbs.Ota_Failed_Callback(&Entity_Http_Ota_Info);
        }
        Ota_Task_Start = 0;
        Entity_Ota_Flash_Deinit();
        Entity_Ota_Timeout_Timer_Stop();
        Entity_Http_Ota_Deinit();
        return -1;
    }
    ENTITY_LOGI("OTA启动阶段完成：HTTP OTA任务已创建\r\n");
    
    return 0;
}

/**
*@名称 		Entity_Http_Ota_Task_Runing
*@功能 	    HTTP OTA任务是否正在进行
*@参数 		void
*@返回值 	void
*@使用说明	
*/
unsigned char Entity_Http_Ota_Task_Runing(void)
{
    return Ota_Task_Start;
}

/**
*@名称 		Entity_Http_Ota_Task_Exit
*@功能 		退出HTTP OTA任务
*@参数 		void
*@返回值 	void
*@使用说明	
*/
void Entity_Http_Ota_Task_Exit(void)
{
    Ota_Task_Cancel = 1;
    ENTITY_LOGI("cancel ota task\r\n");
}

/**
*@名称 		Wait_Entity_Http_Ota_Task_Exit
*@功能 		等待退出HTTP OTA任务
*@参数 		void
*@返回值 	void
*@使用说明	
*/
int Wait_Entity_Http_Ota_Task_Exit(unsigned int timeout_ms)
{
    while(1)
    {
        if(!Ota_Task_Start)
            return 0;
        if(timeout_ms)
        {
            timeout_ms--;
            if(timeout_ms == 0)
                return -1;
        }
        Entity_Sleep_Ms(1);
    }
    return -1;
}





