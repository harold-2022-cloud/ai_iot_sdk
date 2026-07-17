//entity_mqtt_app.c
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "entity_mqtt_app.h"

#include "entity_mqtt_client.h"

#include "entity_error_code.h"
#include "entity_iot_func.h"
#include "entity_log.h"
#include "entity_dev_info.h"
#include "com_mbedtls.h"
#include "com_utils.h"
#include "entity_mqtt_event_report.h"
#include "entity_msg_queue.h"
#include "entity_config_net.h" /* 用于停止配网超时定时器 */

#if defined(__has_include)
#if __has_include("FreeRTOS.h") && __has_include("task.h")
#include "FreeRTOS.h"
#include "task.h"
#define ENTITY_MQTT_HAVE_TASK_DIAG 1
#elif __has_include("freertos/FreeRTOS.h") && __has_include("freertos/task.h")
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#define ENTITY_MQTT_HAVE_TASK_DIAG 1
#endif
#endif
#ifndef ENTITY_MQTT_HAVE_TASK_DIAG
#define ENTITY_MQTT_HAVE_TASK_DIAG 0
#endif

/* 开发者模式下使用的 MQTT 地址 */
#define DEV_MODE_NVS_KEY   "dev_mode"
#define DEV_MODE_MQTT_HOST "192.168.31.112"
#define DEV_MODE_MQTT_PORT (1883U)

#define ENTITY_MQTT_APP_TASK_PROI             3
#define ENTITY_MQTT_APP_TASK_STACK_SIZE       12288
#define ENTITY_MQTT_APP_HEARTBEAT_MS          45000
#define ENTITY_MQTT_AI_PENDING_QOS1_STALE_MS  10000

#ifndef AI_HOTPATH_VERBOSE_LOGS
#ifdef CONFIG_AI_HOTPATH_VERBOSE_LOGS
#define AI_HOTPATH_VERBOSE_LOGS CONFIG_AI_HOTPATH_VERBOSE_LOGS
#else
#define AI_HOTPATH_VERBOSE_LOGS 0
#endif
#endif

#define ENTITY_MQTT_VERBOSE_LOGI(...)     \
    do {                                  \
        if (AI_HOTPATH_VERBOSE_LOGS) {    \
            ENTITY_LOGI(__VA_ARGS__);     \
        }                                 \
    } while (0)

static Entity_Mqtt_Context_t Entity_Client_Instance = {0};
static void * Mqtt_Client_Thread_Id;
static volatile uint8_t Flag_Entity_Mqtt_App_Task_Runing = 0;  //任务正在运行标志
static volatile uint8_t Flag_Entity_Mqtt_App_Task_Over = 0;    //任务已结束标志

static Entity_Mqtt_App_Msg_Reach_Callback_t Entity_Mqtt_App_Msg_Reach_Cbs;
static Entity_Mqtt_App_Ready_Callback_f Entity_Mqtt_App_Ready_Cb;
static Entity_Mutex_t s_cb_mutex = NULL;
/* 連線狀態原子標誌：由 Connected/Disconnected callback 在狀態切換時更新。
 * Entity_Mqtt_App_Is_Connected() 直接讀取此標誌（不持任何鎖），確保外部 Task
 * 能立即得到連線狀態，不被 MQTT 重連或 ProcessLoop 阻塞。*/
static volatile bool s_mqtt_app_connected = false;

/* ── coreMQTT-Agent 指令隊列 ─────────────────────────────────────────────
 * 原則：所有 MQTT_Publish 操作只能由 Agent Task（Entity_Mqtt_Client_Task）
 * 在 Entity_Mqtt_Loop() 前後的安全點執行，外部 Task 及 ProcessLoop 內部的
 * callback 只能非阻塞 post 指令到隊列，絕不在 callback 鏈中呼叫 Publish。
 * 好處：徹底消除「callback 中重入 MQTT_Publish」導致的 mutex 死鎖。*/

/** Agent 指令類型 */
typedef enum
{
    AGENT_CMD_PUBLISH = 0,
} Entity_Mqtt_Agent_Cmd_Type_e;

/** Agent 指令結構體（輕量，Queue 以值複製傳遞）*/
typedef struct
{
    Entity_Mqtt_Agent_Cmd_Type_e  type;
    Topic_Type_e                topic_type;
    void                       *data;      /**< Entity_Mem_Malloc(PSRAM) 的副本，Agent Task 負責 free */
    int                         len;
    int                         qos;
    int                         retained;
    uint32_t                    trace_seq;
    uint32_t                    enqueue_ms;
    bool                        ai_access;
} Entity_Mqtt_Agent_Cmd_t;

#define ENTITY_MQTT_AGENT_QUEUE_SIZE  16
static Entity_Queue_t s_agent_queue = NULL;
static uint32_t s_last_queue_wait_log_ms = 0;
static uint32_t s_last_app_heartbeat_ms = 0;
static volatile bool s_connected_post_pending = false;
static volatile uint32_t s_connected_post_seq = 0;
static uint32_t s_agent_trace_seq = 0;
static volatile uint32_t s_mqtt_task_stack_hwm = 0;
static volatile uint8_t s_mqtt_task_priority = 0;

static uint32_t Entity_Mqtt_App_Current_Task_Stack_Hwm(void)
{
#if ENTITY_MQTT_HAVE_TASK_DIAG
    return (uint32_t)uxTaskGetStackHighWaterMark(NULL);
#else
    return 0U;
#endif
}

static uint8_t Entity_Mqtt_App_Current_Task_Priority(void)
{
#if ENTITY_MQTT_HAVE_TASK_DIAG
    return (uint8_t)uxTaskPriorityGet(NULL);
#else
    return 0U;
#endif
}

static bool Entity_Mqtt_App_Buffer_Contains(const char *data, int len, const char *needle)
{
    size_t needle_len;

    if (data == NULL || len <= 0 || needle == NULL)
    {
        return false;
    }

    needle_len = strlen(needle);
    if (needle_len == 0U || (size_t)len < needle_len)
    {
        return false;
    }

    for (int i = 0; i <= len - (int)needle_len; ++i)
    {
        if (memcmp(data + i, needle, needle_len) == 0)
        {
            return true;
        }
    }
    return false;
}

static const char *Entity_Mqtt_App_State_Str(uint8_t state)
{
    switch (state)
    {
        case ENTITY_MQTT_IDLE_STATE:               return "IDLE";
        case ENTITY_MQTT_CONNCET_STATE:            return "CONNECT";
        case ENTITY_MQTT_SUBSCRIBING_STATE:        return "SUBSCRIBING";
        case ENTITY_MQTT_SUBSCRIBE_COMPLETE_STATE: return "SUBSCRIBE_COMPLETE";
        case ENTITY_MQTT_YIELD_STATE:              return "YIELD";
        case ENTITY_MQTT_RECONNECT_STATE:          return "RECONNECT";
        default:                                 return "UNKNOWN";
    }
}

const char *Entity_Mqtt_App_State_Name(uint8_t state)
{
    return Entity_Mqtt_App_State_Str(state);
}

const char *Entity_Mqtt_App_Broker_State_Name(uint8_t state)
{
    return Mqtt_Client_Broker_Liveness_Str((Mqtt_Client_Broker_Liveness_t)state);
}

static Mqtt_Client_Broker_Liveness_t Entity_Mqtt_App_Normalize_Broker_State(const Entity_Mqtt_Context_t *context,
                                                                            Mqtt_Client_Broker_Liveness_t core_state)
{
    if (context == NULL)
    {
        return MQTT_BROKER_LIVENESS_DISCONNECTED;
    }

    switch (context->State)
    {
        case ENTITY_MQTT_IDLE_STATE:
            return MQTT_BROKER_LIVENESS_DISCONNECTED;
        case ENTITY_MQTT_CONNCET_STATE:
        case ENTITY_MQTT_RECONNECT_STATE:
            return MQTT_BROKER_LIVENESS_CONNECTING;
        case ENTITY_MQTT_SUBSCRIBING_STATE:
        case ENTITY_MQTT_SUBSCRIBE_COMPLETE_STATE:
            return MQTT_BROKER_LIVENESS_SUBSCRIBING;
        default:
            return core_state;
    }
}

static bool Entity_Mqtt_App_Broker_Allows_Ai_Publish(Mqtt_Client_Broker_Liveness_t state)
{
    return state == MQTT_BROKER_LIVENESS_READY;
}

static bool Entity_Mqtt_App_Broker_Healthy_For_Ai_Wait(Mqtt_Client_Broker_Liveness_t state)
{
    return state == MQTT_BROKER_LIVENESS_READY;
}

static uint32_t Entity_Mqtt_App_Now_Ms(void)
{
    return Entity_Get_Run_Time_Ms();
}

static void Entity_Mqtt_App_Log_Reconnect_Pending_Snapshot(const char *stage, const char *reason)
{
    Entity_Mqtt_Context_t *context = &Entity_Client_Instance;
    Entity_Mqtt_App_Health_Snapshot_t health = {0};
    char token_id[64] = {0};
    uint64_t token_pub_ms = 0;
    uint64_t now_ms = (uint64_t)Entity_Mqtt_App_Now_Ms();
    bool token_pending = Entity_Mqtt_Get_Token_Pending_Snapshot(token_id, sizeof(token_id), &token_pub_ms);
    uint64_t token_age_ms = (token_pending && now_ms >= token_pub_ms) ? (now_ms - token_pub_ms) : 0U;

    (void)Entity_Mqtt_App_Get_Health_Snapshot(&health);
    ENTITY_LOGW("[MQTT_DIAG][RECONNECT_APP_SNAPSHOT] stage=%s reason=%s "
              "app_connected=%d ctx_connected=%d state=%s broker=%s "
              "wait_ping=%d ping_age=%u ping_rtt=%u last_rx_age=%u last_inbound_age=%u "
              "pending_qos1=%u oldest_msgid=%u oldest_age=%u agent_queue=%u "
              "token_pending=%d token_id=%s token_age=%llu token_pub_ms=%llu "
              "task_prio=%u task_stack_hwm=%u\r\n",
              stage ? stage : "unknown",
              reason ? reason : "unknown",
              health.app_connected ? 1 : 0,
              health.ctx_connected ? 1 : 0,
              Entity_Mqtt_App_State_Str(context->State),
              Entity_Mqtt_App_Broker_State_Name(health.broker_state),
              health.wait_ping ? 1 : 0,
              (unsigned int)health.pingreq_age_ms,
              (unsigned int)health.last_pingresp_rtt_ms,
              (unsigned int)health.last_rx_age_ms,
              (unsigned int)health.broker_last_inbound_alive_age_ms,
              (unsigned int)health.pending_qos1,
              (unsigned int)health.oldest_qos1_msgid,
              (unsigned int)health.oldest_qos1_age_ms,
              (unsigned int)health.agent_queue_depth,
              token_pending ? 1 : 0,
              token_pending ? token_id : "(none)",
              (unsigned long long)token_age_ms,
              (unsigned long long)token_pub_ms,
              (unsigned int)health.mqtt_task_priority,
              (unsigned int)health.mqtt_task_stack_hwm);
}

static bool Entity_Mqtt_App_Topic_Equals(const char *rx_topic, const char *expected_topic)
{
    if (rx_topic == NULL || expected_topic == NULL)
    {
        return false;
    }

    size_t rx_len = strlen(rx_topic);
    size_t expected_len = strlen(expected_topic);
    return (rx_len == expected_len) && (memcmp(rx_topic, expected_topic, rx_len) == 0);
}

static void Entity_Mqtt_App_Drain_Agent_Queue(Entity_Mqtt_Context_t *entity_context)
{
    if (entity_context == NULL || !entity_context->Is_Connected || s_agent_queue == NULL)
    {
        return;
    }

    uint32_t queue_depth = (uint32_t)Entity_Msg_Queue_Get_Msg_Num(&s_agent_queue);
    if (queue_depth > 0)
    {
        ENTITY_MQTT_VERBOSE_LOGI("[MQTT_DIAG][AGENT_DRAIN_BEGIN] state=%s queue_depth=%u\r\n",
                  Entity_Mqtt_App_State_Str(entity_context->State),
                  (unsigned int)queue_depth);
    }

    Entity_Mqtt_Agent_Cmd_t cmd;
    uint32_t msg_sz;
    while (Entity_Msg_Queue_Wait(&s_agent_queue, &cmd, &msg_sz, 0) == 0)
    {
        if (cmd.type == AGENT_CMD_PUBLISH)
        {
            uint32_t dequeue_ms = Entity_Mqtt_App_Now_Ms();
            if (cmd.ai_access)
            {
                ENTITY_LOGI("[MQTT_TRACE][T5_DEQUEUE] trace=%u dequeue_ms=%u queued_age_ms=%u "
                            "topic_type=%d len=%d qos=%d retained=%d depth_left=%u "
                            "task_prio=%u task_stack_hwm=%u\r\n",
                            (unsigned int)cmd.trace_seq,
                            (unsigned int)dequeue_ms,
                            (unsigned int)(dequeue_ms - cmd.enqueue_ms),
                            (int)cmd.topic_type,
                            cmd.len,
                            cmd.qos,
                            cmd.retained,
                            (unsigned int)Entity_Msg_Queue_Get_Msg_Num(&s_agent_queue),
                            (unsigned int)s_mqtt_task_priority,
                            (unsigned int)s_mqtt_task_stack_hwm);
            }
            ENTITY_MQTT_VERBOSE_LOGI("[MQTT_DIAG][AGENT_PUB] topic_type=%d len=%d qos=%d "
                      "retained=%d depth_left=%u\r\n",
                      (int)cmd.topic_type,
                      cmd.len,
                      cmd.qos,
                      cmd.retained,
                      (unsigned int)Entity_Msg_Queue_Get_Msg_Num(&s_agent_queue));
            uint32_t publish_begin_ms = Entity_Mqtt_App_Now_Ms();
            int pub_ret = Entity_Mqtt_Topic_Publish(entity_context,
                                                  cmd.topic_type,
                                                  (const char *)cmd.data,
                                                  cmd.len,
                                                  cmd.qos,
                                                  cmd.retained);
            uint32_t publish_end_ms = Entity_Mqtt_App_Now_Ms();
            if (cmd.ai_access)
            {
                ENTITY_LOGI("[MQTT_TRACE][T6_TOPIC_PUBLISH] trace=%u begin_ms=%u end_ms=%u cost_ms=%u "
                            "ret=%d topic_type=%d len=%d qos=%d\r\n",
                            (unsigned int)cmd.trace_seq,
                            (unsigned int)publish_begin_ms,
                            (unsigned int)publish_end_ms,
                            (unsigned int)(publish_end_ms - publish_begin_ms),
                            pub_ret,
                            (int)cmd.topic_type,
                            cmd.len,
                            cmd.qos);
            }
            bool publish_ok = (pub_ret > 0) ||
                              ((cmd.qos == QOS0_MOST_ONCE) && (pub_ret == 0));
            if (!publish_ok)
            {
                ENTITY_LOGE("[AGENT] publish failed topic=%d ret=%d\r\n",
                          (int)cmd.topic_type, pub_ret);
            }
            else
            {
                s_last_app_heartbeat_ms = Entity_Mqtt_App_Now_Ms();
            }
        }
        Entity_Mem_Free(cmd.data);
    }
}

static void Entity_Mqtt_Connected_Post_Process(Entity_Mqtt_Context_t *context)
{
    if (!s_connected_post_pending)
    {
        return;
    }

    uint32_t post_seq = s_connected_post_seq;
    s_connected_post_pending = false;

    if (context == NULL || !context->Is_Connected || !s_mqtt_app_connected)
    {
        ENTITY_LOGW("[MQTT_DIAG][CONNECTED_POST_SKIP] seq=%u reason=disconnected "
                  "ctx=%p ctx_connected=%d app_connected=%d\r\n",
                  (unsigned int)post_seq,
                  context,
                  (context && context->Is_Connected) ? 1 : 0,
                  s_mqtt_app_connected ? 1 : 0);
        return;
    }

    uint32_t start_ms = Entity_Mqtt_App_Now_Ms();
    ENTITY_LOGI("[MQTT_DIAG][CONNECTED_POST_BEGIN] seq=%u state=%s ctx_connected=%d "
              "queue_depth=%u\r\n",
              (unsigned int)post_seq,
              Entity_Mqtt_App_State_Str(context->State),
              context->Is_Connected ? 1 : 0,
              s_agent_queue ? (unsigned int)Entity_Msg_Queue_Get_Msg_Num(&s_agent_queue) : 0);

    Entity_Config_Net_Timer_Stop(); /* MQTT 已连接，停止配网超时定时器，防止误清除配网信息 */
    Entity_Set_Dev_Status(DEV_WIFI_CLOUD_CONNECT_STATE);
    Entity_Dev_Config_Net_Info_t *dev_config_info = Entity_Get_Dev_Config_Net_Info();
    Entity_Device_Info_t *dev_info = Entity_Get_Dev_Info();
    if(!dev_config_info->Flag_Bind)//获取到了配网信息，但是还未与云端交互完成配网流程
    {
        Entity_Config_Net_Info_t *config_info = Entity_Get_Config_Net_Info();
        dev_info->Need_Clean_Data = 1;//暂时先清除云端所有用户数据,正常应该到FLASH中读标志
        Entity_Mqtt_Event_Device_Bind_Report(config_info->User_Id, config_info->Bind_Id, dev_info->Dev_Version, dev_info->Sub_Version, dev_info->Need_Clean_Data, 1);
    }
    else
    {
        char token_pub_id[32] = {0};
        if (Entity_Mqtt_Get_Token_Pending_Snapshot(token_pub_id, sizeof(token_pub_id), NULL))
        {
            ENTITY_LOGW("[MQTT_DIAG][STARTUP_REPORT_SKIP] reason=ai_token_pending token_id=%s\r\n",
                      token_pub_id);
        }
        else
        {
            ENTITY_LOGI("[MQTT_DIAG][STARTUP_REPORT_QOS0] code=info ack=1 reason=avoid_qos1_outbox\r\n");
            Entity_Mqtt_Event_Device_Info_Report_Qos(1, dev_info->Dev_Version, dev_info->Sub_Version, NULL, 1, QOS0_MOST_ONCE);
            ENTITY_LOGI("[MQTT_DIAG][STARTUP_REPORT_QOS0] code=time ack=1 reason=avoid_qos1_outbox\r\n");
            Entity_Mqtt_Event_Get_Time_Request_Qos(1, QOS0_MOST_ONCE);
        }
    }
    cJSON *iot_properties = Entity_Mqtt_Get_Dev_Thing_Model_Cjson();
    if(!iot_properties)
    {
        ENTITY_LOGD(" %s get model\r\n", __func__);
        Entity_Mqtt_Event_Get_Model_Request(MODEL_TYPE_COMPLETE, 1);//获取设备完整物模型
    }
    else//已经有物模型了，则不处理
    {
        char *content = cJSON_PrintUnformatted(iot_properties);
        if(content == NULL)
        {
            ENTITY_LOGE("%s cJSON_PrintUnformatted faild\r\n", __func__);
        }
        else
        {
            ENTITY_LOGI("%s mini model:%s\r\n", __func__, content);
            cJSON_free(content);
        }
    }
    Entity_Mqtt_App_Ready_Callback_f ready_cb = Get_Entity_Mqtt_App_Ready_Cbs();
    if(ready_cb)
        ready_cb();

    ENTITY_LOGI("[MQTT_DIAG][CONNECTED_POST_DONE] seq=%u cost=%u state=%s "
              "queue_depth=%u\r\n",
              (unsigned int)post_seq,
              (unsigned int)(Entity_Mqtt_App_Now_Ms() - start_ms),
              Entity_Mqtt_App_State_Str(context->State),
              s_agent_queue ? (unsigned int)Entity_Msg_Queue_Get_Msg_Num(&s_agent_queue) : 0);
}

/**
*@名称 		Entity_Mqtt_Generate_Client_Config
*@功能 		生成MQTT客户端登录配置信息
*@参数 		Mqtt_Client_Config_t *client_info
*@返回值 	void
*@使用说明	
*/
static void Entity_Mqtt_Generate_Client_Config(const char* uuid, const char* secret, Entity_Mqtt_Auth_t *mqtt_auth)
{
    unsigned char hmac_sha1[20] = {0};
    snprintf(mqtt_auth->Client_Id, sizeof(mqtt_auth->Client_Id)-1, "rlink_%s_V2", uuid);
    snprintf(mqtt_auth->Username, sizeof(mqtt_auth->Username)-1,"%s|signMethod=hmacSha1,ts=0", uuid);
    snprintf(mqtt_auth->Password, sizeof(mqtt_auth->Password)-1,"uuid=%s,ts=0", uuid);
    char password_mask[32];
    ENTITY_LOGI("client_id: %s\n", mqtt_auth->Client_Id);
    ENTITY_LOGI("username: %s\n", mqtt_auth->Username);
    Utils_Mask_Secret(mqtt_auth->Password, password_mask, sizeof(password_mask));
    ENTITY_LOGI("password_mask: %s\n", password_mask);
    Mbedtls_Hmac(MBEDTLS_MD_SHA1, (unsigned char *)secret, \
        strlen(secret), (unsigned char *)mqtt_auth->Password, strlen(mqtt_auth->Password), hmac_sha1);

    Hex_Array_To_String(hmac_sha1, sizeof(hmac_sha1), mqtt_auth->Password);
    Utils_Mask_Secret(mqtt_auth->Password, password_mask, sizeof(password_mask));
    ENTITY_LOGI("hmac_sha1 password_mask: %s\r\n", password_mask);
}



/**
*@名称 		Register_Entity_Mqtt_App_Ready_Cb
*@功能 		注册MQTT准备就绪回调
*@参数 		Entity_Mqtt_App_Ready_Callback_f cb
*@返回值 	void
*@使用说明	
*/
void Register_Entity_Mqtt_App_Ready_Cb(Entity_Mqtt_App_Ready_Callback_f cb)
{
    Entity_Mutex_Lock(&s_cb_mutex, ENTITY_WAIT_FOREVER);
    Entity_Mqtt_App_Ready_Cb = cb;
    Entity_Mutex_Unlock(&s_cb_mutex);
}

/**
*@名称 		Get_Entity_Mqtt_App_Ready_Cbs
*@功能 		准备就绪回调
*@参数 		Entity_Mqtt_App_Ready_Callback_f *
*@返回值 	cvoid
*@使用说明	
*/
Entity_Mqtt_App_Ready_Callback_f Get_Entity_Mqtt_App_Ready_Cbs(void)
{
    Entity_Mutex_Lock(&s_cb_mutex, ENTITY_WAIT_FOREVER);
    Entity_Mqtt_App_Ready_Callback_f cb = Entity_Mqtt_App_Ready_Cb;
    Entity_Mutex_Unlock(&s_cb_mutex);
    return cb;
}

/**
*@名称 		Register_Entity_Mqtt_App_Msg_Reach_Cbs
*@功能 		注册MQTT消息到达回调
*@参数 		Entity_Mqtt_Msg_Reach_Callback_t *cbs
*@返回值 	void
*@使用说明	
*/
void Register_Entity_Mqtt_App_Msg_Reach_Cbs(Entity_Mqtt_App_Msg_Reach_Callback_t *cbs)
{
    Entity_Mutex_Lock(&s_cb_mutex, ENTITY_WAIT_FOREVER);
    Entity_Mqtt_App_Msg_Reach_Cbs = *cbs;
    Entity_Mutex_Unlock(&s_cb_mutex);
}

/**
*@名称 		Get_MEntity_qtt_Msg_Reach_Cbs
*@功能 		获取MQTT消息到达回调
*@参数 		Entity_Mqtt_App_Msg_Reach_Callback_t *
*@返回值 	cvoid
*@使用说明	
*/
Entity_Mqtt_App_Msg_Reach_Callback_t Get_Entity_Mqtt_App_Msg_Reach_Cbs(void)
{
    Entity_Mutex_Lock(&s_cb_mutex, ENTITY_WAIT_FOREVER);
    Entity_Mqtt_App_Msg_Reach_Callback_t cbs = Entity_Mqtt_App_Msg_Reach_Cbs;
    Entity_Mutex_Unlock(&s_cb_mutex);
    return cbs;
}

/**
*@名称 		Entity_Mqtt_Connected_Callback
*@功能 		MQTT客户端连接成功回调
*@参数 		Entity_Mqtt_Context_t* context, void* user_data
*@返回值 	void
*@使用说明	
*/
static void Entity_Mqtt_Connected_Callback(Entity_Mqtt_Context_t* context, void* user_data)
{
    (void)user_data;
    s_mqtt_app_connected = true;
    s_last_app_heartbeat_ms = Entity_Mqtt_App_Now_Ms();
    s_connected_post_seq++;
    s_connected_post_pending = true;
    ENTITY_LOGE("%s\r\n", __func__);
    ENTITY_LOGI("[MQTT_DIAG][APP_CONNECTED_CB] state=%s ctx_connected=%d queue_depth=%u "
              "post_seq=%u\r\n",
              Entity_Mqtt_App_State_Str(context->State),
              context->Is_Connected ? 1 : 0,
              s_agent_queue ? (unsigned int)Entity_Msg_Queue_Get_Msg_Num(&s_agent_queue) : 0,
              (unsigned int)s_connected_post_seq);
    ENTITY_LOGI("[MQTT_DIAG][CONNECTED_POST_PENDING] seq=%u state=%s\r\n",
              (unsigned int)s_connected_post_seq,
              Entity_Mqtt_App_State_Str(context->State));
}

/**
*@名称 		Entity_Mqtt_Disconnected_Callback
*@功能 		MQTT客户端断开连接回调
*@参数 		Entity_Mqtt_Context_t* context, void* user_data
*@返回值 	void
*@使用说明	
*/
static void Entity_Mqtt_Disconnected_Callback(Entity_Mqtt_Context_t* context, void* user_data)
{
#if 1
    (void)user_data;
    s_mqtt_app_connected = false;
    s_connected_post_pending = false;
    ENTITY_LOGI("----------now mqtt client disconnected-----------\r\n");
    ENTITY_LOGW("[MQTT_DIAG][APP_DISCONNECTED_CB] state=%s ctx_connected=%d queue_depth=%u\r\n",
              Entity_Mqtt_App_State_Str(context->State),
              context->Is_Connected ? 1 : 0,
              s_agent_queue ? (unsigned int)Entity_Msg_Queue_Get_Msg_Num(&s_agent_queue) : 0);
    Entity_Set_Dev_Status(DEV_WIFI_CLOUD_DISCONNECT_STATE);
#endif
}

/**
*@名称 		Entity_Mqtt_Recv_Message_Callback
*@功能 		MQTT客户端接收到消息回调
*@参数 		Entity_Mqtt_Context_t* context, void* user_data, const void* vmsg
*@返回值 	void
*@使用说明	
*/
static void Entity_Mqtt_Recv_Message_Callback(Entity_Mqtt_Context_t* context, void* user_data, const void* vmsg)
{
    (void)user_data;
    Entity_Mqtt_Message_t *entity_msg = (Entity_Mqtt_Message_t *)vmsg;
    if (context == NULL || entity_msg == NULL || entity_msg->Topic == NULL)
    {
        ENTITY_LOGE("[MQTT_DIAG][ROUTE_INVALID] context=%p msg=%p topic=%p\r\n",
                  context, entity_msg, entity_msg ? entity_msg->Topic : NULL);
        return;
    }

    const char *event_topic = Entity_Mqtt_Get_Topic(TOPIC_TYPE_EVENT_SUBSCRIBE);
    const char *cmd_topic = Entity_Mqtt_Get_Topic(TOPIC_TYPE_CMD_SUBSCRIBE);
    size_t rx_topic_len = strlen(entity_msg->Topic);
    size_t event_topic_len = event_topic ? strlen(event_topic) : 0;
    size_t cmd_topic_len = cmd_topic ? strlen(cmd_topic) : 0;
    bool is_event_topic = Entity_Mqtt_App_Topic_Equals(entity_msg->Topic, event_topic);
    bool is_cmd_topic = Entity_Mqtt_App_Topic_Equals(entity_msg->Topic, cmd_topic);
    uint32_t app_queue_depth = s_agent_queue ? (uint32_t)Entity_Msg_Queue_Get_Msg_Num(&s_agent_queue) : 0;

    ENTITY_MQTT_VERBOSE_LOGI("[MQTT_DIAG][ROUTE_RX] msgid=%u qos=%d payload_len=%u "
              "topic_len=%u event_len=%u cmd_len=%u event_match=%d cmd_match=%d "
              "state=%s ctx_connected=%d app_connected=%d last_sub_id=%u pub_queue_depth=%u\r\n",
              (unsigned int)entity_msg->Msgid,
              entity_msg->Qos,
              (unsigned int)entity_msg->Len,
              (unsigned int)rx_topic_len,
              (unsigned int)event_topic_len,
              (unsigned int)cmd_topic_len,
              is_event_topic ? 1 : 0,
              is_cmd_topic ? 1 : 0,
              Entity_Mqtt_App_State_Str(context->State),
              context->Is_Connected ? 1 : 0,
              s_mqtt_app_connected ? 1 : 0,
              (unsigned int)context->Last_Subscribe_Id,
              (unsigned int)app_queue_depth);

    if (is_event_topic)
    {
        if (!context->Is_Connected || !s_mqtt_app_connected || context->State != ENTITY_MQTT_YIELD_STATE)
        {
            ENTITY_LOGW("[MQTT_DIAG][REPORT_RESPONSE_RX_NOT_READY] msgid=%u state=%s "
                      "ctx_connected=%d app_connected=%d last_sub_id=%u len=%u\r\n",
                      (unsigned int)entity_msg->Msgid,
                      Entity_Mqtt_App_State_Str(context->State),
                      context->Is_Connected ? 1 : 0,
                      s_mqtt_app_connected ? 1 : 0,
                      (unsigned int)context->Last_Subscribe_Id,
                      (unsigned int)entity_msg->Len);
        }
        ENTITY_MQTT_VERBOSE_LOGI("[MQTT_ROUTE] event to queue len=%u", (unsigned int)entity_msg->Len);
        int ret = Entity_App_Msg_Queue_Send(ENTITY_MSG_TYPE_MQTT_EVENT, (const void*)entity_msg->Payload, entity_msg->Len);
        ENTITY_MQTT_VERBOSE_LOGI("[MQTT_DIAG][REPORT_RESPONSE_ROUTE_DONE] msgid=%u ret=%d len=%u state=%s "
                  "ctx_connected=%d app_connected=%d\r\n",
                  (unsigned int)entity_msg->Msgid,
                  ret,
                  (unsigned int)entity_msg->Len,
                  Entity_Mqtt_App_State_Str(context->State),
                  context->Is_Connected ? 1 : 0,
                  s_mqtt_app_connected ? 1 : 0);
        if (ret != 0) { ENTITY_LOGE("[MQTT_ROUTE] event queue send failed\r\n"); }
    }
    else if(is_cmd_topic)
    {
        ENTITY_MQTT_VERBOSE_LOGI("[MQTT_ROUTE] cmd to queue len=%u", (unsigned int)entity_msg->Len);
        int ret = Entity_App_Msg_Queue_Send(ENTITY_MSG_TYPE_MQTT_COMMAND, (const void*)entity_msg->Payload, entity_msg->Len);
        ENTITY_MQTT_VERBOSE_LOGI("[MQTT_DIAG][CMD_ROUTE_DONE] msgid=%u ret=%d len=%u state=%s "
                  "ctx_connected=%d app_connected=%d\r\n",
                  (unsigned int)entity_msg->Msgid,
                  ret,
                  (unsigned int)entity_msg->Len,
                  Entity_Mqtt_App_State_Str(context->State),
                  context->Is_Connected ? 1 : 0,
                  s_mqtt_app_connected ? 1 : 0);
        if (ret != 0) { ENTITY_LOGE("[MQTT_ROUTE] cmd queue send failed\r\n"); }
    }
    else
    {
        ENTITY_LOGE("[MQTT_ROUTE] unknown topic=%s expected_event=%s expected_cmd=%s\n",
                  entity_msg->Topic,
                  Entity_Mqtt_Get_Topic(TOPIC_TYPE_EVENT_SUBSCRIBE),
                  Entity_Mqtt_Get_Topic(TOPIC_TYPE_CMD_SUBSCRIBE));
    }
}




/**
*@名称 		Entity_Mqtt_Client_Task
*@功能 		MQTT客户端任务
*@参数 		void *arg
*@返回值 	void
*@使用说明	
*/
void Entity_Mqtt_Client_Task(void *arg)
{
    int ret = OPRT_OK;
    Flag_Entity_Mqtt_App_Task_Over = 0;
    Entity_Mqtt_Context_t *entity_context = (Entity_Mqtt_Context_t *)&Entity_Client_Instance;
    memset(entity_context, 0, sizeof(Entity_Mqtt_Context_t));

    Entity_Triple_Info_t *triple_info = Entity_Get_Triple_Info();
    Entity_Mqtt_Topic_Init(triple_info->Pid, triple_info->Uuid);
    Entity_Config_Net_Info_t *config_net_info = Entity_Get_Config_Net_Info();
	Entity_Mqtt_Generate_Client_Config(triple_info->Uuid, triple_info->Secret, &entity_context->Mqtt_Auth);
    ENTITY_LOGW("[MQTT_AUTH_DUMP] pub_topic = \"%s\"\r\n",
              Entity_Mqtt_Get_Topic(TOPIC_TYPE_EVENT_PUBLISH));
    ENTITY_LOGW("[MQTT_AUTH_DUMP] sub_topic = \"%s\"\r\n",
              Entity_Mqtt_Get_Topic(TOPIC_TYPE_EVENT_SUBSCRIBE));
    ENTITY_LOGW("[MQTT_AUTH_DUMP] client_id = \"%s\"\r\n",
              entity_context->Mqtt_Auth.Client_Id);
    ENTITY_LOGW("[MQTT_AUTH_DUMP] username = \"%s\"\r\n",
              entity_context->Mqtt_Auth.Username);
    ENTITY_LOGW("[MQTT_AUTH_DUMP] password = \"<masked>\"\r\n");
	Entity_Mqtt_Config_t entity_config = {0};
	entity_config.Tcp_Connect_Params.Host = config_net_info->Mqtt_Info.Mqtt_Host;
	entity_config.Tcp_Connect_Params.Port = config_net_info->Mqtt_Info.Mqtt_Port;

	/* 开发者模式：覆盖 MQTT 连接地址为本地调试服务器 */
	{
		uint8_t dev_mode_val = 0;
		Entity_Flash_Read_Key_Value(DEV_MODE_NVS_KEY, &dev_mode_val, sizeof(dev_mode_val));
		if (dev_mode_val != 0)
		{
			entity_config.Tcp_Connect_Params.Host = DEV_MODE_MQTT_HOST;
			entity_config.Tcp_Connect_Params.Port = DEV_MODE_MQTT_PORT;
			ENTITY_LOGI("[Dev Mode] MQTT override: %s:%d\r\n", DEV_MODE_MQTT_HOST, DEV_MODE_MQTT_PORT);
		}
	}

    if (entity_config.Tcp_Connect_Params.Host == NULL ||
        entity_config.Tcp_Connect_Params.Host[0] == '\0' ||
        entity_config.Tcp_Connect_Params.Port == 0)
    {
        ENTITY_LOGE("MQTT start failed: reason=empty_connect_address host:%s port:%u\r\n",
                  entity_config.Tcp_Connect_Params.Host ? entity_config.Tcp_Connect_Params.Host : "(null)",
                  (unsigned int)entity_config.Tcp_Connect_Params.Port);
        Flag_Entity_Mqtt_App_Task_Over = 1;
        Flag_Entity_Mqtt_App_Task_Runing = 0;
        Entity_Pthread_Delete(&Mqtt_Client_Thread_Id);
        return;
    }

	entity_config.Tcp_Connect_Params.Timeout_Ms = 10000;    //建立连接和接收一个完整包的超时时间，考虑网络波动和弱网环境，超时时间不能太短
	entity_config.Device_Pid = triple_info->Pid;
	entity_config.Device_Id = triple_info->Uuid;
	entity_config.Device_Secret = triple_info->Secret;
    /* P0: keepalive 10s。esp-mqtt 會在 keepalive/2 發 PINGREQ，PINGRESP timeout
     * 也從 keepalive 派生；這是在 MQTT 層偵測 broker 下行死鏈，不讓 AI token
     * timeout 用 wait_ping/pingreq_age magic number 猜測網路狀態。 */
    entity_config.Keepalive = 10;
	entity_config.Connected_Cb = Entity_Mqtt_Connected_Callback,
	entity_config.Disconnect_Cb = Entity_Mqtt_Disconnected_Callback,
    entity_config.Recv_Messages_Cb = Entity_Mqtt_Recv_Message_Callback,

	ret = Entity_Mqtt_Init(entity_context, &entity_config);
    if(ret != OPRT_OK){
        Flag_Entity_Mqtt_App_Task_Over = 1;
        return;
    }
 #if 0  
    Entity_Mqtt_App_Msg_Reach_Callback_t msg_reach_cbs = {
        .Entity_Mqtt_Msg_Event_Cb = Entity_Mqtt_Msg_Event_Reach_Callback,
        .Entity_Mqtt_Msg_Cmd_Cb = Entity_Mqtt_Msg_Cmd_Reach_Callback,
    };
    Register_Entity_Mqtt_Msg_Reach_Cbs(&msg_reach_cbs);
#endif
    while(1)
    {
        /* coreMQTT-Agent 主循環：
         * 1. 先排空 Agent 指令隊列，避免 request 已在 queue 中仍先進長時間 ProcessLoop。
         * 2. 執行 MQTT 狀態機（連線 / 訂閱 / 短切片 ProcessLoop）。
         * 3. ProcessLoop 返回後處理 connected post work，再排空本輪 callback/外部 task 新送入的指令。
         *    兩個 drain 點都不在 MQTT callback 鏈中，不會重入 MQTT_Publish。
         * 4. 按需睡眠後繼續下一輪。*/
        s_mqtt_task_priority = Entity_Mqtt_App_Current_Task_Priority();
        s_mqtt_task_stack_hwm = Entity_Mqtt_App_Current_Task_Stack_Hwm();
        Entity_Mqtt_App_Drain_Agent_Queue(entity_context);
        uint32_t loop_enter_ms = Entity_Mqtt_App_Now_Ms();
        uint32_t loop_enter_depth = s_agent_queue ? (uint32_t)Entity_Msg_Queue_Get_Msg_Num(&s_agent_queue) : 0;
        int need_sleep_ms = Entity_Mqtt_Loop(entity_context);
        uint32_t loop_exit_ms = Entity_Mqtt_App_Now_Ms();
        uint32_t loop_cost_ms = loop_exit_ms - loop_enter_ms;
        bool should_exit = (entity_context->Prohibit_Connect && !entity_context->Is_Connected);
        Entity_Mqtt_Connected_Post_Process(entity_context);
        uint32_t queue_depth = s_agent_queue ? (uint32_t)Entity_Msg_Queue_Get_Msg_Num(&s_agent_queue) : 0;
        s_mqtt_task_priority = Entity_Mqtt_App_Current_Task_Priority();
        s_mqtt_task_stack_hwm = Entity_Mqtt_App_Current_Task_Stack_Hwm();
        if ((loop_cost_ms >= 1000U) || (loop_enter_depth > 0U) || (queue_depth > 0U))
        {
            ENTITY_LOGI("[MQTT_TRACE][T4_LOOP] enter_ms=%u exit_ms=%u cost_ms=%u "
                        "state=%s ctx_connected=%d app_connected=%d depth_enter=%u depth_exit=%u "
                        "task_prio=%u task_stack_hwm=%u\r\n",
                        (unsigned int)loop_enter_ms,
                        (unsigned int)loop_exit_ms,
                        (unsigned int)loop_cost_ms,
                        Entity_Mqtt_App_State_Str(entity_context->State),
                        entity_context->Is_Connected ? 1 : 0,
                        s_mqtt_app_connected ? 1 : 0,
                        (unsigned int)loop_enter_depth,
                        (unsigned int)queue_depth,
                        (unsigned int)s_mqtt_task_priority,
                        (unsigned int)s_mqtt_task_stack_hwm);
        }

        if (need_sleep_ms > 0)
        {
            ENTITY_LOGW("[MQTT_DIAG][APP_LOOP_SLEEP] state=%s ctx_connected=%d "
                      "app_connected=%d queue_depth=%u sleep=%d\r\n",
                      Entity_Mqtt_App_State_Str(entity_context->State),
                      entity_context->Is_Connected ? 1 : 0,
                      s_mqtt_app_connected ? 1 : 0,
                      (unsigned int)queue_depth,
                      need_sleep_ms);
        }
        else if (!entity_context->Is_Connected && queue_depth > 0)
        {
            uint32_t now_ms = Entity_Mqtt_App_Now_Ms();
            if (now_ms - s_last_queue_wait_log_ms >= 2000)
            {
                s_last_queue_wait_log_ms = now_ms;
                ENTITY_LOGW("[MQTT_DIAG][AGENT_QUEUE_WAIT] state=%s app_connected=%d "
                          "queue_depth=%u\r\n",
                          Entity_Mqtt_App_State_Str(entity_context->State),
                          s_mqtt_app_connected ? 1 : 0,
                          (unsigned int)queue_depth);
            }
        }

        Entity_Mqtt_App_Drain_Agent_Queue(entity_context);

        if (entity_context->Is_Connected && s_agent_queue != NULL &&
            Entity_Msg_Queue_Get_Msg_Num(&s_agent_queue) == 0)
        {
            uint32_t now_ms = Entity_Mqtt_App_Now_Ms();
            if (now_ms - s_last_app_heartbeat_ms >= ENTITY_MQTT_APP_HEARTBEAT_MS)
            {
                s_last_app_heartbeat_ms = now_ms;
                ENTITY_MQTT_VERBOSE_LOGI("[MQTT_DIAG][APP_HEARTBEAT] code=time ack=0 qos=0 interval_ms=%u\r\n",
                          (unsigned int)ENTITY_MQTT_APP_HEARTBEAT_MS);
                Entity_Mqtt_Event_Get_Time_Request_Qos(0, QOS0_MOST_ONCE);
            }
        }

        if (need_sleep_ms > 0)
        {
            Entity_Sleep_Ms(need_sleep_ms);
        }
        if(should_exit)//已断连且禁止重连，则退出任务
        {
            ENTITY_LOGE("mqtt client thread exit\r\n");
            Flag_Entity_Mqtt_App_Task_Over = 1;
            break;
        }
        Entity_Sleep_Ms(10);
    }
    Entity_Pthread_Delete(&Mqtt_Client_Thread_Id);
}

/**
*@名称 		Entity_Mqtt_Client_Task_Start
*@功能 		创建MQTT任务
*@参数 		void
*@返回值 	int
*@使用说明	
*/
int Entity_Mqtt_Client_Task_Start(void)
{
    int ret=0;
    /* 开机 HAL 绑定完整性自检：提前暴露漏注册，避免运行时空指针崩溃 */
    const char *missing = NULL;
    if (!Entity_Iot_Func_All_Bound(&missing))
    {
        ENTITY_LOGE("[MQTT] entity HAL 未绑定: %s\r\n", missing ? missing : "?");
        return -1;
    }
    if (s_cb_mutex == NULL)
        Entity_Mutex_Create(&s_cb_mutex);
    /* 初始化 Agent 指令隊列（冪等，重複呼叫安全）*/
    if (s_agent_queue == NULL)
    {
        Entity_Msg_Queue_Create(&s_agent_queue, ENTITY_MQTT_AGENT_QUEUE_SIZE, sizeof(Entity_Mqtt_Agent_Cmd_t));
        if (s_agent_queue == NULL)
        {
            ENTITY_LOGE("[MQTT] agent queue create failed\r\n");
            return -1;
        }
        ENTITY_LOGI("[MQTT] agent queue created depth=%d\r\n", ENTITY_MQTT_AGENT_QUEUE_SIZE);
    }
    if(Flag_Entity_Mqtt_App_Task_Runing == 0)
    {
        Flag_Entity_Mqtt_App_Task_Runing = 1;
        ret = Entity_Pthread_Create(&Mqtt_Client_Thread_Id, "Entity_Mqtt_Client_Task", ENTITY_MQTT_APP_TASK_STACK_SIZE, ENTITY_MQTT_APP_TASK_PROI, Entity_Mqtt_Client_Task, NULL);
        if(ret != 0) 
        {
            Flag_Entity_Mqtt_App_Task_Runing = 0;
            ENTITY_LOGE(" %s Failed!", __func__);
            return -1;
        }
        ENTITY_LOGD(" %s  success!", __func__);
    }
    else
    {
        ENTITY_LOGE("%s have run!\r\n", __func__);
    }
    return 0;
}

/**
*@名称 		Entity_Mqtt_Client_Task_Stop
*@功能 		删除MQTT任务
*@参数 		void
*@返回值 	int
*@使用说明	
*/
int Entity_Mqtt_Client_Task_Stop(void)
{
    if(Flag_Entity_Mqtt_App_Task_Runing)
    {
        Flag_Entity_Mqtt_App_Task_Runing = 0;
        Entity_Mqtt_Context_t *entity_context = (Entity_Mqtt_Context_t *)&Entity_Client_Instance;
        Entity_Mqtt_Manu_Disconnect(entity_context);
        while(!Flag_Entity_Mqtt_App_Task_Over)//等待任务结束
            Entity_Sleep_Ms(10);
        Entity_Mqtt_Deinit(entity_context);
    }
    return 0;
} 


/**
*@名称 		Entity_Mqtt_App_Topic_Subscribe
*@功能 		MQTT应用订阅主题
*@参数 		Topic_Type_e type_e, int qos
*@返回值 	int                
*@使用说明	
*/
int Entity_Mqtt_App_Topic_Subscribe(Topic_Type_e type_e, int qos)
{
    return Entity_Mqtt_Topic_Subscribe(&Entity_Client_Instance, type_e, qos);
}


/**
*@名称 		Entity_Mqtt_App_Topic_Unsubscribe
*@功能 		MQTT应用取消订阅主题
*@参数 		Topic_Type_e type_e, int qos
*@返回值 	int                
*@使用说明	
*/
int Entity_Mqtt_App_Topic_Unsubscribe(Topic_Type_e type_e, int qos)
{
    return Entity_Mqtt_Topic_Unsubscribe(&Entity_Client_Instance, type_e, qos);
}


/**
*@名称 		Entity_Mqtt_App_Topic_Publish
*@功能 		MQTT应用发布主题
*@参数 		Entity_Mqtt_Context_t* context, Topic_Type_e type_e, const char *data, int len, int qos, int retained
*@返回值 	int           
*@使用说明	
*/
int Entity_Mqtt_App_Topic_Publish(Topic_Type_e type_e, const char *data, int len, int qos, int retained)
{
    /* coreMQTT-Agent 非阻塞發布介面：
     *
     * 呼叫方（Controller task、cloud_sync task、Connected callback 等）將
     * 指令 POST 到 s_agent_queue 後立即返回（timeout=0，永不阻塞呼叫方）。
     * Agent Task 在 Entity_Mqtt_Loop() 前後的安全點排空隊列並執行真正的 Publish。
     *
     * 解決的問題：
     *   1. Connected callback 從 ProcessLoop 回調鏈觸發，若直接呼叫 Publish
     *      會在 MQTT task 內重入 → 同 task 非遞歸 mutex 死鎖 → crash。
     *   2. 外部 Task 的 Publish 阻塞（pthread_mutex_lock 競爭），
     *      導致 Controller Task 卡住 20s。
     *   3. 資料生命週期由 data_copy（PSRAM）延伸至 Agent Task 消費完畢。*/

    if (!s_mqtt_app_connected)
    {
        ENTITY_LOGW("[MQTT_PUB] publish skipped: reason=subscription_not_ready flag=false\r\n");
        ENTITY_LOGW("[MQTT_DIAG][PUB_SKIP_APP_FLAG] state=%s ctx_connected=%d "
                  "queue_depth=%u len=%d qos=%d\r\n",
                  Entity_Mqtt_App_State_Str(Entity_Client_Instance.State),
                  Entity_Client_Instance.Is_Connected ? 1 : 0,
                  s_agent_queue ? (unsigned int)Entity_Msg_Queue_Get_Msg_Num(&s_agent_queue) : 0,
                  len,
                  qos);
        return OPRT_COM_ERROR;
    }
    if (s_agent_queue == NULL)
    {
        ENTITY_LOGE("[MQTT_PUB] agent queue not initialized\r\n");
        return OPRT_COM_ERROR;
    }

    /* 將 data 複製到 PSRAM（Entity_Mem_Malloc），延長生命週期至 Agent Task 消費完畢 */
    void *data_copy = Entity_Mem_Malloc((unsigned int)len);
    if (data_copy == NULL)
    {
        /* PSRAM 分配失敗，降級到 DRAM（保留原行為） */
        data_copy = malloc((size_t)len);
    }
    if (data_copy == NULL)
    {
        ENTITY_LOGE("[MQTT_PUB] memory allocation failed len=%d\r\n", len);
        return OPRT_COM_ERROR;
    }
    memcpy(data_copy, data, (size_t)len);

    Entity_Mqtt_Agent_Cmd_t cmd =
    {
        .type       = AGENT_CMD_PUBLISH,
        .topic_type = type_e,
        .data       = data_copy,
        .len        = len,
        .qos        = qos,
        .retained   = retained,
        .trace_seq  = ++s_agent_trace_seq,
        .enqueue_ms = Entity_Mqtt_App_Now_Ms(),
        .ai_access  = Entity_Mqtt_App_Buffer_Contains(data, len, "agora_agent_device_access"),
    };

    if (Entity_Msg_Queue_Send(&s_agent_queue, &cmd, sizeof(cmd), 0) != 0)
    {
        /* 隊列已滿（16 項），本次丟棄並釋放副本 */
        ENTITY_LOGE("[MQTT_PUB] agent queue full, drop publish topic=%d\r\n", (int)type_e);
        ENTITY_LOGE("[MQTT_DIAG][PUB_ENQUEUE_FULL] topic=%d len=%d qos=%d depth=%u/%u\r\n",
                  (int)type_e,
                  len,
                  qos,
                  (unsigned int)Entity_Msg_Queue_Get_Msg_Num(&s_agent_queue),
                  (unsigned int)ENTITY_MQTT_AGENT_QUEUE_SIZE);
        Entity_Mem_Free(data_copy);
        return OPRT_COM_ERROR;
    }
    if (cmd.ai_access)
    {
        ENTITY_LOGI("[MQTT_TRACE][T3_ENQUEUE] trace=%u enqueue_ms=%u topic_type=%d len=%d qos=%d "
                    "retained=%d depth=%u/%u task_prio=%u task_stack_hwm=%u\r\n",
                    (unsigned int)cmd.trace_seq,
                    (unsigned int)cmd.enqueue_ms,
                    (int)type_e,
                    len,
                    qos,
                    retained,
                    (unsigned int)Entity_Msg_Queue_Get_Msg_Num(&s_agent_queue),
                    (unsigned int)ENTITY_MQTT_AGENT_QUEUE_SIZE,
                    (unsigned int)Entity_Mqtt_App_Current_Task_Priority(),
                    (unsigned int)Entity_Mqtt_App_Current_Task_Stack_Hwm());
    }
    ENTITY_MQTT_VERBOSE_LOGI("[MQTT_DIAG][PUB_ENQUEUE] topic=%d len=%d qos=%d retained=%d "
              "depth=%u/%u\r\n",
              (int)type_e,
              len,
              qos,
              retained,
              (unsigned int)Entity_Msg_Queue_Get_Msg_Num(&s_agent_queue),
              (unsigned int)ENTITY_MQTT_AGENT_QUEUE_SIZE);
    return OPRT_OK;
}

bool Entity_Mqtt_App_Is_Connected(void)
{
    /* 直接讀 volatile 標誌，不持任何鎖，零等待。
     * 標誌由 Connected/Disconnected callback 更新：
     *   Connected  callback → s_mqtt_app_connected = true
     *   Disconnected callback → s_mqtt_app_connected = false（先於重連觸發）
     * 精度足夠：斷線時 flag 立即清除，重連成功後訂閱完成才置位。*/
    return s_mqtt_app_connected;
}

bool Entity_Mqtt_App_Prepare_Ai_Publish(void)
{
    Entity_Mqtt_Context_t *context = &Entity_Client_Instance;
    uint32_t last_rx_age_ms = Mqtt_Client_Last_Rx_Age_Ms(context->Mqtt_Client);
    uint32_t pending_count = 0;
    uint32_t oldest_age_ms = 0;
    uint16_t oldest_msg_id = 0;
    bool stale_puback = false;
    Mqtt_Client_Broker_Liveness_Snapshot_t broker = {0};
    bool broker_ok = false;

    if (!Entity_Mqtt_App_Is_Connected())
    {
        ENTITY_LOGW("[MQTT_DIAG][AI_PUBLISH_NOT_READY] app_connected=0 state=%s ctx_connected=%d\r\n",
                  Entity_Mqtt_App_State_Str(context->State),
                  context->Is_Connected ? 1 : 0);
        return false;
    }

    (void)Mqtt_Client_Pending_Qos1_Snapshot(context->Mqtt_Client,
                                            &pending_count,
                                            &oldest_age_ms,
                                            &oldest_msg_id);
    broker_ok = Mqtt_Client_Broker_Liveness_Snapshot(context->Mqtt_Client, &broker);
    broker.state = Entity_Mqtt_App_Normalize_Broker_State(context, broker.state);
    stale_puback = (pending_count > 0U) &&
                   (oldest_age_ms >= ENTITY_MQTT_AI_PENDING_QOS1_STALE_MS);

    if (broker_ok && Entity_Mqtt_App_Broker_Allows_Ai_Publish(broker.state) && !stale_puback)
    {
        ENTITY_LOGI("[MQTT_DIAG][AI_PUBLISH_READY] broker=%s keepalive=%u pingresp_timeout=%u "
                  "last_inbound_age=%u wait_ping=%d pingreq_age=%u pingresp_rtt=%u "
                  "last_rx_age=%u pending_qos1=%u oldest_msgid=%u oldest_age=%u\r\n",
                  Mqtt_Client_Broker_Liveness_Str(broker.state),
                  (unsigned int)broker.keepalive_ms,
                  (unsigned int)broker.pingresp_timeout_ms,
                  (unsigned int)broker.last_inbound_alive_age_ms,
                  broker.wait_ping ? 1 : 0,
                  (unsigned int)broker.pingreq_age_ms,
                  (unsigned int)broker.last_pingresp_rtt_ms,
                  (unsigned int)last_rx_age_ms,
                  (unsigned int)pending_count,
                  (unsigned int)oldest_msg_id,
                  (unsigned int)oldest_age_ms);
        return true;
    }

    bool request_reconnect = stale_puback ||
                             (broker.state == MQTT_BROKER_LIVENESS_PROBING) ||
                             (broker.state == MQTT_BROKER_LIVENESS_DEAD) ||
                             (broker.state == MQTT_BROKER_LIVENESS_DISCONNECTED);
    ENTITY_LOGW("[MQTT_DIAG][AI_PUBLISH_NOT_READY] broker_ok=%d broker=%s request_reconnect=%d "
              "stale_puback=%d keepalive=%u pingresp_timeout=%u last_inbound_age=%u "
              "wait_ping=%d pingreq_age=%u pingresp_rtt=%u last_rx_age=%u "
              "pending_qos1=%u oldest_msgid=%u oldest_age=%u stale_puback_ms=%u\r\n",
              broker_ok ? 1 : 0,
              Mqtt_Client_Broker_Liveness_Str(broker.state),
              request_reconnect ? 1 : 0,
              stale_puback ? 1 : 0,
              (unsigned int)broker.keepalive_ms,
              (unsigned int)broker.pingresp_timeout_ms,
              (unsigned int)broker.last_inbound_alive_age_ms,
              broker.wait_ping ? 1 : 0,
              (unsigned int)broker.pingreq_age_ms,
              (unsigned int)broker.last_pingresp_rtt_ms,
              (unsigned int)last_rx_age_ms,
              (unsigned int)pending_count,
              (unsigned int)oldest_msg_id,
              (unsigned int)oldest_age_ms,
              (unsigned int)ENTITY_MQTT_AI_PENDING_QOS1_STALE_MS);

    if (request_reconnect)
    {
        Entity_Mqtt_App_Log_Reconnect_Pending_Snapshot("ai_publish_not_ready",
                                        stale_puback ? "ai_publish_pending_qos1" :
                                        (broker.state == MQTT_BROKER_LIVENESS_PROBING) ? "ai_publish_broker_probing" :
                                        (broker.state == MQTT_BROKER_LIVENESS_DEAD) ? "ai_publish_broker_dead" :
                                        "ai_publish_broker_disconnected");
        s_mqtt_app_connected = false;
        (void)Entity_Mqtt_Force_Reconnect(context,
                                        stale_puback ? "ai_publish_pending_qos1" :
                                        (broker.state == MQTT_BROKER_LIVENESS_PROBING) ? "ai_publish_broker_probing" :
                                        (broker.state == MQTT_BROKER_LIVENESS_DEAD) ? "ai_publish_broker_dead" :
                                        "ai_publish_broker_disconnected");
    }
    return false;
}

bool Entity_Mqtt_App_Is_Ai_Link_Healthy(void)
{
    Entity_Mqtt_Context_t *context = &Entity_Client_Instance;
    uint32_t last_rx_age_ms = Mqtt_Client_Last_Rx_Age_Ms(context->Mqtt_Client);
    uint32_t pending_count = 0;
    uint32_t oldest_age_ms = 0;
    uint16_t oldest_msg_id = 0;
    bool stale_puback = false;
    bool healthy = false;
    Mqtt_Client_Broker_Liveness_Snapshot_t broker = {0};
    bool broker_ok = false;

    if (!Entity_Mqtt_App_Is_Connected())
    {
        ENTITY_LOGW("[MQTT_DIAG][AI_LINK_HEALTH] healthy=0 app_connected=0 state=%s ctx_connected=%d\r\n",
                  Entity_Mqtt_App_State_Str(context->State),
                  context->Is_Connected ? 1 : 0);
        return false;
    }

    (void)Mqtt_Client_Pending_Qos1_Snapshot(context->Mqtt_Client,
                                            &pending_count,
                                            &oldest_age_ms,
                                            &oldest_msg_id);
    broker_ok = Mqtt_Client_Broker_Liveness_Snapshot(context->Mqtt_Client, &broker);
    broker.state = Entity_Mqtt_App_Normalize_Broker_State(context, broker.state);
    stale_puback = (pending_count > 0U) &&
                   (oldest_age_ms >= ENTITY_MQTT_AI_PENDING_QOS1_STALE_MS);
    healthy = broker_ok &&
              Entity_Mqtt_App_Broker_Healthy_For_Ai_Wait(broker.state) &&
              !stale_puback;

    ENTITY_LOGI("[MQTT_DIAG][AI_LINK_HEALTH] healthy=%d broker_ok=%d broker=%s "
              "stale_puback=%d keepalive=%u pingresp_timeout=%u last_inbound_age=%u "
              "wait_ping=%d pingreq_age=%u pingresp_rtt=%u last_rx_age=%u "
              "pending_qos1=%u oldest_msgid=%u oldest_age=%u stale_puback_ms=%u\r\n",
              healthy ? 1 : 0,
              broker_ok ? 1 : 0,
              Mqtt_Client_Broker_Liveness_Str(broker.state),
              stale_puback ? 1 : 0,
              (unsigned int)broker.keepalive_ms,
              (unsigned int)broker.pingresp_timeout_ms,
              (unsigned int)broker.last_inbound_alive_age_ms,
              broker.wait_ping ? 1 : 0,
              (unsigned int)broker.pingreq_age_ms,
              (unsigned int)broker.last_pingresp_rtt_ms,
              (unsigned int)last_rx_age_ms,
              (unsigned int)pending_count,
              (unsigned int)oldest_msg_id,
              (unsigned int)oldest_age_ms,
              (unsigned int)ENTITY_MQTT_AI_PENDING_QOS1_STALE_MS);
    return healthy;
}

bool Entity_Mqtt_App_Get_Health_Snapshot(Entity_Mqtt_App_Health_Snapshot_t *snapshot)
{
    if (snapshot == NULL)
    {
        return false;
    }

    memset(snapshot, 0, sizeof(*snapshot));
    Entity_Mqtt_Context_t *context = &Entity_Client_Instance;
    snapshot->app_connected = s_mqtt_app_connected;
    snapshot->ctx_connected = context->Is_Connected;
    snapshot->state = context->State;
    snapshot->keepalive_ms = (uint32_t)context->Config.Keepalive * 1000U;
    snapshot->last_rx_age_ms = Mqtt_Client_Last_Rx_Age_Ms(context->Mqtt_Client);
    snapshot->broker_state = MQTT_BROKER_LIVENESS_DISCONNECTED;
    snapshot->broker_last_inbound_alive_age_ms = UINT32_MAX;
    snapshot->last_pingresp_rtt_ms = UINT32_MAX;
    snapshot->agent_queue_depth = s_agent_queue ? (uint32_t)Entity_Msg_Queue_Get_Msg_Num(&s_agent_queue) : 0U;

    (void)Mqtt_Client_Pending_Qos1_Snapshot(context->Mqtt_Client,
                                            &snapshot->pending_qos1,
                                            &snapshot->oldest_qos1_age_ms,
                                            &snapshot->oldest_qos1_msgid);
    Mqtt_Client_Broker_Liveness_Snapshot_t broker = {0};
    if (Mqtt_Client_Broker_Liveness_Snapshot(context->Mqtt_Client, &broker))
    {
        broker.state = Entity_Mqtt_App_Normalize_Broker_State(context, broker.state);
        snapshot->broker_state = (uint8_t)broker.state;
        snapshot->broker_pingresp_timeout_ms = broker.pingresp_timeout_ms;
        snapshot->broker_last_inbound_alive_age_ms = broker.last_inbound_alive_age_ms;
        snapshot->wait_ping = broker.wait_ping;
        snapshot->pingreq_age_ms = broker.pingreq_age_ms;
        snapshot->last_pingresp_rtt_ms = broker.last_pingresp_rtt_ms;
        snapshot->mqtt_task_progress_age_ms = broker.task_progress_age_ms;
        snapshot->mqtt_task_last_loop_gap_ms = broker.task_last_loop_gap_ms;
        snapshot->mqtt_task_max_loop_gap_ms = broker.task_max_loop_gap_ms;
        snapshot->mqtt_task_last_poll_ms = broker.task_last_poll_ms;
        snapshot->mqtt_task_max_poll_ms = broker.task_max_poll_ms;
        snapshot->mqtt_task_stack_hwm = broker.task_stack_hwm;
        snapshot->mqtt_core_snapshot_wait_ms = broker.core_snapshot_wait_ms;
        snapshot->mqtt_task_core = broker.task_core;
        snapshot->mqtt_task_priority = broker.task_priority;
        snapshot->mqtt_event_count = broker.event_count;
        snapshot->mqtt_last_event_age_ms = broker.last_event_age_ms;
        snapshot->mqtt_callback_active = broker.callback_active;
        snapshot->mqtt_callback_active_age_ms = broker.callback_active_age_ms;
        snapshot->mqtt_callback_last_ms = broker.callback_last_ms;
        snapshot->mqtt_callback_max_ms = broker.callback_max_ms;
        snapshot->mqtt_callback_slow_count = broker.callback_slow_count;
    }
    if (s_mqtt_task_stack_hwm != 0U)
    {
        snapshot->mqtt_task_stack_hwm = s_mqtt_task_stack_hwm;
    }
    if (s_mqtt_task_priority != 0U)
    {
        snapshot->mqtt_task_priority = s_mqtt_task_priority;
    }
    return true;
}

void Entity_Mqtt_App_Reset_Keepalive(void)
{
    /* esp-mqtt 內建 keepalive 會自行維護，此函數保留供舊呼叫點使用。 */
    ENTITY_LOGI("[mqtt] Entity_Mqtt_App_Reset_Keepalive: no-op (esp-mqtt keepalive enabled)\r\n");
}

bool Entity_Mqtt_App_Force_Reconnect(const char *reason)
{
    Entity_Mqtt_App_Log_Reconnect_Pending_Snapshot("force_reconnect", reason);
    s_mqtt_app_connected = false;
    return Entity_Mqtt_Force_Reconnect(&Entity_Client_Instance, reason) == OPRT_OK;
}
