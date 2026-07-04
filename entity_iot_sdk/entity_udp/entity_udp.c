//entity_udp.c
#include "entity_udp.h"


#include "entity_log.h"
#include "entity_iot_func.h"
#include "entity_network.h"


#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>



static Udp_Server_Task_Info_t *Udp_Server_Task_Info;
static void *Boardcast_Addr;
static void *Server_Addr;

/**
*@名称 		Udp_Broadcast_Task
*@功能 		UDP广播任务
*@参数 		void *args
*@返回值 	void
*@使用说明	
*/
static void Udp_Broadcast_Task(void *args)
{
    Udp_Boradcast_Task_Info_t *task_info = (Udp_Boradcast_Task_Info_t *)args;

    task_info->Socket_Id = Entity_Udp_Broadcast_Init(task_info->Boardcast_Addr, task_info->Boardcast_Port, &Boardcast_Addr);
    //申请UDP套接字
    if(task_info->Socket_Id == -1)
    {
        ENTITY_LOGE("%s Failed to create socket:%s\r\n", __func__, strerror(errno));
        goto exit;
    }
    ENTITY_LOGI("%s Socket_Id:%d, Boardcast_Addr:%x, Boardcast_Port:%d\r\n", __func__, task_info->Socket_Id, task_info->Boardcast_Addr, task_info->Boardcast_Port);

    while (1)
    {
        if(!task_info->Task_Runing)
        { 
            break;
        }  
        if(Entity_Udp_Broadcast_Send_Bytes(task_info->Socket_Id, task_info->Context, task_info->Context_Len, Boardcast_Addr) == -1)
        {
            ENTITY_LOGE("%s socket_id:%d Failed to send broadcast message:%s\r\n", __func__, task_info->Socket_Id, strerror(errno));
        }
        Entity_Sleep_Ms(1000);
    }
exit:
    Entity_Network_Socket_Close(task_info->Socket_Id);
    task_info->Socket_Id = -1;
    task_info->Task_Runing = 0;
    ENTITY_LOGE("%s exit task\r\n", __func__);
    Entity_Pthread_Delete(&task_info->Thread_Id);
    return ;
}

/**
*@名称 		Entity_Udp_Broadcast_Task_Start
*@功能 		启动UDP广播任务
*@参数 		Udp_Boradcast_Task_Info_t *info
*@返回值 	int
*@使用说明	
*/
int Entity_Udp_Broadcast_Task_Start(Udp_Boradcast_Task_Info_t *info)
{
    if(!info->Context || info->Context_Len==0)
    {
        ENTITY_LOGE("%s param error!\r\n", __func__);
        return -1;
    }
    if(info->Task_Runing)
    {
        ENTITY_LOGE("%s have start!\r\n", __func__);
        return -1;
    }
    info->Task_Runing = 1;
    int ret = Entity_Pthread_Create(&info->Thread_Id, "Udp_Broadcast_Task", UDP_BOARDCAST_TASK_STACK_SIZE, UDP_BOARDCAST_TASK_PROI, Udp_Broadcast_Task, info);
    if(ret != 0) 
    {
        ENTITY_LOGE("%s task Create Failed!\r\n", __func__);
        info->Task_Runing = 0;
        return -1;
    }
    
    ENTITY_LOGI("%s task Create success!\r\n", __func__);
    return 0;
}

/**
*@名称 		Entity_Udp_Broadcast_Task_Stop
*@功能 		停止UDP广播任务
*@参数 		Udp_Boradcast_Task_Info_t *info
*@返回值 	int
*@使用说明	
*/
int Entity_Udp_Broadcast_Task_Stop(Udp_Boradcast_Task_Info_t *info)
{
    if(info->Task_Runing)
    {
        info->Task_Runing = 0;
        Entity_Pthread_Delete(&info->Thread_Id);
    }
    return 0;
} 

/**
*@名称 		Entity_Udp_Server_Task_Send_Bytes
*@功能 		UDP服务端发送数据
*@参数 		char *pdata, unsigned int le
*@返回值 	int
*@使用说明	
*/
static int Entity_Udp_Server_Task_Send_Bytes(char *pdata, unsigned int len)
{
    if(Udp_Server_Task_Info == NULL)
    {
        ENTITY_LOGE("%s Udp_Server_Task_Info not init\r\n", __func__);
        return -1;
    }
    if(Udp_Server_Task_Info->Socket_Id < 0)
        return -1;
    if(!Udp_Server_Task_Info->Client_Connected)
        return -1;
    int ret = Entity_Udp_Server_Send_Bytes(Udp_Server_Task_Info->Socket_Id, pdata, len, Server_Addr);
    return ret;
}

/**
*@名称 		Udp_Server_Task
*@功能 		UDP服务端监听任务
*@参数 		void *args
*@返回值 	void
*@使用说明	
*/
static void Udp_Server_Task(void *args)
{
    Udp_Server_Task_Info = (Udp_Server_Task_Info_t *)args;
    
    if(Udp_Server_Task_Info == NULL)
    {
        ENTITY_LOGE("%s param error\r\n", __func__);
        return ;
    }
    char *recv_buf = (char*)Entity_Mem_Malloc(UDP_SERVER_RECV_BUF_SIZE);
    if(recv_buf == NULL)
    {
        ENTITY_LOGE("%s malloc recv_buf faild\r\n", __func__);
        goto exit;
    }
    memset(recv_buf, 0, UDP_SERVER_RECV_BUF_SIZE);
    //申请UDP套接字
    Udp_Server_Task_Info->Socket_Id = Entity_Udp_Server_Init(Udp_Server_Task_Info->Server_Port, &Server_Addr);
    if(Udp_Server_Task_Info->Socket_Id == -1)
    {
        ENTITY_LOGE("%s Failed to create socket:%s\r\n", __func__, strerror(errno));
        goto exit;
    }
    ENTITY_LOGI("%s socket:%d,Server_Port:%d \r\n", __func__, Udp_Server_Task_Info->Socket_Id, Udp_Server_Task_Info->Server_Port);
    
    Udp_Server_Task_Info->Send_Bytes = Entity_Udp_Server_Task_Send_Bytes;

    while (1)
    {
        if(!Udp_Server_Task_Info->Task_Runing)
        { 
            break;
        }  
        unsigned int recv_len = UDP_SERVER_RECV_BUF_SIZE;
        memset(recv_buf, 0, UDP_SERVER_RECV_BUF_SIZE);
        int ret = Entity_Udp_Server_Recv_Bytes(Udp_Server_Task_Info->Socket_Id, recv_buf, &recv_len, Server_Addr);
       
        if (ret == 0 && recv_len > 0)
        {     
            Udp_Server_Task_Info->Client_Connected = 1;

            if(Udp_Server_Task_Info->Recv_Callback)
                Udp_Server_Task_Info->Recv_Callback(recv_buf);
                
        }
    }
exit:
    Entity_Network_Socket_Close(Udp_Server_Task_Info->Socket_Id);
    Udp_Server_Task_Info->Socket_Id = -1;
    Udp_Server_Task_Info->Task_Runing = 0;
    ENTITY_LOGE("%s exit task\r\n", __func__);
    Entity_Pthread_Delete(&Udp_Server_Task_Info->Thread_Id);
    return ;
}



/**
*@名称 		Entity_Udp_Server_Task_Start
*@功能 		启动UDP服务端监听任务
*@参数 		Udp_Server_Task_Info_t *info
*@返回值 	int
*@使用说明	
*/
int Entity_Udp_Server_Task_Start(Udp_Server_Task_Info_t *info)
{
    if(info->Task_Runing)
    {
        ENTITY_LOGE("%s have start!\r\n", __func__);
        return -1;
    }
    info->Task_Runing = 1;
    int ret = Entity_Pthread_Create(&info->Thread_Id, "Udp_Server_Task", UDP_SERVER_TASK_STACK_SIZE, UDP_SERVER_TASK_PROI, Udp_Server_Task, info);
    if(ret != 0) 
    {
        ENTITY_LOGE("%s task Create Failed!\r\n", __func__);
        info->Task_Runing = 0;
        return -1;
    }
    
    ENTITY_LOGI("%s task Create success!\r\n", __func__);
    return 0;
}

/**
*@名称 		Entity_Udp_Server_Task_Stop
*@功能 		停止UDP服务端监听任务
*@参数 		Udp_Server_Task_Info_t *info
*@返回值 	int
*@使用说明	
*/
int Entity_Udp_Server_Task_Stop(Udp_Server_Task_Info_t *info)
{
    if(info->Task_Runing)
    {
        info->Task_Runing = 0;
        Entity_Pthread_Delete(&info->Thread_Id);
    }
    return 0;
} 



























