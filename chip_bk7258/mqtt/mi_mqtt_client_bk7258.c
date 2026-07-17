// mi_mqtt_client_bk7258.c — BK7258 mi_mqtt client wrapper (Task 5.1b)
//
// 21 个公共函数的实现，覆盖 5.1a 内嵌的 coreMQTT 引擎（mi_mqtt.c/serializer/state）。
// 公共 API 以 library/mi_mqtt/mi_mqtt_client.h（entity 头）为准，签名逐字匹配。
//
// 适配自 rino 的 mi_mqtt_client.c（同族、同引擎）。差异：
//   - 平台原语用 entity SDK 的 Bsp_*（bsp_system.h），而非 rino_hal。
//   - 互斥量生命周期由本客户端负责：Init 里 Bsp_Mutex_Init，Deinit 里 Bsp_Mutex_Destroy。
//   - getTime 用本地 wrapper 包 Bsp_Get_Run_Time_Ms（匹配 MQTTGetCurrentTimeFunc_t）。
//   - 诊断函数（spec §6 非 BK 移植范围）保守实现：NULL 守卫 + 清零/false/UNKNOWN，不伪造数据。

#include "mi_mqtt_client.h"   // entity 公共 API（含 Config/Message/Liveness/Snapshot 类型）

#include "mi_mqtt.h"          // 内嵌引擎：MQTTContext_t / MQTT_Init / MQTT_Connect / ...
#include "mi_mqtt_port_bk.h"  // hal_malloc / hal_free（→ Bsp_Mem_*）

#include "bsp_system.h"       // Bsp_Mem_*, Bsp_Mutex_*, Bsp_Get_Run_Time_Ms
#include "bsp_network.h"      // Network_Tcp/Tls_*, Transport_Interface_t, Network_Context_t
#include "entity_log.h"
#include "FreeRTOS.h"
#include "task.h"

#include <string.h>
#include <stdbool.h>

#define ENTITY_MQTT_PROCESS_LOOP_SLICE_MS 200U

static bool Mi_Mqtt_Client_Buffer_Contains(const uint8_t *data, size_t len, const char *needle)
{
    size_t needle_len;

    if (data == NULL || len == 0U || needle == NULL)
    {
        return false;
    }

    needle_len = strlen(needle);
    if (needle_len == 0U || len < needle_len)
    {
        return false;
    }

    for (size_t i = 0; i <= len - needle_len; ++i)
    {
        if (memcmp(data + i, needle, needle_len) == 0)
        {
            return true;
        }
    }
    return false;
}

#define MQTT_SEND_DISCONNECT_PACKET 1

// --------------------------------------------------------------------------
// 不透明 context 的实际定义（entity 头中仅前置声明 struct Mqtt_Client_Context）。
//   持有引擎 MQTTContext、网络上下文、Config 副本、收发缓冲区，以及诊断所需字段。
// --------------------------------------------------------------------------
struct Mqtt_Client_Context
{
    MQTTContext_t        Mqtt_Context;            // 内嵌引擎连接上下文
    Network_Context_t    Network;                 // 传输层上下文（TCP / TLS）
    Mqtt_Client_Config_t Config;                  // 配置副本
    uint8_t              Rxbuf[MQTT_RX_BUFFER_SIZE];
    uint8_t              Txbuf[MQTT_TX_BUFFER_SIZE];
    uint32_t             Last_Rx_Ms;              // 最近一次入站包时间戳（诊断用，0 = 尚未收到）
    uint32_t             Last_Alive_Ms;           // 最近一次 broker 活性包时间戳（PUBLISH/PINGRESP）
};

typedef struct Mqtt_Client_Context Mqtt_Client_Context_t;

// --------------------------------------------------------------------------
// getTime 适配：引擎要求 uint32_t(void)，Bsp_Get_Run_Time_Ms 正好匹配。
// --------------------------------------------------------------------------
static uint32_t Mi_Mqtt_Get_Time_Ms(void)
{
    return Bsp_Get_Run_Time_Ms();
}

/**
*@名称        Mi_Mqtt_Event_User_Callback
*@功能        MQTT 客户端数据接收事件回调（引擎 → 应用层路由）
*@参数        struct MQTTContext* pContext
*@参数        struct MQTTPacketInfo* pPacketInfo
*@参数        struct MQTTDeserializedInfo* pDeserializedInfo
*@返回值      void
*/
static void Mi_Mqtt_Event_User_Callback( struct MQTTContext* pContext,
                                         struct MQTTPacketInfo* pPacketInfo,
                                         struct MQTTDeserializedInfo* pDeserializedInfo )
{
    Mqtt_Client_Context_t* context = (Mqtt_Client_Context_t*)pContext->userData;

    // 记录最近入站包时间戳（供 Mqtt_Client_Last_Rx_Age_Ms 计算 age）
    context->Last_Rx_Ms = Bsp_Get_Run_Time_Ms();
    context->Last_Alive_Ms = context->Last_Rx_Ms;

    uint16_t msgid = pDeserializedInfo->packetIdentifier;

    // 服务器发布消息
    if( ( pPacketInfo->type & 0xF0U ) == MQTT_PACKET_TYPE_PUBLISH )
    {
        if (context->Config.On_Message == NULL)
        {
            return;
        }
        // 提取 TOPIC（引擎不保证 NUL 结尾，复制后补零）
        char* topic = hal_malloc(pDeserializedInfo->pPublishInfo->topicNameLength + 1);
        if (topic == NULL)
        {
            return;
        }
        memcpy(topic, pDeserializedInfo->pPublishInfo->pTopicName, pDeserializedInfo->pPublishInfo->topicNameLength);
        topic[pDeserializedInfo->pPublishInfo->topicNameLength] = '\0';
        const Mqtt_Client_Message_t message = {
            .Topic   = topic,
            .Payload = pDeserializedInfo->pPublishInfo->pPayload,
            .Length  = pDeserializedInfo->pPublishInfo->payloadLength,
            .Qos     = pDeserializedInfo->pPublishInfo->qos,
        };
        // 应用层回调
        context->Config.On_Message( context,
                                    msgid,
                                    &message,
                                    context->Config.Userdata);
        hal_free(topic);
    }
    else // 应答
    {
        switch ( pPacketInfo->type )
        {
        case MQTT_PACKET_TYPE_SUBACK:
            LogDebug( ("MQTT_PACKET_TYPE_SUBACK id:%d", msgid) );
            if(context->Config.On_Subscribed)
            {
                context->Config.On_Subscribed(context, msgid, context->Config.Userdata);
            }
            break;

        case MQTT_PACKET_TYPE_UNSUBACK:
            LogDebug( ("MQTT_PACKET_TYPE_UNSUBACK id:%d", msgid) );
            if(context->Config.On_Unsubscribed)
            {
                context->Config.On_Unsubscribed(context, msgid, context->Config.Userdata);
            }
            break;

        case MQTT_PACKET_TYPE_PUBACK:
            LogDebug( ("MQTT_PACKET_TYPE_PUBACK id:%d", msgid) );
            if(context->Config.On_Published)
            {
                context->Config.On_Published(context, msgid, context->Config.Userdata);
            }
            break;

        default:
            LogDebug( ("type:0x%02x, id:%d", pPacketInfo->type, msgid) );
        }
    }
}

/**
*@名称        Mqtt_Client_New
*@功能        创建 MQTT 客户端结构体
*@返回值      void*
*/
void* Mqtt_Client_New(void)
{
    return Bsp_Mem_Calloc(1, sizeof(Mqtt_Client_Context_t));
}

/**
*@名称        Mqtt_Client_Free
*@功能        释放 MQTT 客户端结构体
*@参数        void* client_context
*@返回值      void
*/
void Mqtt_Client_Free(void* client_context)
{
    if(client_context)
    {
        Bsp_Mem_Free(client_context);
    }
}

static void Mqtt_Client_Reset_Runtime_State(Mqtt_Client_Context_t* context, const char *reason)
{
    if (context == NULL)
    {
        return;
    }

    uint32_t now_ms = Bsp_Get_Run_Time_Ms();
    Bsp_Mutex_Lock(&context->Mqtt_Context.mutex, 0xFFFFFFFF);
    context->Mqtt_Context.connectStatus = MQTTNotConnected;
    context->Mqtt_Context.waitingForPingResp = false;
    context->Mqtt_Context.pingReqSendTimeMs = 0U;
    context->Mqtt_Context.pingRespRecvTimeMs = 0U;
    context->Mqtt_Context.ping_rtt_ms = 0U;
    context->Mqtt_Context.lastPacketTime = now_ms;
    context->Mqtt_Context.controlPacketSent = false;
    Bsp_Mutex_Unlock(&context->Mqtt_Context.mutex);
    context->Last_Rx_Ms = 0U;
    context->Last_Alive_Ms = 0U;

    ENTITY_LOGW("[MQTT_DIAG][SESSION_RESET] reason=%s now=%u\r\n",
                reason ? reason : "unknown",
                (unsigned int)now_ms);
}

/**
*@名称        Mqtt_Client_Init
*@功能        MQTT 客户端初始化（建传输接口 + MQTT_Init + 互斥量创建）
*@参数        void* client_context, const Mqtt_Client_Config_t* config
*@返回值      Mqtt_Client_Status_t
*/
Mqtt_Client_Status_t Mqtt_Client_Init(void* client_context, const Mqtt_Client_Config_t* config)
{
    if (client_context == NULL || config == NULL)
    {
        return MQTT_STATUS_INVALID_PARAM;
    }
    Mqtt_Client_Context_t* context = (Mqtt_Client_Context_t*)client_context;
    MQTTStatus_t mqtt_status;
    Transport_Interface_t transport;

    memset(context, 0, sizeof(Mqtt_Client_Context_t)); // 恢复默认值

    // 加载配置副本
    context->Config = *config;

    // 按是否校验服务器证书选择 TLS / TCP 传输
    if (context->Config.Tls_Connect_Params.Cert_Verify)
    {
        int ret = Network_Tls_Init(&context->Network, &context->Config.Tcp_Connect_Params, &context->Config.Tls_Connect_Params);
        if (0 != ret)
        {
            LogError( ("Network_Tls_Init fail:%d", ret) );
            return MQTT_STATUS_NETWORK_INIT_FAILED;
        }
        transport.Network_Context = &context->Network;
        transport.Send = (Transport_Send_f)Network_Tls_Write;
        transport.Recv = (Transport_Recv_f)Network_Tls_Read;
    }
    else
    {
        int ret = Network_Tcp_Init(&context->Network, &context->Config.Tcp_Connect_Params);
        if (0 != ret)
        {
            LogError( ("Network_Tcp_Init fail:%d", ret) );
            return MQTT_STATUS_NETWORK_INIT_FAILED;
        }
        transport.Network_Context = &context->Network;
        transport.Send = (Transport_Send_f)Network_Tcp_Write;
        transport.Recv = (Transport_Recv_f)Network_Tcp_Read;
    }

    // 网络缓冲区
    MQTTFixedBuffer_t network_buffer_rx;
    network_buffer_rx.size    = MQTT_RX_BUFFER_SIZE;
    network_buffer_rx.pBuffer = context->Rxbuf;

    MQTTFixedBuffer_t network_buffer_tx;
    network_buffer_tx.size    = MQTT_TX_BUFFER_SIZE;
    network_buffer_tx.pBuffer = context->Txbuf;

    // 初始化引擎。MQTT_Init 内部对 pContext 做 memset(0)，故互斥量必须在其后创建。
    mqtt_status = MQTT_Init( &context->Mqtt_Context,
                             &transport,
                             Mi_Mqtt_Get_Time_Ms,
                             Mi_Mqtt_Event_User_Callback,
                             &network_buffer_rx,
                             &network_buffer_tx,
                             context );

    if( mqtt_status != MQTTSuccess )
    {
        LogError( ("MQTT init failed: Status = %s.", MQTT_Status_strerror( mqtt_status )) );
        context->Network.Destroy(&context->Network);
        return MQTT_STATUS_NETWORK_INIT_FAILED;
    }

    // 互斥量生命周期由客户端负责（引擎只 lock/unlock，不创建/销毁）。
    Bsp_Mutex_Init(&context->Mqtt_Context.mutex);
    context->Mqtt_Context.timeout_ms = context->Config.Tcp_Connect_Params.Timeout_Ms;
    return MQTT_STATUS_SUCCESS;
}

/**
*@名称        Mqtt_Client_Deinit
*@功能        MQTT 客户端反初始化（销毁互斥量 + 断网 + 销毁传输）
*@参数        void* client_context
*@返回值      Mqtt_Client_Status_t
*/
Mqtt_Client_Status_t Mqtt_Client_Deinit(void* client_context)
{
    if (!client_context) return MQTT_STATUS_SUCCESS;
    Mqtt_Client_Context_t* context = (Mqtt_Client_Context_t*)client_context;
    Bsp_Mutex_Destroy(&context->Mqtt_Context.mutex);
    if (context->Network.Disconnect) context->Network.Disconnect(&context->Network); // 断开网络连接
    if (context->Network.Destroy)    context->Network.Destroy(&context->Network);
    return MQTT_STATUS_SUCCESS;
}

/**
*@名称        Mqtt_Client_Connect
*@功能        MQTT 客户端连接（建立网络连接 → 发送 CONNECT 包）
*@参数        void* client_context
*@返回值      Mqtt_Client_Status_t
*/
Mqtt_Client_Status_t Mqtt_Client_Connect(void* client_context)
{
    Mqtt_Client_Context_t* context = (Mqtt_Client_Context_t*)client_context;
    MQTTStatus_t mqtt_status;

    Mqtt_Client_Reset_Runtime_State(context, "before_connect");

    // 1、建立网络连接 host port
    int ret = context->Network.Connect(&context->Network, &context->Config.Tcp_Connect_Params, NULL);
    if (0 != ret)
    {
        return MQTT_STATUS_NETWORK_CONNECT_FAILED;
    }
    bool pSessionPresent = false;
    Bsp_Mutex_Lock(&context->Mqtt_Context.mutex, 0xFFFFFFFF);

    // 2、发送 MQTT CONNECT 数据包到服务器
    const MQTTConnectInfo_t connect_info = {
        .cleanSession           = true,
        .keepAliveSeconds       = context->Config.Keepalive,
        .pClientIdentifier      = context->Config.Client_Id,
        .clientIdentifierLength = strlen(context->Config.Client_Id),
        .pUserName              = context->Config.Username,
        .userNameLength         = strlen(context->Config.Username),
        .pPassword              = context->Config.Password,
        .passwordLength         = strlen(context->Config.Password)
    };
    mqtt_status = MQTT_Connect( &context->Mqtt_Context,
                                &connect_info,
                                NULL,
                                context->Config.Tcp_Connect_Params.Timeout_Ms,
                                &pSessionPresent );

    Bsp_Mutex_Unlock(&context->Mqtt_Context.mutex);
    if (MQTTSuccess != mqtt_status) // MQTT 连接失败
    {
        LogError( ("mqtt connect err:  %s(%d)", MQTT_Status_strerror(mqtt_status), mqtt_status) );
        context->Network.Disconnect(&context->Network); // 断开网络连接
        if (MQTTNotAuthorized == mqtt_status) // 未授权错误
        {
            return MQTT_STATUS_NOT_AUTHORIZED;
        }
        return MQTT_STATUS_CONNECT_FAILED;
    }
    // 连接 MQTT 成功
    if(context->Config.On_Connected)
    {
        context->Config.On_Connected(context, context->Config.Userdata);
    }

    return MQTT_STATUS_SUCCESS;
}

/**
*@名称        Mqtt_Client_Disconnect
*@功能        MQTT 客户端断开连接（先断 MQTT 会话，再断网络）
*@参数        void* client_context
*@返回值      Mqtt_Client_Status_t
*/
Mqtt_Client_Status_t Mqtt_Client_Disconnect(void* client_context)
{
    Mqtt_Client_Context_t* context = (Mqtt_Client_Context_t*)client_context;
    MQTTStatus_t mqtt_status;
    // 1、先断 MQTT 连接
#ifdef MQTT_SEND_DISCONNECT_PACKET
    Bsp_Mutex_Lock(&context->Mqtt_Context.mutex, 0xFFFFFFFF);
    mqtt_status = MQTT_Disconnect(&context->Mqtt_Context);
    if (MQTTSuccess != mqtt_status)
    {
        LogError( ("mqtt disconnect err: %s(%d)", MQTT_Status_strerror(mqtt_status), mqtt_status) );
    }
    Bsp_Mutex_Unlock(&context->Mqtt_Context.mutex);
#endif
    // 2、再断网络连接
    context->Network.Disconnect(&context->Network);
    Mqtt_Client_Reset_Runtime_State(context, "after_disconnect");
    if(context->Config.On_Disconnected)
    {
        context->Config.On_Disconnected(context, context->Config.Userdata);
    }

    return MQTT_STATUS_SUCCESS;
}

/**
*@名称        Mqtt_Client_Subscribe
*@功能        MQTT 客户端订阅
*@参数        void* client_context, const char* topic, uint8_t qos
*@返回值      uint16_t 包序号（0 表示失败）
*/
uint16_t Mqtt_Client_Subscribe(void* client_context, const char* topic, uint8_t qos)
{
    Mqtt_Client_Context_t* context = (Mqtt_Client_Context_t*)client_context;
    MQTTStatus_t mqtt_status;
    Bsp_Mutex_Lock(&context->Mqtt_Context.mutex, 0xFFFFFFFF);
    uint16_t msgid = MQTT_GetPacketId(&context->Mqtt_Context);
    const MQTTSubscribeInfo_t sub_info = {
        .qos               = qos,
        .pTopicFilter      = topic,
        .topicFilterLength = strlen(topic),
    };
    mqtt_status = MQTT_Subscribe( &context->Mqtt_Context,
                                  &sub_info,
                                  1,
                                  msgid );
    Bsp_Mutex_Unlock(&context->Mqtt_Context.mutex);

    if( mqtt_status != MQTTSuccess )
    {
        LogError( ("Failed to send SUBSCRIBE packet to broker with error = %s.", MQTT_Status_strerror(mqtt_status)) );
        return 0;
    }
    return msgid;
}

/**
*@名称        Mqtt_Client_Unsubscribe
*@功能        MQTT 客户端取消订阅
*@参数        void* client_context, const char* topic, uint8_t qos
*@返回值      uint16_t 包序号（0 表示失败）
*/
uint16_t Mqtt_Client_Unsubscribe(void* client_context, const char* topic, uint8_t qos)
{
    Mqtt_Client_Context_t* context = (Mqtt_Client_Context_t*)client_context;
    MQTTStatus_t mqtt_status;
    Bsp_Mutex_Lock(&context->Mqtt_Context.mutex, 0xFFFFFFFF);
    uint16_t msgid = MQTT_GetPacketId( &context->Mqtt_Context );
    const MQTTSubscribeInfo_t sub_info = {
        .qos               = qos,
        .pTopicFilter      = topic,
        .topicFilterLength = strlen(topic),
    };
    mqtt_status = MQTT_Unsubscribe( &context->Mqtt_Context,
                                    &sub_info,
                                    1,
                                    msgid );
    Bsp_Mutex_Unlock(&context->Mqtt_Context.mutex);
    if( mqtt_status != MQTTSuccess )
    {
        LogError( ("Failed to send UNSUBSCRIBE packet to broker with error = %s.", MQTT_Status_strerror(mqtt_status)) );
        return 0;
    }
    return msgid;
}

/**
*@名称        Mqtt_Client_Publish
*@功能        MQTT 客户端发布
*@参数        void* client_context, const char* topic, const uint8_t* payload, size_t length, uint8_t qos
*@返回值      int 包序号（>0 成功；-1 失败）
*/
int Mqtt_Client_Publish(void* client_context, const char* topic, const uint8_t* payload, size_t length, uint8_t qos)
{
    Mqtt_Client_Context_t* context = (Mqtt_Client_Context_t*)client_context;
    MQTTStatus_t mqtt_status;
    bool ai_access = Mi_Mqtt_Client_Buffer_Contains(payload, length, "agora_agent_device_access");
    uint32_t begin_ms = Bsp_Get_Run_Time_Ms();
    Bsp_Mutex_Lock(&context->Mqtt_Context.mutex, 0xFFFFFFFF);

    uint16_t msgid = MQTT_GetPacketId( &context->Mqtt_Context );
    const MQTTPublishInfo_t pub_info = {
        .qos             = qos,
        .pTopicName      = topic,
        .topicNameLength = strlen(topic),
        .pPayload        = payload,
        .payloadLength   = length,
    };
    if (ai_access)
    {
        ENTITY_LOGI("[MQTT_TRACE][T7_CORE_PUBLISH_BEGIN] mono_ms=%u topic=%s len=%u qos=%u msgid=%u "
                    "task_prio=%u task_stack_hwm=%u\r\n",
                    (unsigned int)begin_ms,
                    topic ? topic : "(null)",
                    (unsigned int)length,
                    (unsigned int)qos,
                    (unsigned int)msgid,
                    (unsigned int)uxTaskPriorityGet(NULL),
                    (unsigned int)uxTaskGetStackHighWaterMark(NULL));
    }
    mqtt_status = MQTT_Publish( &context->Mqtt_Context,
	                                &pub_info,
	                                msgid);

    Bsp_Mutex_Unlock(&context->Mqtt_Context.mutex);
    if (ai_access)
    {
        uint32_t end_ms = Bsp_Get_Run_Time_Ms();
        ENTITY_LOGI("[MQTT_TRACE][T7_CORE_PUBLISH_END] begin_ms=%u end_ms=%u cost_ms=%u "
                    "status=%d msgid=%u len=%u qos=%u\r\n",
                    (unsigned int)begin_ms,
                    (unsigned int)end_ms,
                    (unsigned int)(end_ms - begin_ms),
                    (int)mqtt_status,
                    (unsigned int)msgid,
                    (unsigned int)length,
                    (unsigned int)qos);
    }

    if (MQTTSuccess != mqtt_status)
    {
        LogError( ("Failed to send publish packet to broker with error = %s.", MQTT_Status_strerror(mqtt_status)) );
        return -1;
    }
    return (int)msgid;
}

/**
*@名称        Mqtt_Client_Yield
*@功能        MQTT 客户端数据处理（驱动引擎 ProcessLoop，含保活）
*@参数        void* client_context
*@返回值      Mqtt_Client_Status_t
*/
Mqtt_Client_Status_t Mqtt_Client_Yield(void* client_context)
{
    Mqtt_Client_Context_t* context = (Mqtt_Client_Context_t*)client_context;
    MQTTStatus_t mqtt_status;
    static uint32_t s_last_probing_diag_ms = 0U;
    uint32_t process_loop_timeout_ms = context->Config.Tcp_Connect_Params.Timeout_Ms;
    uint32_t now_ms = context->Mqtt_Context.getTime ? context->Mqtt_Context.getTime() : 0U;
    if (process_loop_timeout_ms > ENTITY_MQTT_PROCESS_LOOP_SLICE_MS)
    {
        process_loop_timeout_ms = ENTITY_MQTT_PROCESS_LOOP_SLICE_MS;
    }
    // 循环从传输接口接收数据包, 并处理保活
    mqtt_status = MQTT_ProcessLoop( &context->Mqtt_Context, process_loop_timeout_ms);
    if (context->Mqtt_Context.pingRespRecvTimeMs != 0U &&
        context->Mqtt_Context.pingRespRecvTimeMs != context->Last_Alive_Ms)
    {
        context->Last_Alive_Ms = context->Mqtt_Context.pingRespRecvTimeMs;
    }
    if( mqtt_status != MQTTSuccess )
    {
        uint32_t ping_age_ms = 0U;
        now_ms = context->Mqtt_Context.getTime ? context->Mqtt_Context.getTime() : now_ms;
        if (context->Mqtt_Context.waitingForPingResp &&
            now_ms >= context->Mqtt_Context.pingReqSendTimeMs)
        {
            ping_age_ms = now_ms - context->Mqtt_Context.pingReqSendTimeMs;
        }
        ENTITY_LOGW("[MQTT_LOOP_DIAG][PROCESS_LOOP_STATUS] return_status=%s(%d) "
                    "mapped_status=%d slice_ms=%u wait_ping=%d ping_age=%u "
                    "ping_sent=%u last_packet=%u ping_rtt=%u last_rx=%u connected=%d\r\n",
                    MQTT_Status_strerror( mqtt_status ),
                    mqtt_status,
                    MQTT_STATUS_NETWORK_TIMEOUT,
                    (unsigned int)process_loop_timeout_ms,
                    context->Mqtt_Context.waitingForPingResp ? 1 : 0,
                    (unsigned int)ping_age_ms,
                    (unsigned int)context->Mqtt_Context.pingReqSendTimeMs,
                    (unsigned int)context->Mqtt_Context.lastPacketTime,
                    (unsigned int)context->Mqtt_Context.ping_rtt_ms,
                    (unsigned int)context->Last_Rx_Ms,
                    context->Mqtt_Context.connectStatus == MQTTConnected ? 1 : 0);
        (void)ping_age_ms;
        Mqtt_Client_Disconnect(context);
        return MQTT_STATUS_NETWORK_TIMEOUT;
    }
    if (context->Mqtt_Context.waitingForPingResp)
    {
        now_ms = context->Mqtt_Context.getTime ? context->Mqtt_Context.getTime() : now_ms;
        if ((s_last_probing_diag_ms == 0U) ||
            (now_ms - s_last_probing_diag_ms >= 5000U))
        {
            uint32_t ping_age_ms = (now_ms >= context->Mqtt_Context.pingReqSendTimeMs) ?
                                   (now_ms - context->Mqtt_Context.pingReqSendTimeMs) : 0U;
            s_last_probing_diag_ms = now_ms;
            LogInfo( ("[MQTT_LOOP_DIAG][PROBING] slice_ms=%u wait_ping=1 ping_age=%u "
                      "ping_sent=%u last_packet=%u ping_rtt=%u",
                      (unsigned int)process_loop_timeout_ms,
                      (unsigned int)ping_age_ms,
                      (unsigned int)context->Mqtt_Context.pingReqSendTimeMs,
                      (unsigned int)context->Mqtt_Context.lastPacketTime,
                      (unsigned int)context->Mqtt_Context.ping_rtt_ms) );
            (void)ping_age_ms;
        }
    }
    else
    {
        s_last_probing_diag_ms = 0U;
    }
    return MQTT_STATUS_SUCCESS;
}

// ==========================================================================
// 诊断 / 接口查询函数
// ==========================================================================

/**
*@名称        Mqtt_Client_Last_Rx_Age_Ms
*@功能        距最近一次入站包的毫秒数（基于回调中记录的时间戳）
*@参数        void* client_context
*@返回值      uint32_t（0 = 无 ctx 或尚未收到任何入站包）
*/
uint32_t Mqtt_Client_Last_Rx_Age_Ms(void* client_context)
{
    Mqtt_Client_Context_t* context = (Mqtt_Client_Context_t*)client_context;
    if (context == NULL || context->Last_Rx_Ms == 0)
    {
        return 0;
    }
    uint32_t now = Bsp_Get_Run_Time_Ms();
    return (now >= context->Last_Rx_Ms) ? (now - context->Last_Rx_Ms) : 0;
}

/**
*@名称        Mqtt_Client_Pending_Qos1_Snapshot
*@功能        QoS1 待确认快照
*@返回值      bool（始终 false）
*@说明        BK conservative — ESP-era diagnostic（spec §6）：清零 out 参，返回 false
*/
bool Mqtt_Client_Pending_Qos1_Snapshot(void* client_context, uint32_t* pending_count, uint32_t* oldest_age_ms, uint16_t* oldest_msg_id)
{
    (void)client_context;
    if (pending_count) *pending_count = 0;
    if (oldest_age_ms) *oldest_age_ms = 0;
    if (oldest_msg_id) *oldest_msg_id = 0;
    return false;
}

/**
*@名称        Mqtt_Client_Pingresp_Snapshot
*@功能        PINGRESP 等待状态快照
*@返回值      bool（始终 false）
*@说明        BK conservative — ESP-era diagnostic（spec §6）
*/
bool Mqtt_Client_Pingresp_Snapshot(void* client_context, bool* wait_ping, uint32_t* pingreq_age_ms)
{
    (void)client_context;
    if (wait_ping)      *wait_ping = false;
    if (pingreq_age_ms) *pingreq_age_ms = 0;
    return false;
}

/**
*@名称        Mqtt_Client_Pingresp_Diag_Snapshot
*@功能        PINGRESP 诊断快照
*@返回值      bool（始终 false）
*@说明        BK conservative — ESP-era diagnostic（spec §6）
*/
bool Mqtt_Client_Pingresp_Diag_Snapshot(void* client_context, bool* wait_ping, uint32_t* pingreq_age_ms, uint32_t* last_pingresp_rtt_ms)
{
    (void)client_context;
    if (wait_ping)            *wait_ping = false;
    if (pingreq_age_ms)       *pingreq_age_ms = 0;
    if (last_pingresp_rtt_ms) *last_pingresp_rtt_ms = 0;
    return false;
}

/**
*@名称        Mqtt_Client_Broker_Liveness_Str
*@功能        将 broker liveness 枚举转为静态字符串
*@参数        Mqtt_Client_Broker_Liveness_t state
*@返回值      const char*（不会返回 NULL）
*/
const char* Mqtt_Client_Broker_Liveness_Str(Mqtt_Client_Broker_Liveness_t state)
{
    switch (state)
    {
    case MQTT_BROKER_LIVENESS_DISCONNECTED: return "DISCONNECTED";
    case MQTT_BROKER_LIVENESS_CONNECTING:   return "CONNECTING";
    case MQTT_BROKER_LIVENESS_SUBSCRIBING:  return "SUBSCRIBING";
    case MQTT_BROKER_LIVENESS_READY:        return "READY";
    case MQTT_BROKER_LIVENESS_PROBING:      return "PROBING";
    case MQTT_BROKER_LIVENESS_DEAD:         return "DEAD";
    default:                                return "UNKNOWN";
    }
}

/**
*@名称        Mqtt_Client_Broker_Liveness_Snapshot
*@功能        broker liveness 快照
*@返回值      bool（ctx/snapshot 均非空才 true）
*@说明        BK: derive liveness from core MQTT keepalive/PINGRESP state.
*/
bool Mqtt_Client_Broker_Liveness_Snapshot(void* client_context, Mqtt_Client_Broker_Liveness_Snapshot_t* snapshot)
{
    if (client_context == NULL || snapshot == NULL)
    {
        return false;
    }
    Mqtt_Client_Context_t* context = (Mqtt_Client_Context_t*)client_context;
    uint32_t now_ms = Bsp_Get_Run_Time_Ms();
    uint32_t keepalive_ms = (uint32_t)context->Config.Keepalive * 1000U;
    uint32_t pingreq_age_ms = 0U;
    uint32_t last_inbound_alive_age_ms = UINT32_MAX;

    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->keepalive_ms = keepalive_ms;
    snapshot->pingresp_timeout_ms = MQTT_PINGRESP_TIMEOUT_MS;
    snapshot->wait_ping = context->Mqtt_Context.waitingForPingResp;
    snapshot->last_pingresp_rtt_ms = context->Mqtt_Context.ping_rtt_ms;

    if (context->Last_Alive_Ms != 0U)
    {
        last_inbound_alive_age_ms = (now_ms >= context->Last_Alive_Ms) ?
                                    (now_ms - context->Last_Alive_Ms) : 0U;
    }
    snapshot->last_inbound_alive_age_ms = last_inbound_alive_age_ms;

    if (context->Mqtt_Context.waitingForPingResp)
    {
        pingreq_age_ms = (now_ms >= context->Mqtt_Context.pingReqSendTimeMs) ?
                         (now_ms - context->Mqtt_Context.pingReqSendTimeMs) : 0U;
    }
    snapshot->pingreq_age_ms = pingreq_age_ms;

    if (context->Mqtt_Context.connectStatus != MQTTConnected)
    {
        snapshot->state = MQTT_BROKER_LIVENESS_DISCONNECTED;
    }
    else if (context->Mqtt_Context.waitingForPingResp)
    {
        snapshot->state = (pingreq_age_ms > MQTT_PINGRESP_TIMEOUT_MS) ?
                          MQTT_BROKER_LIVENESS_DEAD :
                          MQTT_BROKER_LIVENESS_PROBING;
    }
    else if ((keepalive_ms != 0U) &&
             (last_inbound_alive_age_ms != UINT32_MAX) &&
             (last_inbound_alive_age_ms > (keepalive_ms + MQTT_PINGRESP_TIMEOUT_MS)))
    {
        snapshot->state = MQTT_BROKER_LIVENESS_DEAD;
    }
    else
    {
        snapshot->state = MQTT_BROKER_LIVENESS_READY;
    }
    return true;
}

/**
*@名称        Mqtt_Client_Reset_Keepalive
*@功能        重置保活
*@返回值      void
*@说明        BK conservative（spec §6）：引擎内部自管保活，此处 no-op
*/
void Mqtt_Client_Reset_Keepalive(void* client_context)
{
    (void)client_context;
}

/**
*@名称        Mqtt_Client_Interface_Local_Ip
*@功能        获取本地 IP 地址
*@参数        void* client_context, char* buf, int max_size
*@返回值      char*
*/
char* Mqtt_Client_Interface_Local_Ip(void* client_context, char* buf, int max_size)
{
    Mqtt_Client_Context_t* context = (Mqtt_Client_Context_t*)client_context;
    if(buf && context)
        strncpy(buf, context->Network.Interface_Info.Ip, max_size);
    return buf;
}

/**
*@名称        Mqtt_Client_Interface_Mac
*@功能        获取 MAC 地址
*@参数        void* client_context, char* buf, int max_size
*@返回值      char*
*/
char* Mqtt_Client_Interface_Mac(void* client_context, char* buf, int max_size)
{
    Mqtt_Client_Context_t* context = (Mqtt_Client_Context_t*)client_context;
    if(buf && context)
        strncpy(buf, context->Network.Interface_Info.Mac, max_size);
    return buf;
}

/**
*@名称        Mqtt_Client_Interface_Name
*@功能        获取网络接口名称
*@参数        void* client_context, char* buf, int max_size
*@返回值      char*
*/
char* Mqtt_Client_Interface_Name(void* client_context, char* buf, int max_size)
{
    Mqtt_Client_Context_t* context = (Mqtt_Client_Context_t*)client_context;
    if(buf && context)
        strncpy(buf, context->Network.Interface_Info.Name, max_size);
    return buf;
}

/**
*@名称        Mqtt_Client_Ping_Rtt_Ms
*@功能        获取 ping 请求的 RTT（毫秒），由引擎 MQTTContext 维护
*@参数        void* client_context
*@返回值      uint32_t
*/
uint32_t Mqtt_Client_Ping_Rtt_Ms(void* client_context)
{
    Mqtt_Client_Context_t* context = (Mqtt_Client_Context_t*)client_context;
    if (context == NULL)
    {
        return 0;
    }
    return context->Mqtt_Context.ping_rtt_ms;
}
