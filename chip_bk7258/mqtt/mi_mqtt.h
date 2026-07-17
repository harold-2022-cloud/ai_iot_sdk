//mi_mqtt.h
#pragma once


//如果未声明不用自定义配置，则使用自定义配置文件
#ifndef MQTT_DO_NOT_USE_CUSTOM_CONFIG
    //在其他头文件之前包含自定义配置文件
    #include "mi_mqtt_custom_config.h"
#endif

//定义一些自定义头文件中没有的参数
#include "mi_mqtt_config_defaults.h"

//包含MQTT序列化程序库
#include "mi_mqtt_serializer.h"



//根据MQTT v3.1.1规范，零是无效的数据包标识符。
#define MQTT_PACKET_ID_INVALID    ( ( uint16_t ) 0U )

/* Structures defined in this file. */
struct MQTTPubAckInfo;
struct MQTTContext;
struct MQTTDeserializedInfo;

//应用程序提供回调以检索中的当前时间毫秒。
typedef uint32_t (* MQTTGetCurrentTimeFunc_t )( void );

//用于接收传入发布和传入ACK的应用程序回调
typedef void (* MQTTEventCallback_t )( struct MQTTContext * pContext,
                                       struct MQTTPacketInfo * pPacketInfo,
                                       struct MQTTDeserializedInfo * pDeserializedInfo );

//指示MQTT连接是否存在的值
typedef enum MQTTConnectionStatus
{
    MQTTNotConnected, //连接无效
    MQTTConnected     //已连接
} MQTTConnectionStatus_t;

//MQTT状态
typedef enum MQTTPublishState
{
    MQTTStateNull = 0,  /**< @brief An empty state with no corresponding PUBLISH. */
    MQTTPublishSend,    /**< @brief The library will send an outgoing PUBLISH packet. */
    MQTTPubAckSend,     /**< @brief The library will send a PUBACK for a received PUBLISH. */
    MQTTPubRecSend,     /**< @brief The library will send a PUBREC for a received PUBLISH. */
    MQTTPubRelSend,     /**< @brief The library will send a PUBREL for a received PUBREC. */
    MQTTPubCompSend,    /**< @brief The library will send a PUBCOMP for a received PUBREL. */
    MQTTPubAckPending,  /**< @brief The library is awaiting a PUBACK for an outgoing PUBLISH. */
    MQTTPubRecPending,  /**< @brief The library is awaiting a PUBREC for an outgoing PUBLISH. */
    MQTTPubRelPending,  /**< @brief The library is awaiting a PUBREL for an incoming PUBLISH. */
    MQTTPubCompPending, /**< @brief The library is awaiting a PUBCOMP for an outgoing PUBLISH. */
    MQTTPublishDone     /**< @brief The PUBLISH has been completed. */
} MQTTPublishState_t;

//用于确认QoS 1或QoS 2发布的数据包类型
typedef enum MQTTPubAckType
{
    MQTTPuback, /**< @brief PUBACKs are sent in response to a QoS 1 PUBLISH. */
    MQTTPubrec, /**< @brief PUBRECs are sent in response to a QoS 2 PUBLISH. */
    MQTTPubrel, /**< @brief PUBRELs are sent in response to a PUBREC. */
    MQTTPubcomp /**< @brief PUBCOMPs are sent in response to a PUBREL. */
} MQTTPubAckType_t;

//订阅请求的SUBACK响应中的状态代码
typedef enum MQTTSubAckStatus
{
    MQTTSubAckSuccessQos0 = 0x00, /**< @brief Success with a maximum delivery at QoS 0 . */
    MQTTSubAckSuccessQos1 = 0x01, /**< @brief Success with a maximum delivery at QoS 1. */
    MQTTSubAckSuccessQos2 = 0x02, /**< @brief Success with a maximum delivery at QoS 2. */
    MQTTSubAckFailure = 0x80      /**< @brief Failure. */
} MQTTSubAckStatus_t;

//元素记录了QoS 1或QoS 2的发布
typedef struct MQTTPubAckInfo
{
    uint16_t packetId;               /**< @brief The packet ID of the original PUBLISH. */
    MQTTQoS_t qos;                   /**< @brief The QoS of the original PUBLISH. */
    MQTTPublishState_t publishState; /**< @brief The current state of the publish process. */
} MQTTPubAckInfo_t;

//MQTT连接的结构体
typedef struct MQTTContext
{
    MQTTPubAckInfo_t outgoingPublishRecords[ MQTT_STATE_ARRAY_MAX_COUNT ];//即将发出的发布记录
    MQTTPubAckInfo_t incomingPublishRecords[ MQTT_STATE_ARRAY_MAX_COUNT ];//接收到的发布记录
    Transport_Interface_t transportInterface;//MQTT连接使用的传输接口
    MQTTFixedBuffer_t networkBuffer;//连接  网络发送和接收数据包的缓冲区
    MQTTFixedBuffer_t networkBufferTX;//发布、订阅 网络发送数据包的缓冲区
    uint16_t nextPacketId;//MQTT数据包的下一个可用ID
    MQTTConnectionStatus_t connectStatus;//当前连接状态
    MQTTGetCurrentTimeFunc_t getTime;//用于获取毫秒时间戳的函数
    MQTTEventCallback_t appCallback;//用于向应用程序提供反序列化的MQTT数据包回调函数
    uint32_t lastPacketTime;//库发送的最后一个数据包的时间戳
    bool controlPacketSent;//库是否在调用#MQTT_ProcessLoop或MQTT_ReceiveLoop期间发送了数据包
    uint16_t keepAliveIntervalSec;//保活周期秒数
    uint32_t pingReqSendTimeMs;   //发送最后一个PING数据包的时间戳
    bool waitingForPingResp;      //等待PING应答的标志
    uint32_t pingRespRecvTimeMs;  //收到最后一个PINGRESP数据包的时间戳
    void * userData;//用户数据

    void* mutex;
    uint32_t timeout_ms;//接收一个完整包的超时时间
    uint32_t ping_rtt_ms;//mqtt ping 请求的 RTT 时间，单位：ms
} MQTTContext_t;

//反序列化数据包信息的结构
typedef struct MQTTDeserializedInfo
{
    uint16_t packetIdentifier;          /**< @brief Packet ID of deserialized packet. */
    MQTTPublishInfo_t * pPublishInfo;   /**< @brief Pointer to deserialized publish info. */
    MQTTStatus_t deserializationResult; /**< @brief Return code of deserialization. */
} MQTTDeserializedInfo_t;

//初始化MQTT上下文
MQTTStatus_t MQTT_Init( MQTTContext_t * pContext,
                        const Transport_Interface_t * pTransportInterface,
                        MQTTGetCurrentTimeFunc_t getTimeFunction,
                        MQTTEventCallback_t userCallback,
                        const MQTTFixedBuffer_t * pNetworkBuffer,
                        const MQTTFixedBuffer_t * pNetworkBufferTX,
                        void * userData );

//建立MQTT会话
MQTTStatus_t MQTT_Connect( MQTTContext_t * pContext,
                           const MQTTConnectInfo_t * pConnectInfo,
                           const MQTTPublishInfo_t * pWillInfo,
                           uint32_t timeoutMs,
                           bool * pSessionPresent );

//将给定主题列表的MQTT SUBSCRIBE发送到代理
MQTTStatus_t MQTT_Subscribe( MQTTContext_t * pContext,
                             const MQTTSubscribeInfo_t * pSubscriptionList,
                             size_t subscriptionCount,
                             uint16_t packetId );

//将消息发布到给定的主题名称
MQTTStatus_t MQTT_Publish( MQTTContext_t * pContext,
                           const MQTTPublishInfo_t * pPublishInfo,
                           uint16_t packetId );

//向代理发送MQTT PING请求
MQTTStatus_t MQTT_Ping( MQTTContext_t * pContext );

//将给定主题列表的MQTT UNSUBSCRIBE发送到代理
MQTTStatus_t MQTT_Unsubscribe( MQTTContext_t * pContext,
                               const MQTTSubscribeInfo_t * pSubscriptionList,
                               size_t subscriptionCount,
                               uint16_t packetId );

//断开MQTT会话
MQTTStatus_t MQTT_Disconnect( MQTTContext_t * pContext );

//循环以从传输接口接收数据包, 并处理保活
MQTTStatus_t MQTT_ProcessLoop( MQTTContext_t * pContext,
                               uint32_t timeoutMs );

//循环以从传输接口接收数据包。不处理保活
MQTTStatus_t MQTT_ReceiveLoop( MQTTContext_t * pContext,
                               uint32_t timeoutMs );

//获取根据MQTT 3.1.1规范有效的数据包ID
uint16_t MQTT_GetPacketId( MQTTContext_t * pContext );

//用于确定传递的主题过滤器和根据MQTT 3.1.1协议规范，主题名称匹配
MQTTStatus_t MQTT_MatchTopic( const char * pTopicName,
                              const uint16_t topicNameLength,
                              const char * pTopicFilter,
                              const uint16_t topicFilterLength,
                              bool * pIsMatch );

//解析包含状态代码的MQTT SUBACK数据包的有效载荷对应于原始主题筛选器订阅请求,订阅数据包。
MQTTStatus_t MQTT_GetSubAckStatusCodes( const MQTTPacketInfo_t * pSubackPacket,
                                        uint8_t ** pPayloadStart,
                                        size_t * pPayloadSize );


//MQTT状态的字符串转换错误代码
const char * MQTT_Status_strerror( MQTTStatus_t status );
