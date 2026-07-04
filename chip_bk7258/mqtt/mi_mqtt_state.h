//mi_mqtt_state.h
#pragma once


#include "mi_mqtt.h"

/*
发送方流程（Publisher）
    MQTTPublishSend → 发送PUBLISH消息（QoS 1/2）
    MQTTPubAckPending → 等待PUBACK（QoS 1）
        收到PUBACK → MQTTStateNull

    MQTTPubRecPending → 等待PUBREC（QoS 2）
        收到PUBREC → MQTTPubRelSend

    MQTTPubCompPending → 发送PUBREL后等待PUBCOMP（QoS 2）
        收到PUBCOMP → MQTTStateNull

接收方流程（Subscriber）
    收到PUBLISH → MQTTPubAckSend（QoS 1）
        发送PUBACK → MQTTStateNull
    收到PUBLISH → MQTTPubRecSend（QoS 2）
        发送PUBREC → MQTTPubRelPending
    MQTTPubRelPending → 等待PUBREL
        收到PUBREL → MQTTPubCompSend
        发送PUBCOMP → MQTTStateNull

*/


//MQTTStateCursor_t的初始化器值，表示搜索应该从状态记录数组的开头开始
#define MQTT_STATE_CURSOR_INITIALIZER    ( ( size_t ) 0 )

//用于遍历状态记录的游标
typedef size_t MQTTStateCursor_t;

//操作状态，表示发送或接收的值
typedef enum MQTTStateOperation
{
    MQTT_SEND,
    MQTT_RECEIVE
} MQTTStateOperation_t;


//为即将发布的QoS 1或QoS 2保留一个条目。
MQTTStatus_t MQTT_ReserveState( MQTTContext_t * pMqttContext,
                                uint16_t packetId,
                                MQTTQoS_t qos );


//根据发布的qos和操作类型计算发布的新状态
MQTTPublishState_t MQTT_CalculateStatePublish( MQTTStateOperation_t opType,
                                               MQTTQoS_t qos );

//更新PUBLISH数据包的状态记录
MQTTStatus_t MQTT_UpdateStatePublish( MQTTContext_t * pMqttContext,
                                      uint16_t packetId,
                                      MQTTStateOperation_t opType,
                                      MQTTQoS_t qos,
                                      MQTTPublishState_t * pNewState );


//根据PUBACK、PUBREC、PUBREL或PUBCOMP计算状态
MQTTPublishState_t MQTT_CalculateStateAck( MQTTPubAckType_t packetType,
                                           MQTTStateOperation_t opType,
                                           MQTTQoS_t qos );


//更新已确认发布的状态记录
MQTTStatus_t MQTT_UpdateStateAck( MQTTContext_t * pMqttContext,
                                  uint16_t packetId,
                                  MQTTPubAckType_t packetType,
                                  MQTTStateOperation_t opType,
                                  MQTTPublishState_t * pNewState );

//获取要重新发送的下一个待处理PUBREL ack的数据包ID
uint16_t MQTT_PubrelToResend( const MQTTContext_t * pMqttContext,
                              MQTTStateCursor_t * pCursor,
                              MQTTPublishState_t * pState );

//获取下一个待重新发送的待定发布的数据包ID
uint16_t MQTT_PublishToResend( const MQTTContext_t * pMqttContext,
                               MQTTStateCursor_t * pCursor );

//状态转字符串
const char * MQTT_State_strerror( MQTTPublishState_t state );


