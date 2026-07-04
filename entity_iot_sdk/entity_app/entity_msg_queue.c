//entity_msg_queue.c
#include "entity_msg_queue.h"


#include "entity_log.h"
#include "entity_iot_func.h"
#include "entity_ble_gatt.h"
#include "entity_mqtt_cmd_parse.h"
#include "entity_mqtt_event_respone_parse.h"
#include "entity_mqtt_app.h"
#include "entity_config_net.h"
#include "factory_mqtt_parse.h"
#include "factory_dynamic_register.h"
#include <string.h>


#define ENTITY_MSG_QUEUE_LEN                 500
#define ENTITY_MSG_QUEUE_TASK_PROI           3
#define ENTITY_MSG_QUEUE_TASK_STACK_SIZE     16384
#define ENTITY_MSG_QUEUE_COST_WARN_MS        20

// 對象池：減少高頻 malloc/free 造成的堆碎片
// 塊在 Entity_App_Msg_Queue_Init 中通過 Entity_Mem_Calloc（PSRAM）動態分配
#define MSG_POOL_ITEM_SIZE    512
#define MSG_POOL_ITEM_COUNT   16
static uint8_t *s_pool_blocks[MSG_POOL_ITEM_COUNT];
static uint16_t s_pool_used = 0;
static Entity_Critical_t s_pool_cs = NULL;

static void Msg_Pool_Init(void)
{
    Entity_Critical_Create(&s_pool_cs);
    for (int i = 0; i < MSG_POOL_ITEM_COUNT; i++)
        s_pool_blocks[i] = (uint8_t *)Entity_Mem_Calloc(1, MSG_POOL_ITEM_SIZE);
}

static void *Msg_Pool_Alloc(unsigned int size)
{
    if (size < MSG_POOL_ITEM_SIZE)
    {
        Entity_Critical_Enter(&s_pool_cs);
        for (int i = 0; i < MSG_POOL_ITEM_COUNT; i++)
        {
            if (s_pool_blocks[i] && !(s_pool_used & (uint16_t)(1u << i)))
            {
                s_pool_used |= (uint16_t)(1u << i);
                Entity_Critical_Exit(&s_pool_cs);
                memset(s_pool_blocks[i], 0, size + 1);
                return s_pool_blocks[i];
            }
        }
        Entity_Critical_Exit(&s_pool_cs);
    }
    return Entity_Mem_Calloc(1, size + 1);
}

static void Msg_Pool_Free(void *ptr)
{
    Entity_Critical_Enter(&s_pool_cs);
    for (int i = 0; i < MSG_POOL_ITEM_COUNT; i++)
    {
        if (s_pool_blocks[i] == ptr)
        {
            s_pool_used &= (uint16_t)~(1u << i);
            Entity_Critical_Exit(&s_pool_cs);
            return;
        }
    }
    Entity_Critical_Exit(&s_pool_cs);
    Entity_Mem_Free(ptr);
}


static void *Msg_Queue_Id;
static void * Msg_Recv_Thread_Id;

void *Entity_Msg_Queue_Recv_Thread(void *arg);

static uint32_t Entity_Msg_Queue_Now_Ms(void)
{
    return (uint32_t)Entity_Get_Uptime_Ms();
}

/**
*@名称 		Entity_App_Msg_Queue_Init
*@功能 		消息队列处理初始化
*@参数 		uint16_t value
*@返回值 	void
*@使用说明	
*/
int Entity_App_Msg_Queue_Init(void)
{
    Msg_Pool_Init();
    int ret = Entity_Msg_Queue_Create(&Msg_Queue_Id, ENTITY_MSG_QUEUE_LEN, sizeof(Entity_Msg_Data_t));

    if(ret != 0) 
    {
        ENTITY_LOGE("[%s][%d]Entity_App_Msg_Queue_Init err!", __func__, __LINE__);
        return -1;
    }
    ENTITY_LOGD(" %s Entity_App_Msg_Queue_Init success!", __func__);
    ret = Entity_Pthread_Create(&Msg_Recv_Thread_Id, "msg queue task", ENTITY_MSG_QUEUE_TASK_STACK_SIZE, ENTITY_MSG_QUEUE_TASK_PROI, Entity_Msg_Queue_Recv_Thread, NULL);
    if(ret != 0) 
    {
        ENTITY_LOGE(" %s task Create Failed!", __func__);
        return -1;
    }
    ENTITY_LOGD(" %s task Create success!", __func__);
    return 0;
}

/**
*@名称 		Entity_App_Msg_Queue_Send
*@功能 		发送消息
*@参数 		unsigned char msg_type, const void *pdata, unsigned int data_len
*@返回值 	int
*@使用说明	
*/
int Entity_App_Msg_Queue_Send(unsigned char msg_type, const void *pdata, unsigned int data_len)
{
    Entity_Msg_Data_t Entity_Msg_Data;
    Entity_Msg_Data.Type = msg_type;
    Entity_Msg_Data.Data_Len = data_len;
    uint32_t alloc_start_ms = Entity_Msg_Queue_Now_Ms();
    Entity_Msg_Data.Data = Msg_Pool_Alloc(data_len);
    uint32_t queue_alloc_ms = Entity_Msg_Queue_Now_Ms() - alloc_start_ms;
    if(Entity_Msg_Data.Data == NULL)
    {
        ENTITY_LOGE("[%s][%d]Entity_Mem_Calloc faild! queue_alloc_ms=%u",
                  __func__, __LINE__, (unsigned int)queue_alloc_ms);
        return -1;
    }
    memcpy(Entity_Msg_Data.Data, pdata, data_len);
    uint32_t send_start_ms = Entity_Msg_Queue_Now_Ms();
    int ret = Entity_Msg_Queue_Send(&Msg_Queue_Id, &Entity_Msg_Data, sizeof(Entity_Msg_Data_t), 0);// 非阻塞入列
    uint32_t queue_send_ms = Entity_Msg_Queue_Now_Ms() - send_start_ms;
    if (queue_alloc_ms >= ENTITY_MSG_QUEUE_COST_WARN_MS ||
        queue_send_ms >= ENTITY_MSG_QUEUE_COST_WARN_MS)
    {
        ENTITY_LOGW("[MQTT_DIAG][APP_QUEUE_COST] msg_type=%u len=%u "
                  "queue_alloc_ms=%u queue_send_ms=%u warn=%u ret=%d\r\n",
                  (unsigned int)msg_type,
                  data_len,
                  (unsigned int)queue_alloc_ms,
                  (unsigned int)queue_send_ms,
                  (unsigned int)ENTITY_MSG_QUEUE_COST_WARN_MS,
                  ret);
    }
    if (ret != 0)
    {
        ENTITY_LOGE("Entity_App_Msg_Queue_Send failed ret=%d\r\n", ret);
        Msg_Pool_Free(Entity_Msg_Data.Data);
        return -1;
    }
    ENTITY_LOGD("Entity_App_Msg_Queue_Send done msg_type=%d\r\n", msg_type);
    return 0;
}


/**
*@名称 		Entity_Msg_Queue_Recv_Thread
*@功能 		接收消息队列处理任务
*@参数 		uint16_t value
*@返回值 	void
*@使用说明	
*/
void *Entity_Msg_Queue_Recv_Thread(void *arg)
{
    Entity_Msg_Data_t msg_data = {0};
    uint32_t msg_size = sizeof(Entity_Msg_Data_t);
    int ret=0;

    while (1)
    {
        ret = Entity_Msg_Queue_Wait(&Msg_Queue_Id, &msg_data, &msg_size, -1);//一直等待
        if (ret != 0) 
        {
            continue;
        }
        ENTITY_LOGD("[%s]type:%d, len:%d\r\n", __func__, msg_data.Type, msg_data.Data_Len);
        switch (msg_data.Type)
        {
            case ENTITY_MSG_TYPE_MQTT_COMMAND://云平台下发命令
                Entity_Mqtt_Msg_Cmd_parse_Process((char*)msg_data.Data);
                break;
            case ENTITY_MSG_TYPE_MQTT_EVENT://云平台应答
                Entity_Mqtt_Msg_Event_Respone_Parse_Process(msg_data.Data);
                break;
            case ENTITY_MSG_TYPE_BLE://接收到的蓝牙消息
                Entity_Ble_Parser_Data(msg_data.Data, msg_data.Data_Len);
                break;
            case ENTITY_MSG_TYPE_SET_NET_MODE:
            {
                Entity_Net_Mode_e mode = *(Entity_Net_Mode_e*)msg_data.Data;
                Entity_Set_Net_Mode(mode);
                break;    
            }
            case ENTITY_MSG_TYPE_FACTORY_MQTT_EVENT:
                Factory_Mqtt_Msg_Event_Respone_Parse_Process(msg_data.Data);
                break;
            case ENTITY_MSG_TYPE_REGISTER_MQTT_EVENT:
                Register_Mqtt_Msg_Event_Respone_Parse_Process(msg_data.Data);
                break;
             case ENTITY_MSG_TYPE_FACTORY_MQTT_COMMAND:
                Factory_Mqtt_Msg_Cmd_parse_Process(msg_data.Data);
                break;
            default:
                ENTITY_LOGE("msg queue type error!!!\r\n");
                break;
        }
        Msg_Pool_Free(msg_data.Data);
    }
}















