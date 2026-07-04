//entity_uart.c
#include "entity_uart.h"

#include "entity_log.h"
#include "entity_iot_func.h"
#include "entity_authorization.h"

#include <string.h>


static Entity_Uart_Cbs_t Entity_Uart_Cbs;
static uint8_t Flag_Uart_Init;

#define ENTITY_UART_RECV_BUF_SIZE     1024
static unsigned short Entity_Uart_Rx_Len;
static unsigned char *Entity_Uart_Rx_Buf;
static Entity_Sema_t s_auth_rx_sem = NULL;

/**
*@名称 		Entity_Auth_Uart_Wait_Data
*@功能 		阻塞等待 UART 授權數據就緒（semaphore 驅動，取代 10ms 輪詢）
*@参数 		timeout_ms：等待超時時間（ms），0 表示永久等待
*@返回值 	0=有數據，-1=超時
*/
static int Entity_Auth_Uart_Wait_Data(unsigned int timeout_ms)
{
    if (s_auth_rx_sem == NULL)
        return -1;
    // 0 表示永久等待，映射到 ENTITY_WAIT_FOREVER
    uint32_t entity_timeout = (timeout_ms == 0) ? ENTITY_WAIT_FOREVER : timeout_ms;
    return Entity_Semaphore_Get(&s_auth_rx_sem, entity_timeout);
}

/**
*@名称 		Entity_Uart_Rec_Data_Callback
*@功能 		Uart接口数据回调
*@参数 		uint8_t *pdata, uint16_t len
*@返回值 	void
*@使用说明
*/
static void Entity_Uart_Rec_Data_Callback(uint8_t *pdata, uint16_t len)
{
    memcpy(Entity_Uart_Rx_Buf, pdata, len);
    Entity_Uart_Rx_Len = len;
    if (s_auth_rx_sem != NULL)
        Entity_Semaphore_Set(&s_auth_rx_sem);
}


/**
*@名称 		Entity_Uart_Cbs_Init
*@功能 		Uart接口函数初始化
*@参数 		Entity_Uart_Cbs_t *cbs
*@返回值 	void
*@使用说明	
*/
void Entity_Uart_Cbs_Init(Entity_Uart_Cbs_t *cbs)
{
    Entity_Uart_Cbs = *cbs;
}

/**
*@名称 		Entity_Uart_Init
*@功能 		Uart初始化
*@参数 		void
*@返回值 	void
*@使用说明	
*/
void Entity_Uart_Init(void)
{
    if(Flag_Uart_Init == 0)
    {
        if(Entity_Uart_Rx_Buf == NULL)
        {
            Entity_Uart_Rx_Buf = (unsigned char *)Entity_Mem_Malloc(ENTITY_UART_RECV_BUF_SIZE);
            if(Entity_Uart_Rx_Buf == NULL)
            {
                ENTITY_LOGE("Entity_Uart_Rx_Buf malloc faild\r\n");
                return ;
            }
        }
        Entity_Uart_Cbs.User_Uart_Init(ENTITY_UART_BAUD);
        Entity_Uart_Cbs.User_Uart_Register_Rec_Cbs(Entity_Uart_Rec_Data_Callback);
        Flag_Uart_Init = 1;      
    }
}

/**
*@名称 		Entity_Uart_Deinit
*@功能 		Uart初始化
*@参数 		void
*@返回值 	void
*@使用说明	
*/
void Entity_Uart_Deinit(void)
{
    if(Flag_Uart_Init == 1)
    { 
        if(Entity_Uart_Cbs.User_Uart_Deinit)
            Entity_Uart_Cbs.User_Uart_Deinit();
        Flag_Uart_Init = 0;         
    }
}

/**
*@名称 		Entity_User_Uart_Write_Bytes
*@功能 		Uart数据发送
*@参数 		uint8_t *pdata, uint16_t len
*@返回值 	void
*@使用说明	
*/
int Entity_User_Uart_Write_Bytes(uint8_t *pdata, uint16_t len)
{
    if(Entity_Uart_Cbs.User_Uart_Write_Bytes)
    {
        Entity_Uart_Cbs.User_Uart_Write_Bytes(pdata, len);
    }
    return 0;
}

//---------------------串口三元组授权相关------------------------//

/**
*@名称 		Entity_Auth_Uart_Polling_Read_Rec_Data
*@功能 		Uart授权轮询是否有接收数据
*@参数 		uint8_t *pdata, uint16_t len
*@返回值 	void
*@使用说明	
*/
static int Entity_Auth_Uart_Polling_Read_Rec_Data(uint8_t *pdata, uint16_t *len)
{
    if(Entity_Uart_Rx_Len)
    {
        memcpy(pdata, Entity_Uart_Rx_Buf, Entity_Uart_Rx_Len);
        *len = Entity_Uart_Rx_Len;
        Entity_Uart_Rx_Len = 0;
        return 0;
    }
    else
    {
        return -1;
    }
}

//授权接口
static Entity_Auth_Cbs_t Entity_Auth_Cbs={
    .Rx_Buf = 0,
    .Rx_Buf_Size = ENTITY_UART_RECV_BUF_SIZE,
    .Rx_Len = 0,
    .Write_Bytes = Entity_User_Uart_Write_Bytes,
    .Read_Bytes = Entity_Auth_Uart_Polling_Read_Rec_Data,
    .Wait_Data = Entity_Auth_Uart_Wait_Data,
};

/**
*@名称 		Entity_Uart_Auth_Init
*@功能 		Uart授权接口初始化
*@参数 		void
*@返回值 	void
*@使用说明	
*/
void Entity_Uart_Auth_Init(void)
{
    ENTITY_LOGE("%s\r\n", __func__);
    if (s_auth_rx_sem == NULL)
        Entity_Semaphore_Create(&s_auth_rx_sem, 1);
    Entity_Uart_Init();
    if(Entity_Auth_Cbs.Rx_Buf == NULL)
    {
        Entity_Auth_Cbs.Rx_Buf = (unsigned char *)Entity_Mem_Malloc(Entity_Auth_Cbs.Rx_Buf_Size);
        if(Entity_Auth_Cbs.Rx_Buf == NULL)
        {
            ENTITY_LOGE("Entity_Auth_Cbs.Rx_Buf malloc faild\r\n");
            return ;
        }
    }
    Register_Entity_Auth_Cbs(&Entity_Auth_Cbs);
}










