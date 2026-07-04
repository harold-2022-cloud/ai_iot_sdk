//mi_mqtt_serializer.h
#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * @cond DOXYGEN_IGNORE
 * Doxygen should ignore this section.
 */

/* bool is defined in only C99+. */
#if defined( __cplusplus ) || ( defined( __STDC_VERSION__ ) && ( __STDC_VERSION__ >= 199901L ) )
    #include <stdbool.h>
#elif !defined( bool ) && !defined( false ) && !defined( true )
    #define bool     int8_t
    #define false    ( int8_t ) 0
    #define true     ( int8_t ) 1
#endif
/** @endcond */

//如果未声明不用自定义配置，则使用自定义配置文件
#ifndef MQTT_DO_NOT_USE_CUSTOM_CONFIG
    //在其他头文件之前包含自定义配置文件
    #include "mi_mqtt_custom_config.h"
#endif

//定义一些自定义头文件中没有的参数
#include "mi_mqtt_config_defaults.h"

#include "hal_network_common.h"



#define MQTT_PACKET_TYPE_CONNECT        ( ( uint8_t ) 0x10U )  /**< @brief CONNECT (client-to-server). */
#define MQTT_PACKET_TYPE_CONNACK        ( ( uint8_t ) 0x20U )  /**< @brief CONNACK (server-to-client). */
#define MQTT_PACKET_TYPE_PUBLISH        ( ( uint8_t ) 0x30U )  /**< @brief PUBLISH (bidirectional). */
#define MQTT_PACKET_TYPE_PUBACK         ( ( uint8_t ) 0x40U )  /**< @brief PUBACK (bidirectional). */
#define MQTT_PACKET_TYPE_PUBREC         ( ( uint8_t ) 0x50U )  /**< @brief PUBREC (bidirectional). */
#define MQTT_PACKET_TYPE_PUBREL         ( ( uint8_t ) 0x62U )  /**< @brief PUBREL (bidirectional). */
#define MQTT_PACKET_TYPE_PUBCOMP        ( ( uint8_t ) 0x70U )  /**< @brief PUBCOMP (bidirectional). */
#define MQTT_PACKET_TYPE_SUBSCRIBE      ( ( uint8_t ) 0x82U )  /**< @brief SUBSCRIBE (client-to-server). */
#define MQTT_PACKET_TYPE_SUBACK         ( ( uint8_t ) 0x90U )  /**< @brief SUBACK (server-to-client). */
#define MQTT_PACKET_TYPE_UNSUBSCRIBE    ( ( uint8_t ) 0xA2U )  /**< @brief UNSUBSCRIBE (client-to-server). */
#define MQTT_PACKET_TYPE_UNSUBACK       ( ( uint8_t ) 0xB0U )  /**< @brief UNSUBACK (server-to-client). */
#define MQTT_PACKET_TYPE_PINGREQ        ( ( uint8_t ) 0xC0U )  /**< @brief PINGREQ (client-to-server). */
#define MQTT_PACKET_TYPE_PINGRESP       ( ( uint8_t ) 0xD0U )  /**< @brief PINGRESP (server-to-client). */
#define MQTT_PACKET_TYPE_DISCONNECT     ( ( uint8_t ) 0xE0U )  /**< @brief DISCONNECT (client-to-server). */



//MQTT PUBACK、PUBREC、PUBREL和PUBCOMP数据包的大小。
#define MQTT_PUBLISH_ACK_PACKET_SIZE    ( 4UL )

/* Structures defined in this file. */
struct MQTTFixedBuffer;
struct MQTTConnectInfo;
struct MQTTSubscribeInfo;
struct MQTTPublishInfo;
struct MQTTPacketInfo;

//MQTT函数的返回码
typedef enum MQTTStatus
{
    MQTTSuccess = 0,     /**< Function completed successfully. */
    MQTTBadParameter,    /**< At least one parameter was invalid. */
    MQTTNoMemory,        /**< A provided buffer was too small. */
    MQTTSendFailed,      /**< The transport send function failed. */
    MQTTRecvFailed,      /**< The transport receive function failed. */
    MQTTBadResponse,     /**< An invalid packet was received from the server. */
    MQTTServerRefused,   /**< The server refused a CONNECT or SUBSCRIBE. */
    MQTTNoDataAvailable, /**< No data available from the transport interface. */
    MQTTIllegalState,    /**< An illegal state in the state record. */
    MQTTStateCollision,  /**< A collision with an existing state record entry. */
    MQTTKeepAliveTimeout, /**< Timeout while waiting for PINGRESP. */
    MQTTNotAuthorized /**< onnection refused: not authorized. */
} MQTTStatus_t;

//MQTT服务质量值
typedef enum MQTTQoS
{
    MQTTQoS0 = 0, /**< Delivery at most once. */
    MQTTQoS1 = 1, /**< Delivery at least once. */
    MQTTQoS2 = 2  /**< Delivery exactly once. */
} MQTTQoS_t;

//传递给MQTT库的缓冲区, 这些缓冲区不会被复制，且在程序运行期间必须保持在作用域内
typedef struct MQTTFixedBuffer
{
    uint8_t * pBuffer; /**< @brief Pointer to buffer. */
    size_t size;       /**< @brief Size of buffer. */
} MQTTFixedBuffer_t;

//MQTT连接参数
typedef struct MQTTConnectInfo
{
    bool cleanSession;//是建立一个新的、干净的会话，还是恢复之前的会话 
    uint16_t keepAliveSeconds;//保活周期
    const char * pClientIdentifier;//MQTT客户端标识符
    uint16_t clientIdentifierLength;
    const char * pUserName;
    uint16_t userNameLength;
    const char * pPassword;
    uint16_t passwordLength;
} MQTTConnectInfo_t;

//订阅信息
typedef struct MQTTSubscribeInfo
{
    MQTTQoS_t qos;//订阅服务质量
    const char * pTopicFilter;//要订阅的主题
    uint16_t topicFilterLength;//订阅主题的长度
} MQTTSubscribeInfo_t;

//发布信息
typedef struct MQTTPublishInfo
{
    MQTTQoS_t qos;//服务质量
    bool retain;//是否为保留消息
    bool dup;//是否是重复的发布消息
    const char * pTopicName;//发布消息的主题名称
    uint16_t topicNameLength;//发布消息的主题名称长度
    const void * pPayload;//消息内容
    size_t payloadLength;//消息长度
} MQTTPublishInfo_t;

//包信息
typedef struct MQTTPacketInfo
{
    uint8_t type;//MQTT数据包的类型
    uint8_t * pRemainingData;//MQTT数据包中剩余的序列化数据
    size_t remainingLength;//MQTT数据包中剩余的序列化数据长度
} MQTTPacketInfo_t;

//获取MQTT CONNECT数据包的大小和剩余长度
MQTTStatus_t MQTT_GetConnectPacketSize( const MQTTConnectInfo_t * pConnectInfo,
                                        const MQTTPublishInfo_t * pWillInfo,
                                        size_t * pRemainingLength,
                                        size_t * pPacketSize );

//在给定的固定缓冲区中序列化MQTT CONNECT数据包
MQTTStatus_t MQTT_SerializeConnect( const MQTTConnectInfo_t * pConnectInfo,
                                    const MQTTPublishInfo_t * pWillInfo,
                                    size_t remainingLength,
                                    const MQTTFixedBuffer_t * pFixedBuffer );

//获取MQTT SUBSCRIBE数据包的数据包大小和剩余长度
MQTTStatus_t MQTT_GetSubscribePacketSize( const MQTTSubscribeInfo_t * pSubscriptionList,
                                          size_t subscriptionCount,
                                          size_t * pRemainingLength,
                                          size_t * pPacketSize );

//在给定的缓冲区中序列化MQTT SUBSCRIBE数据包
MQTTStatus_t MQTT_SerializeSubscribe( const MQTTSubscribeInfo_t * pSubscriptionList,
                                      size_t subscriptionCount,
                                      uint16_t packetId,
                                      size_t remainingLength,
                                      const MQTTFixedBuffer_t * pFixedBuffer );

//获取MQTT UNSUBSCRIBE数据包的数据包大小和剩余长度
MQTTStatus_t MQTT_GetUnsubscribePacketSize( const MQTTSubscribeInfo_t * pSubscriptionList,
                                            size_t subscriptionCount,
                                            size_t * pRemainingLength,
                                            size_t * pPacketSize );

//在给定的缓冲区中序列化MQTT UNSUBSCRIBE数据包
MQTTStatus_t MQTT_SerializeUnsubscribe( const MQTTSubscribeInfo_t * pSubscriptionList,
                                        size_t subscriptionCount,
                                        uint16_t packetId,
                                        size_t remainingLength,
                                        const MQTTFixedBuffer_t * pFixedBuffer );

//获取MQTT PUBLISH数据包的数据包大小和剩余长度
MQTTStatus_t MQTT_GetPublishPacketSize( const MQTTPublishInfo_t * pPublishInfo,
                                        size_t * pRemainingLength,
                                        size_t * pPacketSize );

//在给定的缓冲区中序列化MQTT PUBLISH数据包
MQTTStatus_t MQTT_SerializePublish( const MQTTPublishInfo_t * pPublishInfo,
                                    uint16_t packetId,
                                    size_t remainingLength,
                                    const MQTTFixedBuffer_t * pFixedBuffer );

//在给定的缓冲区中序列化MQTT PUBLISH数据包标头
MQTTStatus_t MQTT_SerializePublishHeader( const MQTTPublishInfo_t * pPublishInfo,
                                          uint16_t packetId,
                                          size_t remainingLength,
                                          const MQTTFixedBuffer_t * pFixedBuffer,
                                          size_t * pHeaderSize );

//将MQTT PUBACK、PUBREC、PUBREL或PUBCOMP序列化为给定的
MQTTStatus_t MQTT_SerializeAck( const MQTTFixedBuffer_t * pFixedBuffer,
                                uint8_t packetType,
                                uint16_t packetId );

//获取MQTT DISCONNECT数据包的大小
MQTTStatus_t MQTT_GetDisconnectPacketSize( size_t * pPacketSize );

//将MQTT DISCONNECT数据包序列化到给定的缓冲区中
MQTTStatus_t MQTT_SerializeDisconnect( const MQTTFixedBuffer_t * pFixedBuffer );

//获取MQTT PINGREQ数据包的大小
MQTTStatus_t MQTT_GetPingreqPacketSize( size_t * pPacketSize );

//将MQTT PINGREQ数据包序列化到给定的缓冲区中
MQTTStatus_t MQTT_SerializePingreq( const MQTTFixedBuffer_t * pFixedBuffer );

//反序列化MQTT PUBLISH数据包
MQTTStatus_t MQTT_DeserializePublish( const MQTTPacketInfo_t * pIncomingPacket,
                                      uint16_t * pPacketId,
                                      MQTTPublishInfo_t * pPublishInfo );

//反序列化MQTT CONNACK、SUBACK、UNSUBACK、PUBACK、PUBREC、PUBREL、PUBCOMP或PINGRESP
MQTTStatus_t MQTT_DeserializeAck( const MQTTPacketInfo_t * pIncomingPacket,
                                  uint16_t * pPacketId,
                                  bool * pSessionPresent );

//从传入数据包中提取MQTT数据包类型和长度
MQTTStatus_t MQTT_GetIncomingPacketTypeAndLength( Transport_Recv_f readFunc,
                                                  Network_Context_t * pNetworkContext,
                                                  MQTTPacketInfo_t * pIncomingPacket );

