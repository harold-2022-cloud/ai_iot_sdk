//entity_mqtt_client.c
#include <stdio.h>

#include "entity_mqtt_client.h"

#include "entity_error_code.h"
#include "entity_iot_func.h"
#include "entity_log.h"
#include "com_utils.h"

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

static bool Entity_Mqtt_Client_Buffer_Contains(const char *data, int len, const char *needle)
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

static const char *Mqtt_Status_Str(int status)
{
    switch (status)
    {
        case MQTT_STATUS_SUCCESS:               return "SUCCESS";
        case MQTT_STATUS_INVALID_PARAM:         return "INVALID_PARAM";
        case MQTT_STATUS_CONNECT_FAILED:        return "CONNECT_FAILED";
        case MQTT_STATUS_NOT_AUTHORIZED:        return "NOT_AUTHORIZED";
        case MQTT_STATUS_NETWORK_INIT_FAILED:   return "NETWORK_INIT_FAILED";
        case MQTT_STATUS_NETWORK_CONNECT_FAILED:return "NETWORK_CONNECT_FAILED";
        case MQTT_STATUS_NETWORK_TIMEOUT:       return "NETWORK_TIMEOUT";
        default:                                return "UNKNOWN";
    }
}

static const char *Entity_Mqtt_State_Str(uint8_t state)
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


ENTITY_PSRAM_BSS static char Event_Publish_Topic[ENTITY_MQTT_TOPIC_MAX_LEN];//事件发布主题
ENTITY_PSRAM_BSS static char Event_Subscribe_Topic[ENTITY_MQTT_TOPIC_MAX_LEN];//事件订阅主题
ENTITY_PSRAM_BSS static char Cmd_Publish_Topic[ENTITY_MQTT_TOPIC_MAX_LEN];//命令发布主题
ENTITY_PSRAM_BSS static char Cmd_Subscribe_Topic[ENTITY_MQTT_TOPIC_MAX_LEN];//命令订阅主题

/* 重連指數退避：1s → 2s → 4s → … → 30s */
static uint32_t s_reconnect_delay_ms = 1000;
static uint32_t s_sub_enter_ms = 0;
static uint32_t s_loop_connect_attempt = 0;

static void Entity_Mqtt_Set_Reconnect_Reason(Entity_Mqtt_Context_t *context, const char *reason)
{
	if (context == NULL) {
		return;
	}
	const char *safe_reason = (reason != NULL) ? reason : "unknown";
	snprintf(context->Reconnect_Reason, sizeof(context->Reconnect_Reason), "%s", safe_reason);
}

static void Entity_Mqtt_Log_Reconnect_Core_Snapshot(Entity_Mqtt_Context_t *context,
                                                    const char *stage,
                                                    const char *reason)
{
    uint32_t last_rx_age_ms = 0U;
    uint32_t pending_qos1 = 0U;
    uint32_t oldest_qos1_age_ms = 0U;
    uint16_t oldest_qos1_msgid = 0U;
    Mqtt_Client_Broker_Liveness_Snapshot_t broker = {0};
    bool broker_ok = false;

    if (context == NULL || context->Mqtt_Client == NULL)
    {
        ENTITY_LOGW("[MQTT_DIAG][RECONNECT_REASON_SNAPSHOT] stage=%s reason=%s context_ready=0\r\n",
                  stage ? stage : "unknown",
                  reason ? reason : "unknown");
        return;
    }

    last_rx_age_ms = Mqtt_Client_Last_Rx_Age_Ms(context->Mqtt_Client);
    (void)Mqtt_Client_Pending_Qos1_Snapshot(context->Mqtt_Client,
                                            &pending_qos1,
                                            &oldest_qos1_age_ms,
                                            &oldest_qos1_msgid);
    broker_ok = Mqtt_Client_Broker_Liveness_Snapshot(context->Mqtt_Client, &broker);

    ENTITY_LOGW("[MQTT_DIAG][RECONNECT_REASON_SNAPSHOT] stage=%s reason=%s "
              "state=%s ctx_connected=%d broker_ok=%d broker=%s keepalive=%u pingresp_timeout=%u "
              "wait_ping=%d ping_age=%u ping_rtt=%u last_inbound_age=%u last_rx_age=%u "
              "pending_qos1=%u oldest_msgid=%u oldest_age=%u reconnect_pending=%d prohibit=%d\r\n",
              stage ? stage : "unknown",
              reason ? reason : "unknown",
              Entity_Mqtt_State_Str(context->State),
              context->Is_Connected ? 1 : 0,
              broker_ok ? 1 : 0,
              Mqtt_Client_Broker_Liveness_Str(broker.state),
              (unsigned int)broker.keepalive_ms,
              (unsigned int)broker.pingresp_timeout_ms,
              broker.wait_ping ? 1 : 0,
              (unsigned int)broker.pingreq_age_ms,
              (unsigned int)broker.last_pingresp_rtt_ms,
              (unsigned int)broker.last_inbound_alive_age_ms,
              (unsigned int)last_rx_age_ms,
              (unsigned int)pending_qos1,
              (unsigned int)oldest_qos1_msgid,
              (unsigned int)oldest_qos1_age_ms,
              context->Reconnect_Pending ? 1 : 0,
              context->Prohibit_Connect ? 1 : 0);
}

/**
*@名称 		Entity_Mqtt_Topic_Init
*@功能 		TOPIC初始化
*@参数 		const char* pid, const char* uuid
*@返回值 	void
*@使用说明	
*/
void Entity_Mqtt_Topic_Init(const char* pid, const char* uuid)
{
    snprintf(Event_Publish_Topic, sizeof(Event_Publish_Topic)-1, "rlink/v2/%s/%s/report", pid, uuid);
    snprintf(Event_Subscribe_Topic, sizeof(Event_Subscribe_Topic)-1, "rlink/v2/%s/%s/report_response", pid, uuid);
    snprintf(Cmd_Publish_Topic, sizeof(Cmd_Publish_Topic)-1, "rlink/v2/%s/%s/issue_response", pid, uuid);
    snprintf(Cmd_Subscribe_Topic, sizeof(Cmd_Subscribe_Topic)-1, "rlink/v2/%s/%s/issue", pid, uuid);

    ENTITY_LOGI(" %s event_publish_topic: %s\n", __func__, Event_Publish_Topic);
    ENTITY_LOGI(" %s event_subscribe_topic: %s\n", __func__, Event_Subscribe_Topic);
    ENTITY_LOGI(" %s command_publish_topic: %s\n", __func__, Cmd_Publish_Topic);
    ENTITY_LOGI(" %s command_subscribe_topic: %s\n", __func__, Cmd_Subscribe_Topic);
}

/**
*@名称 		Entity_Register_Mqtt_Topic_Init
*@功能 		动态注册TOPIC初始化
*@参数 		const char* pid, const char* uuid
*@返回值 	void
*@使用说明	
*/
void Entity_Register_Mqtt_Topic_Init(const char* pid, const char* uuid)
{
    snprintf(Event_Publish_Topic, sizeof(Event_Publish_Topic)-1, "register/v2/%s/%s/register", pid, uuid);
    snprintf(Event_Subscribe_Topic, sizeof(Event_Subscribe_Topic)-1, "register/v2/%s/%s/register_response", pid, uuid);
    ENTITY_LOGI("%s event_publish_topic: %s\n", __func__, Event_Publish_Topic);
    ENTITY_LOGI("%s event_subscribe_topic: %s\n", __func__, Event_Subscribe_Topic);
}

/**
*@名称 		Entity_Mqtt_Get_Topic
*@功能 		根据主题类型查找主题内容
*@参数 		Topic_Type_e type_e     主题类型
*@返回值 	char                    主题内容
*@使用说明	主题内容与三元组信息相关
*/
char *Entity_Mqtt_Get_Topic(Topic_Type_e type_e)
{
    char *topic = NULL;
    switch (type_e)
    {
        case TOPIC_TYPE_EVENT_PUBLISH:
            topic = Event_Publish_Topic;
            break;
        case TOPIC_TYPE_EVENT_SUBSCRIBE:
            topic = Event_Subscribe_Topic;
            break;
        case TOPIC_TYPE_CMD_PUBLISH:
            topic = Cmd_Publish_Topic;
            break;
        case TOPIC_TYPE_CMD_SUBSCRIBE:
            topic = Cmd_Subscribe_Topic;
            break;
        default:
            break;
    }

    return topic;
}

/**　
*@名称 		Entity_Mqtt_Subscribe_Complete_Process
*@功能 		订阅完成的处理　
*@参数 		Entity_Mqtt_Context_t *context
*@返回值 	void
*@使用说明	
*/
static int Entity_Mqtt_Subscribe_Complete_Process(Entity_Mqtt_Context_t* context)
{
	uint8_t prev_state = context->State;
	ENTITY_LOGI("[MQTT_DIAG][SUB_COMPLETE] state=%s last_id=%u is_connected_before=%d\r\n",
	          Entity_Mqtt_State_Str(prev_state),
	          (unsigned int)context->Last_Subscribe_Id,
	          context->Is_Connected ? 1 : 0);
	/* 实际以自动订阅成功作为连接成功的标志 */
	context->Is_Connected = true;
	s_reconnect_delay_ms = 1000;    /* 連線成功，重置退避計時器 */
	if (context->Config.Connected_Cb) //此回调中可以进入相关的上报及请求
	{
		context->Config.Connected_Cb(context, context->User_Data);
	}
	context->State = ENTITY_MQTT_YIELD_STATE;
	ENTITY_LOGI("[MQTT_DIAG][STATE] %s -> %s by SUB_COMPLETE\r\n",
	          Entity_Mqtt_State_Str(prev_state),
	          Entity_Mqtt_State_Str(context->State));

	return OPRT_OK;
}

/**
*@名称 		Mqtt_Client_Connected_Callback
*@功能 	    MQTT客户端连接成功的处理 
*@参数 		void* client_context MQTT客户端信息
*@参数 		void* userdata      用户数据
*@返回值 	void
*@使用说明	连接成功进行订阅操作
*/
static void Mqtt_Client_Connected_Callback(void* client_context, void* userdata)
{
	Entity_Mqtt_Context_t* entity_mqtt_context = (Entity_Mqtt_Context_t*)userdata;
	ENTITY_LOGI("mqtt client connected!\r\n");
	ENTITY_LOGI("[MQTT_DIAG][CONNECTED_CB] state=%s is_connected=%d last_id=%u prohibit=%d\r\n",
	          Entity_Mqtt_State_Str(entity_mqtt_context->State),
	          entity_mqtt_context->Is_Connected ? 1 : 0,
	          (unsigned int)entity_mqtt_context->Last_Subscribe_Id,
	          entity_mqtt_context->Prohibit_Connect ? 1 : 0);
	uint16_t sub_id1 = Mqtt_Client_Subscribe(client_context, Event_Subscribe_Topic, (uint8_t)MQTT_QOS_1);
	ENTITY_LOGI("[MQTT_SUB] subscribe event topic: %s, msgId=%d%s\r\n",
	          Event_Subscribe_Topic, sub_id1, sub_id1 == 0 ? " [FAILED]" : "");
	entity_mqtt_context->Last_Subscribe_Id = sub_id1;

	uint16_t sub_id2 = Mqtt_Client_Subscribe(client_context, Cmd_Subscribe_Topic, (uint8_t)MQTT_QOS_1);
	ENTITY_LOGI("[MQTT_SUB] subscribe cmd topic: %s, msgId=%d%s\r\n",
	          Cmd_Subscribe_Topic, sub_id2, sub_id2 == 0 ? " [FAILED]" : "");
	entity_mqtt_context->Last_Subscribe_Id = sub_id2;
	ENTITY_LOGI("[MQTT_DIAG][SUB_REQ_DONE] state=%s event_id=%u cmd_id=%u wait_last=%u\r\n",
	          Entity_Mqtt_State_Str(entity_mqtt_context->State),
	          (unsigned int)sub_id1,
	          (unsigned int)sub_id2,
	          (unsigned int)entity_mqtt_context->Last_Subscribe_Id);

}

/**
*@名称 		Mqtt_Client_Disconnected_Callback
*@功能 	    MQTT客户端断开连接的处理 
*@参数 		void* client_context MQTT客户端信息
*@参数 		void* userdata      用户数据
*@返回值 	void
*@使用说明	
*/
static void Mqtt_Client_Disconnected_Callback(void* client_context, void* userdata)
{
	(void)client_context;
	Entity_Mqtt_Context_t* entity_mqtt_context = (Entity_Mqtt_Context_t*)userdata;
	ENTITY_LOGI("mqtt client disconnected!\r\n");
    uint8_t prev_state = entity_mqtt_context->State;
    ENTITY_LOGW("[MQTT_DIAG][DISCONNECTED_CB] state=%s is_connected_before=%d prohibit=%d last_id=%u\r\n",
              Entity_Mqtt_State_Str(prev_state),
              entity_mqtt_context->Is_Connected ? 1 : 0,
              entity_mqtt_context->Prohibit_Connect ? 1 : 0,
              (unsigned int)entity_mqtt_context->Last_Subscribe_Id);
    entity_mqtt_context->Is_Connected = false;
    //断连的应用层回调
	if (entity_mqtt_context->Config.Disconnect_Cb)
	{
		entity_mqtt_context->Config.Disconnect_Cb(entity_mqtt_context, entity_mqtt_context->User_Data);
	}
	if (entity_mqtt_context->Prohibit_Connect == false)
	{
		entity_mqtt_context->State = ENTITY_MQTT_CONNCET_STATE;//重入连接状态
	}
    else
    {
        entity_mqtt_context->State = ENTITY_MQTT_IDLE_STATE;//手动停止则进入空闲
    }
    ENTITY_LOGW("[MQTT_DIAG][STATE] %s -> %s by DISCONNECTED_CB\r\n",
              Entity_Mqtt_State_Str(prev_state),
              Entity_Mqtt_State_Str(entity_mqtt_context->State));
}

/**
*@名称 		Mqtt_Client_Message_Callback
*@功能 	    MQTT客户端消息到达的处理 
*@参数 		void* client_context MQTT客户端信息
*@参数 		uint16_t msgid
*@参数 		const Mqtt_Client_Message_t* msg
*@参数 		void* userdata      用户数据
*@返回值 	void
*@使用说明	
*/
static void Mqtt_Client_Message_Callback(void* client_context, uint16_t msgid, const Mqtt_Client_Message_t* msg, void* userdata)
{
	(void)client_context;
	Entity_Mqtt_Context_t* context = (Entity_Mqtt_Context_t*)userdata;

	Entity_Mqtt_Message_t entity_msg = {0};
	entity_msg.Msgid = msgid;
	entity_msg.Topic = msg->Topic;
	entity_msg.Qos = msg->Qos;
	entity_msg.Payload = (const char*)msg->Payload;
	entity_msg.Len = msg->Length;
    ENTITY_LOGI("[MQTT_TRACE][T9_RX_CALLBACK] mono_ms=%u msgid=%u qos=%d topic=%s len=%u\r\n",
                (unsigned int)Entity_Get_Run_Time_Ms(),
                (unsigned int)msgid,
                msg->Qos,
                msg->Topic ? msg->Topic : "(null)",
                (unsigned int)msg->Length);
	if (context->Config.Recv_Messages_Cb) {
		context->Config.Recv_Messages_Cb(context, context->User_Data, &entity_msg);
	}
}

/**
*@名称 		Mqtt_Client_Subscribed_Callback
*@功能 	    MQTT客户端订阅成功的回调的处理 
*@参数 		void* client_context MQTT客户端信息
*@参数 		uint16_t msgid
*@参数 		void* userdata      用户数据
*@返回值 	void
*@使用说明	
*/
static void Mqtt_Client_Subscribed_Callback(void* client_context, uint16_t msgid, void* userdata)
{
	(void)client_context;
	Entity_Mqtt_Context_t* context = (Entity_Mqtt_Context_t*)userdata;

    uint32_t now_ms = Entity_Get_Run_Time_Ms();
    uint32_t wait_ms = s_sub_enter_ms ? now_ms - s_sub_enter_ms : 0;
	ENTITY_LOGI("[MQTT_SUB] received SUBACK msgId=%d waiting_last_id=%d\r\n", msgid, context->Last_Subscribe_Id);
	ENTITY_LOGI("[MQTT_DIAG][SUBACK] msgId=%u wait_last=%u matched=%d state=%s "
	          "sub_wait_ms=%u is_connected=%d\r\n",
	          (unsigned int)msgid,
	          (unsigned int)context->Last_Subscribe_Id,
	          msgid == context->Last_Subscribe_Id ? 1 : 0,
	          Entity_Mqtt_State_Str(context->State),
	          (unsigned int)wait_ms,
	          context->Is_Connected ? 1 : 0);
	if (msgid == context->Last_Subscribe_Id) //是最后一个订阅成功
	{
        uint8_t prev_state = context->State;
		context->Last_Subscribe_Id = 0;
        s_sub_enter_ms = 0;
		ENTITY_LOGI("subscribe completed.\r\n");
		context->State = ENTITY_MQTT_SUBSCRIBE_COMPLETE_STATE;//订阅完成
		ENTITY_LOGI("[MQTT_DIAG][STATE] %s -> %s by SUBACK\r\n",
		          Entity_Mqtt_State_Str(prev_state),
		          Entity_Mqtt_State_Str(context->State));

		/* 由于现在超时接收时间比较长，如果设置成订阅完成状态需要等待tcp剩余超时时间结束才能执行订阅完成状态
		 * 所以直接把订阅完成状态要执行的处理放到此处，避免需要等待剩余超时时间结束才回调已连接函数，提高处理速度 */
		Entity_Mqtt_Subscribe_Complete_Process(context);//进行相关上报后，直接进入到轮询状态
	}
}

/**
*@名称 		Mqtt_Client_Publish_Callback
*@功能 	    MQTT客户端发布成功的回调的处理 
*@参数 		void* client_context MQTT客户端信息
*@参数 		uint16_t msgid
*@参数 		void* userdata      用户数据
*@返回值 	void
*@使用说明	
*/
static void Mqtt_Client_Publish_Callback(void* client_context, uint16_t msgid, void* userdata)
{
	(void)client_context;
	(void)userdata;
	ENTITY_LOGI("[MQTT_PUBACK] QoS1 handshake done msgId=%d\r\n", msgid);
}

/**
*@名称 		Entity_Mqtt_Init
*@功能 	    犀云MQTT客户端初始化
*@参数 		Entity_Mqtt_Context_t* context
*@参数 		const Entity_Mqtt_Config_t* config
*@返回值 	int
*@使用说明	
*/
int Entity_Mqtt_Init(Entity_Mqtt_Context_t* context, const Entity_Mqtt_Config_t* config)
{
    Mqtt_Client_Status_t mqtt_status;
	context->Config = *config;//加载配置 

	context->Mqtt_Client = Mqtt_Client_New();//申请MQTT客户端
	if (context->Mqtt_Client == NULL) {
		ENTITY_LOGE("mqtt client new fault.\r\n");
		return OPRT_MALLOC_FAILED;
	}

	/* MQTT Client init */
	const Mqtt_Client_Config_t mqtt_config = {
		.Tls_Connect_Params = config->Tls_Connect_Params,
		.Tcp_Connect_Params = config->Tcp_Connect_Params,
		.Keepalive = config->Keepalive,
		.Client_Id = context->Mqtt_Auth.Client_Id,
		.Username = context->Mqtt_Auth.Username,
		.Password = context->Mqtt_Auth.Password,
		.On_Connected = Mqtt_Client_Connected_Callback,
		.On_Disconnected = Mqtt_Client_Disconnected_Callback,
		.On_Message = Mqtt_Client_Message_Callback,
		.On_Subscribed = Mqtt_Client_Subscribed_Callback,
		.On_Published = Mqtt_Client_Publish_Callback,
		.Userdata = context,
	};
    char password_mask[32];
    Utils_Mask_Secret(context->Mqtt_Auth.Password, password_mask, sizeof(password_mask));
	ENTITY_LOGI("[MQTT_AUTH] ClientId=%s Username=%s PasswordMask=%s\r\n",
	          context->Mqtt_Auth.Client_Id ? context->Mqtt_Auth.Client_Id : "(null)",
	          context->Mqtt_Auth.Username  ? context->Mqtt_Auth.Username  : "(null)",
	          password_mask);
	ENTITY_LOGI("[MQTT_DIAG][ENTITY_INIT] host=%s port=%u keepalive=%u state=%s\r\n",
	          config->Tcp_Connect_Params.Host ? config->Tcp_Connect_Params.Host : "(null)",
	          (unsigned int)config->Tcp_Connect_Params.Port,
	          (unsigned int)config->Keepalive,
	          Entity_Mqtt_State_Str(context->State));
	mqtt_status = Mqtt_Client_Init(context->Mqtt_Client, &mqtt_config);
    if( mqtt_status != MQTT_STATUS_SUCCESS ) {
        ENTITY_LOGE( "MQTT init failed: Status = %d.", mqtt_status);
		return OPRT_COM_ERROR;
    }
	context->Prohibit_Connect = false;
	context->State = ENTITY_MQTT_CONNCET_STATE;
	return OPRT_OK;
}

/**
*@名称 		Entity_Mqtt_Connect
*@功能 	    连接犀云MQTT客户端
*@参数 		Entity_Mqtt_Context_t* context
*@返回值 	int
*@使用说明	
*/
int Entity_Mqtt_Connect(Entity_Mqtt_Context_t* context)
{
	if (context == NULL) {
		return OPRT_INVALID_PARM;
	}
	Mqtt_Client_Status_t mqtt_status;

	ENTITY_LOGI("Start mqtt connect...\r\n");
	ENTITY_LOGI("[MQTT_DIAG][ENTITY_CONNECT_API] state=%s is_connected=%d prohibit=%d\r\n",
	          Entity_Mqtt_State_Str(context->State),
	          context->Is_Connected ? 1 : 0,
	          context->Prohibit_Connect ? 1 : 0);
	context->Prohibit_Connect = false;
	mqtt_status = Mqtt_Client_Connect(context->Mqtt_Client);
	if (mqtt_status != MQTT_STATUS_SUCCESS) {
		ENTITY_LOGE("MQTT connect failed: Status = %d.\r\n", mqtt_status);
		return OPRT_LINK_CORE_MQTT_CONNECT_ERROR;
	}
	ENTITY_LOGW("[MQTT_DIAG][ENTITY_CONNECT_API_DONE] state_after_callback=%s -> SUBSCRIBING\r\n",
	          Entity_Mqtt_State_Str(context->State));
	context->State = ENTITY_MQTT_SUBSCRIBING_STATE;
	return OPRT_OK;
}

/**
*@名称 		Entity_Mqtt_Manu_Disconnect
*@功能 	    犀云MQTT客户端主动断开连接
*@参数 		Entity_Mqtt_Context_t* context
*@返回值 	int
*@使用说明	当需要主动销毁客户端的时候
*/
int Entity_Mqtt_Manu_Disconnect(Entity_Mqtt_Context_t* context)
{
	if (context == NULL) {
		return OPRT_INVALID_PARM;
	}
	Mqtt_Client_Status_t mqtt_status;
	mqtt_status = Mqtt_Client_Disconnect(context->Mqtt_Client);
	ENTITY_LOGD("MQTT disconnect result:%d\r\n", mqtt_status);
	context->Prohibit_Connect = true;
	return OPRT_OK;
}

int Entity_Mqtt_Force_Reconnect(Entity_Mqtt_Context_t* context, const char *reason)
{
	if (context == NULL) {
		return OPRT_INVALID_PARM;
	}

	const char *safe_reason = (reason != NULL) ? reason : "unknown";
	if (context->Reconnect_Pending ||
	    context->State == ENTITY_MQTT_CONNCET_STATE)
	{
		return OPRT_OK;
	}
    Entity_Mqtt_Log_Reconnect_Core_Snapshot(context, "request", safe_reason);
	Entity_Mqtt_Set_Reconnect_Reason(context, safe_reason);
	context->Reconnect_Pending = true;
	context->Prohibit_Connect = false;
	ENTITY_LOGW("[MQTT_DIAG][RECONNECT_PENDING] reason=%s state=%s is_connected=%d last_id=%u prohibit=%d\r\n",
	          safe_reason,
	          Entity_Mqtt_State_Str(context->State),
	          context->Is_Connected ? 1 : 0,
	          (unsigned int)context->Last_Subscribe_Id,
	          context->Prohibit_Connect ? 1 : 0);
	return OPRT_OK;
}

/**
*@名称 		Entity_Mqtt_Loop
*@功能 	    犀云MQTT客户端工作状态机
*@参数 		Entity_Mqtt_Context_t* context
*@返回值 	int
*@使用说明	
*/
int Entity_Mqtt_Loop(Entity_Mqtt_Context_t* context)
{
	if (context == NULL) {
		return OPRT_INVALID_PARM;
	}
	int ret = OPRT_OK;
	if (context->Reconnect_Pending)
	{
		uint8_t prev_state = context->State;
		uint16_t prev_last_id = context->Last_Subscribe_Id;
		bool should_disconnect = context->Is_Connected ||
		                         (prev_state == ENTITY_MQTT_SUBSCRIBING_STATE) ||
		                         (prev_state == ENTITY_MQTT_SUBSCRIBE_COMPLETE_STATE) ||
		                         (prev_state == ENTITY_MQTT_YIELD_STATE);
		const char *reason = context->Reconnect_Reason[0] ?
		                     context->Reconnect_Reason : "unknown";
        Entity_Mqtt_Log_Reconnect_Core_Snapshot(context, "begin", reason);
		context->State = ENTITY_MQTT_CONNCET_STATE;
		context->Reconnect_Pending = false;
		context->Prohibit_Connect = false;
		context->Is_Connected = false;
		context->Last_Subscribe_Id = 0;
		s_sub_enter_ms = 0;
		ENTITY_LOGW("[MQTT_DIAG][RECONNECT_BEGIN] reason=%s state=%s last_id=%u prohibit=%d "
		          "disconnect=%d\r\n",
		          reason,
		          Entity_Mqtt_State_Str(prev_state),
		          (unsigned int)prev_last_id,
		          context->Prohibit_Connect ? 1 : 0,
		          should_disconnect ? 1 : 0);
		Mqtt_Client_Status_t mqtt_status = MQTT_STATUS_SUCCESS;
		if (should_disconnect)
		{
			mqtt_status = Mqtt_Client_Disconnect(context->Mqtt_Client);
		}
		ENTITY_LOGW("[MQTT_DIAG][STATE] %s -> %s by RECONNECT_PENDING status=%d(%s)\r\n",
		          Entity_Mqtt_State_Str(prev_state),
		          Entity_Mqtt_State_Str(context->State),
		          (int)mqtt_status,
		          Mqtt_Status_Str((int)mqtt_status));
		return 0;
	}
	switch (context->State) 
	{
		case ENTITY_MQTT_IDLE_STATE:		//空闲状态
			break;

		case ENTITY_MQTT_CONNCET_STATE:	//连接中状态
		{
			uint8_t state_before_connect = context->State;
			s_loop_connect_attempt++;
			ENTITY_LOGI("[MQTT_DIAG][LOOP_CONNECT] attempt=%u state=%s is_connected=%d "
			          "last_id=%u prohibit=%d\r\n",
			          (unsigned int)s_loop_connect_attempt,
			          Entity_Mqtt_State_Str(context->State),
			          context->Is_Connected ? 1 : 0,
			          (unsigned int)context->Last_Subscribe_Id,
			          context->Prohibit_Connect ? 1 : 0);
			Mqtt_Client_Status_t conn_ret = Mqtt_Client_Connect(context->Mqtt_Client);
			if(conn_ret == MQTT_STATUS_SUCCESS )
			{
				uint8_t state_after_callback = context->State;
				if (state_after_callback == ENTITY_MQTT_YIELD_STATE)
				{
					ENTITY_LOGW("[MQTT_DIAG][STATE_RACE] connect returned after callbacks already "
					          "set state=YIELD; loop will set SUBSCRIBING\r\n");
				}
				ENTITY_LOGW("[MQTT_DIAG][STATE] %s -> %s by LOOP_CONNECT_SUCCESS "
				          "(after_callback=%s)\r\n",
				          Entity_Mqtt_State_Str(state_before_connect),
				          Entity_Mqtt_State_Str(ENTITY_MQTT_SUBSCRIBING_STATE),
				          Entity_Mqtt_State_Str(state_after_callback));
				context->State = ENTITY_MQTT_SUBSCRIBING_STATE;
			}
			else
			{
				ENTITY_LOGE("[MQTT] connect failed: status=%d(%s), retry_after_ms=%u\r\n",
				          (int)conn_ret, Mqtt_Status_Str((int)conn_ret), (unsigned int)s_reconnect_delay_ms);
				/* 退避延遲由外層（鎖外）執行，此處只返回需等待的 ms */
				ret = (int)s_reconnect_delay_ms;
				/* 指數退避，上限 30s */
				s_reconnect_delay_ms = s_reconnect_delay_ms * 2 > 30000 ? 30000 : s_reconnect_delay_ms * 2;
			}
			break;
		}
			
		case ENTITY_MQTT_SUBSCRIBING_STATE:	//等待订阅主题成功应答
		{
            /* 記錄進入 SUBSCRIBING_STATE 的時間，超過 15s 未收 SUBACK → 主動斷線重連 */
            if (s_sub_enter_ms == 0) {
                s_sub_enter_ms = Entity_Get_Run_Time_Ms();
                ENTITY_LOGI("[MQTT_SUB] subscribe wait enter event=%s cmd=%s\r\n",
                          Entity_Mqtt_Get_Topic(TOPIC_TYPE_EVENT_SUBSCRIBE),
                          Entity_Mqtt_Get_Topic(TOPIC_TYPE_CMD_SUBSCRIBE));
                ENTITY_LOGW("[MQTT_DIAG][SUB_WAIT_ENTER] state=%s last_id=%u is_connected=%d "
                          "prohibit=%d\r\n",
                          Entity_Mqtt_State_Str(context->State),
                          (unsigned int)context->Last_Subscribe_Id,
                          context->Is_Connected ? 1 : 0,
                          context->Prohibit_Connect ? 1 : 0);
            }
            Mqtt_Client_Status_t yield_ret = Mqtt_Client_Yield(context->Mqtt_Client);
			if( yield_ret != MQTT_STATUS_SUCCESS)//轮询失败，MQTT已执行断连，并将状态切到连接
			{
				/* 当检测到错误，延时再进入 ENTITY_STATE_CONNECTING 状态，避免出现连接断开又立马连上云端的问题出现；
				因为云端对于上线下线等消息是并行处理，可能存在后上线的消息比先离线的消息以及关联处理先完成，导致设备一直处于离线的状态，实际设备在线 */
                uint8_t prev_state = context->State;
                ENTITY_LOGW("[MQTT_DIAG][SUB_WAIT_YIELD_FAIL] status=%d(%s) state=%s "
                          "last_id=%u is_connected=%d\r\n",
                          (int)yield_ret,
                          Mqtt_Status_Str((int)yield_ret),
                          Entity_Mqtt_State_Str(context->State),
                          (unsigned int)context->Last_Subscribe_Id,
                          context->Is_Connected ? 1 : 0);
                context->Is_Connected = false;
                context->Last_Subscribe_Id = 0;
                context->State = context->Prohibit_Connect ?
                                 ENTITY_MQTT_IDLE_STATE : ENTITY_MQTT_CONNCET_STATE;
                ENTITY_LOGW("[MQTT_DIAG][STATE] %s -> %s by SUB_WAIT_YIELD_FAIL\r\n",
                          Entity_Mqtt_State_Str(prev_state),
                          Entity_Mqtt_State_Str(context->State));
                s_sub_enter_ms = 0;
				ret = 5000;  /* 告知外層（鎖外）sleep 5s */
			}
            else if (s_sub_enter_ms != 0 && (Entity_Get_Run_Time_Ms() - s_sub_enter_ms > 15000))
            {
                /* 15s 仍未收到 SUBACK，視為 RX 路徑異常，主動斷線觸發重連
                 * 注意：s_sub_enter_ms == 0 代表 SUBACK 已收到（callback 重置），
                 * 必須先判斷非零才能計算超時，否則收到 SUBACK 後 ProcessLoop
                 * 跑完 10s 返回時 (now - 0) >> 15000 會觸發假陽性斷線。*/
                uint8_t prev_state = context->State;
                ENTITY_LOGE("[MQTT_SUB] SUBACK wait timeout over 15s, force disconnect/reconnect\r\n");
                ENTITY_LOGE("[MQTT_DIAG][SUB_WAIT_TIMEOUT] elapsed=%u state=%s last_id=%u "
                          "is_connected=%d prohibit=%d\r\n",
                          (unsigned int)(Entity_Get_Run_Time_Ms() - s_sub_enter_ms),
                          Entity_Mqtt_State_Str(context->State),
                          (unsigned int)context->Last_Subscribe_Id,
                          context->Is_Connected ? 1 : 0,
                          context->Prohibit_Connect ? 1 : 0);
                s_sub_enter_ms = 0;
                Mqtt_Client_Disconnect(context->Mqtt_Client);
                context->Is_Connected = false;
                context->Last_Subscribe_Id = 0;
                context->State = context->Prohibit_Connect ?
                                 ENTITY_MQTT_IDLE_STATE : ENTITY_MQTT_CONNCET_STATE;
                ENTITY_LOGW("[MQTT_DIAG][STATE] %s -> %s by SUB_WAIT_TIMEOUT_FALLBACK\r\n",
                          Entity_Mqtt_State_Str(prev_state),
                          Entity_Mqtt_State_Str(context->State));
                ret = 5000;  /* 告知外層（鎖外）sleep 5s */
            }
			break;
		}

		case ENTITY_MQTT_SUBSCRIBE_COMPLETE_STATE://此步骤实际上被跳过
		{
			//Entity_Mqtt_Subscribe_Complete_Process(context);
			break;
		}

		case ENTITY_MQTT_YIELD_STATE://轮询
		{
            Mqtt_Client_Status_t yield_ret = Mqtt_Client_Yield(context->Mqtt_Client);
			if( yield_ret != MQTT_STATUS_SUCCESS)
			{
				/*当检测到错误，延时再进入 ENTITY_STATE_CONNECTING 状态，避免出现连接断开又立马连上云端的问题出现；
				因为云端对于上线下线等消息是并行处理，可能存在后上线的消息比先离线的消息以及关联处理先完成，导致设备一直处于离线的状态，实际设备在线 */
				uint8_t prev_state = context->State;
				ENTITY_LOGW("[MQTT_DIAG][YIELD_STATE_FAIL] status=%d(%s) state=%s "
				          "is_connected=%d prohibit=%d\r\n",
				          (int)yield_ret,
				          Mqtt_Status_Str((int)yield_ret),
				          Entity_Mqtt_State_Str(context->State),
				          context->Is_Connected ? 1 : 0,
				          context->Prohibit_Connect ? 1 : 0);
				context->Is_Connected = false;
				context->Last_Subscribe_Id = 0;
				context->State = context->Prohibit_Connect ?
				                 ENTITY_MQTT_IDLE_STATE : ENTITY_MQTT_CONNCET_STATE;
				ENTITY_LOGW("[MQTT_DIAG][STATE] %s -> %s by YIELD_STATE_FAIL\r\n",
				          Entity_Mqtt_State_Str(prev_state),
				          Entity_Mqtt_State_Str(context->State));
				ret = 5000;  /* 告知外層（鎖外）sleep 5s */
			}
			break;
		}
		default:
			break;
	}

	return ret;
}


/**
*@名称 		Entity_Mqtt_Deinit
*@功能 	    犀云MQTT客户端反初始化
*@参数 		Entity_Mqtt_Context_t* context
*@返回值 	int
*@使用说明	
*/
int Entity_Mqtt_Deinit(Entity_Mqtt_Context_t* context)
{
	if (context == NULL) {
		return OPRT_COM_ERROR;
	}
	Mqtt_Client_Status_t mqtt_status = Mqtt_Client_Deinit(context->Mqtt_Client);
	Mqtt_Client_Free(context->Mqtt_Client);
	context->Mqtt_Client = NULL;
	context->State = ENTITY_MQTT_IDLE_STATE;
	if (mqtt_status != MQTT_STATUS_SUCCESS) {
		return OPRT_COM_ERROR;
	}
	return OPRT_OK;
}

/**
*@名称 		Entity_Mqtt_Is_Connected
*@功能 	    犀云MQTT客户端是否已连接
*@参数 		Entity_Mqtt_Context_t* context
*@返回值 	bool
*@使用说明	
*/
bool Entity_Mqtt_Is_Connected(Entity_Mqtt_Context_t* context)
{
	if (context == NULL || context->State == ENTITY_MQTT_IDLE_STATE) {
		return false;
	}
	return context->Is_Connected;
}

void Entity_Mqtt_Reset_Keepalive(Entity_Mqtt_Context_t *context)
{
    /* esp-mqtt 內建 keepalive 會自行維護，保留此 no-op 供舊呼叫點使用。 */
    (void)context;
}



/**
*@名称 		Entity_Mqtt_Topic_Subscribe
*@功能 		订阅主题
*@参数 		Entity_Mqtt_Context_t* context,Topic_Type_e type_e, int qos
*@返回值 	int                
*@使用说明	
*/
int Entity_Mqtt_Topic_Subscribe(Entity_Mqtt_Context_t* context,Topic_Type_e type_e, int qos)
{
    int ret = 0;
    switch (type_e)
    {
        case TOPIC_TYPE_EVENT_SUBSCRIBE:
        case TOPIC_TYPE_CMD_SUBSCRIBE:
            ret = Mqtt_Client_Subscribe(context->Mqtt_Client, Entity_Mqtt_Get_Topic(type_e), (uint8_t)qos);
            break;
        default:
            return -1;
    }
    ENTITY_LOGD("subscribe topic:%s ret:%d", Entity_Mqtt_Get_Topic(type_e), ret);
    return ret;
}

/**
*@名称 		Entity_Mqtt_Topic_Unsubscribe
*@功能 		MQTT客户端取消订阅主题
*@参数 		Entity_Mqtt_Context_t* context, Topic_Type_e type_e, int qos
*@返回值 	int                
*@使用说明	
*/
int Entity_Mqtt_Topic_Unsubscribe(Entity_Mqtt_Context_t* context, Topic_Type_e type_e, int qos)
{
    int ret = 0;
    switch (type_e)
    {
        case TOPIC_TYPE_EVENT_SUBSCRIBE:
        case TOPIC_TYPE_CMD_SUBSCRIBE:
            ret = Mqtt_Client_Unsubscribe(context->Mqtt_Client, Entity_Mqtt_Get_Topic(type_e), (uint8_t)qos);
            break;
        default:
            return -1;
    }
    ENTITY_LOGD("unsubscribe topic:%s ret:%d", Entity_Mqtt_Get_Topic(type_e), ret);
    return ret;
}

/**
*@名称 		Entity_Mqtt_Topic_Publish
*@功能 		发布主题
*@参数 		Entity_Mqtt_Context_t* context, Topic_Type_e type_e, const char *data, int len, int qos, int retained
*@返回值 	int           
*@使用说明	
*/
int Entity_Mqtt_Topic_Publish(Entity_Mqtt_Context_t* context, Topic_Type_e type_e, const char *data, int len, int qos, int retained)
{

    int rc = 0;
    uint32_t begin_ms = Entity_Get_Run_Time_Ms();
    bool ai_access = Entity_Mqtt_Client_Buffer_Contains(data, len, "agora_agent_device_access");
	if(!Entity_Mqtt_Is_Connected(context))
	{
		ENTITY_LOGE("[MQTT] publish failed: reason=mqtt_not_connected state=%d\r\n", (int)context->State);
		ENTITY_LOGE("[MQTT_DIAG][PUB_NOT_CONNECTED] state=%s is_connected=%d "
		          "last_id=%u prohibit=%d len=%d qos=%d\r\n",
		          Entity_Mqtt_State_Str(context->State),
		          context->Is_Connected ? 1 : 0,
		          (unsigned int)context->Last_Subscribe_Id,
		          context->Prohibit_Connect ? 1 : 0,
		          len,
		          qos);
		return -1;
	}
	
	    switch (type_e)
	    {
	        case TOPIC_TYPE_EVENT_PUBLISH:  //事件上报发布
	        case TOPIC_TYPE_CMD_PUBLISH:    //命令应答发布
	        {
                if (ai_access)
                {
                    ENTITY_LOGI("[MQTT_TRACE][T6_TOPIC_BEGIN] mono_ms=%u topic=%s len=%d qos=%d retained=%d\r\n",
                                (unsigned int)begin_ms,
                                Entity_Mqtt_Get_Topic(type_e),
                                len,
                                qos,
                                retained);
                }
	            rc = Mqtt_Client_Publish(context->Mqtt_Client, Entity_Mqtt_Get_Topic(type_e), (const uint8_t*)data, len, qos);
	            break;
	        }
        default:
            return -1;
    }
	    bool publish_ok = (rc > 0) || ((qos == QOS0_MOST_ONCE) && (rc == 0));
    if (ai_access)
    {
        uint32_t end_ms = Entity_Get_Run_Time_Ms();
        ENTITY_LOGI("[MQTT_TRACE][T6_TOPIC_END] begin_ms=%u end_ms=%u cost_ms=%u rc=%d ok=%d len=%d qos=%d\r\n",
                    (unsigned int)begin_ms,
                    (unsigned int)end_ms,
                    (unsigned int)(end_ms - begin_ms),
                    rc,
                    publish_ok ? 1 : 0,
                    len,
                    qos);
    }
    if (!publish_ok)
    {
        ENTITY_LOGI("[MQTT_PUB] publish failed topic=%s rc=%d\r\n", Entity_Mqtt_Get_Topic(type_e), rc);
    }
    else
    {
        if (qos > 0)
        {
            ENTITY_MQTT_VERBOSE_LOGI("[MQTT_PUB] publish submitted topic=%s msgId=%d waiting_qos%d_puback=1\r\n",
                      Entity_Mqtt_Get_Topic(type_e), rc, qos);
        }
        else
        {
            ENTITY_MQTT_VERBOSE_LOGI("[MQTT_PUB] publish submitted topic=%s ret=%d qos0_wait_puback=0\r\n",
                      Entity_Mqtt_Get_Topic(type_e), rc);
        }
    }
    return rc;
}
