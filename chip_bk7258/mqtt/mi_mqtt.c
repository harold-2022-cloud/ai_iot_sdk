//mi_mqtt.c
#include "mi_mqtt.h"

#include "mi_mqtt_state.h"

#include "mi_mqtt_port_bk.h"  // BK7258 platform seam: replaces rino_hal.h
#include "entity_log.h"

#include <string.h>
#include <assert.h>


/**
*@名称 		matchWildcards
*@功能 	    尝试将主题名称与以通配符开头的主题过滤器匹配
*@参数 		const char * pTopicName
*@参数 		uint16_t topicNameLength
*@参数 		const char * pTopicFilter
*@参数 		uint16_t topicFilterLength
*@参数 		uint16_t * pNameIndex
*@参数 		uint16_t * pFilterIndex
*@参数 		bool * pMatch
*@返回值 	int32_t
*@使用说明	
*/
static bool matchWildcards( const char * pTopicName,
                            uint16_t topicNameLength,
                            const char * pTopicFilter,
                            uint16_t topicFilterLength,
                            uint16_t * pNameIndex,
                            uint16_t * pFilterIndex,
                            bool * pMatch )
{
    bool shouldStopMatching = false;
    bool locationIsValidForWildcard;

    assert( pTopicName != NULL );
    assert( topicNameLength != 0 );
    assert( pTopicFilter != NULL );
    assert( topicFilterLength != 0 );
    assert( pNameIndex != NULL );
    assert( pFilterIndex != NULL );
    assert( pMatch != NULL );

    /* Wild card in a topic filter is only valid either at the starting position
     * or when it is preceded by a '/'.*/
    locationIsValidForWildcard = ( ( *pFilterIndex == 0u ) ||
                                   ( pTopicFilter[ *pFilterIndex - 1U ] == '/' )
                                   ) ? true : false;

    if( ( pTopicFilter[ *pFilterIndex ] == '+' ) && ( locationIsValidForWildcard == true ) )
    {
        bool nextLevelExistsInTopicName = false;
        bool nextLevelExistsinTopicFilter = false;

        /* Move topic name index to the end of the current level. The end of the
         * current level is identified by the last character before the next level
         * separator '/'. */
        while( *pNameIndex < topicNameLength )
        {
            /* Exit the loop if we hit the level separator. */
            if( pTopicName[ *pNameIndex ] == '/' )
            {
                nextLevelExistsInTopicName = true;
                break;
            }

            ( *pNameIndex )++;
        }

        /* Determine if the topic filter contains a child level after the current level
         * represented by the '+' wildcard. */
        if( ( *pFilterIndex < ( topicFilterLength - 1U ) ) &&
            ( pTopicFilter[ *pFilterIndex + 1U ] == '/' ) )
        {
            nextLevelExistsinTopicFilter = true;
        }

        /* If the topic name contains a child level but the topic filter ends at
         * the current level, then there does not exist a match. */
        if( ( nextLevelExistsInTopicName == true ) &&
            ( nextLevelExistsinTopicFilter == false ) )
        {
            *pMatch = false;
            shouldStopMatching = true;
        }

        /* If the topic name and topic filter have child levels, then advance the
         * filter index to the level separator in the topic filter, so that match
         * can be performed in the next level.
         * Note: The name index already points to the level separator in the topic
         * name. */
        else if( nextLevelExistsInTopicName == true )
        {
            ( *pFilterIndex )++;
        }
        else
        {
            /* If we have reached here, the the loop terminated on the
             * ( *pNameIndex < topicNameLength) condition, which means that have
             * reached past the end of the topic name, and thus, we decrement the
             * index to the last character in the topic name.*/
            ( *pNameIndex )--;
        }
    }

    /* '#' matches everything remaining in the topic name. It must be the
     * last character in a topic filter. */
    else if( ( pTopicFilter[ *pFilterIndex ] == '#' ) &&
             ( *pFilterIndex == ( topicFilterLength - 1U ) ) &&
             ( locationIsValidForWildcard == true ) )
    {
        /* Subsequent characters don't need to be checked for the
         * multi-level wildcard. */
        *pMatch = true;
        shouldStopMatching = true;
    }
    else
    {
        /* Any character mismatch other than '+' or '#' means the topic
         * name does not match the topic filter. */
        *pMatch = false;
        shouldStopMatching = true;
    }

    return shouldStopMatching;
}

/**
*@名称 		sendPacket
*@功能 	    使用传输发送口发送数据包到网络
*@参数 		MQTTContext_t * pContext
*@参数 		const uint8_t * pBufferToSend
*@参数 		size_t bytesToSend
*@返回值 	int32_t
*@使用说明	
*/
static int32_t sendPacket( MQTTContext_t * pContext,
                           const uint8_t * pBufferToSend,
                           size_t bytesToSend )
{
    const uint8_t * pIndex = pBufferToSend;
    size_t bytesRemaining = bytesToSend;
    int32_t totalBytesSent = 0, bytesSent;
    uint32_t sendTime = 0U;
    bool sendError = false;

    assert( pContext != NULL );
    assert( pContext->getTime != NULL );
    assert( pContext->transportInterface.Send != NULL );
    assert( pIndex != NULL );

    bytesRemaining = bytesToSend;

    /* Record the time of transmission. */
    sendTime = pContext->getTime();
    uint32_t trace_start_ms = sendTime;

    /* Loop until the entire packet is sent. */
    while( ( bytesRemaining > 0UL ) && ( sendError == false ) )
    {
        bytesSent = pContext->transportInterface.Send( pContext->transportInterface.Network_Context,
                                                       pIndex,
                                                       bytesRemaining );

        if( bytesSent < 0 )
        {
            LogError( ( "Transport send failed. Error code=%d.", bytesSent ) );
            totalBytesSent = bytesSent;
            sendError = true;
        }
        else
        {
            /* It is a bug in the application's transport send implementation if
             * more bytes than expected are sent. To avoid a possible overflow
             * in converting bytesRemaining from unsigned to signed, this assert
             * must exist after the check for bytesSent being negative. */
            assert( ( size_t ) bytesSent <= bytesRemaining );

            bytesRemaining -= ( size_t ) bytesSent;
            totalBytesSent += bytesSent;
            pIndex += bytesSent;
            LogDebug( ( "BytesSent=%d, BytesRemaining=%lu,"
                        " TotalBytesSent=%d.",
                        bytesSent,
                        ( unsigned long ) bytesRemaining,
                        totalBytesSent ) );
        }
    }

    /* Update time of last transmission if the entire packet is successfully sent. */
    if( totalBytesSent > 0 )
    {
        // pContext->lastPacketTime = sendTime;
        LogDebug( ( "Successfully sent packet at time %u.",
                    sendTime ) );
    }
    {
        uint32_t trace_end_ms = pContext->getTime();
        if ((bytesToSend >= 64U) || (trace_end_ms - trace_start_ms >= 20U) || (totalBytesSent < 0))
        {
            ENTITY_LOGI("[MQTT_TRACE][T8_TRANSPORT_SEND] begin_ms=%u end_ms=%u cost_ms=%u requested=%u sent=%d\r\n",
                        (unsigned int)trace_start_ms,
                        (unsigned int)trace_end_ms,
                        (unsigned int)(trace_end_ms - trace_start_ms),
                        (unsigned int)bytesToSend,
                        (int)totalBytesSent);
        }
    }
    sendTime = sendTime;
    return totalBytesSent;
}

/**
*@名称 		calculateElapsedTime
*@功能 	    计算2个毫秒时间戳的差值
*@参数 		uint32_t later
*@参数 		uint32_t start
*@返回值 	uint32_t
*@使用说明	
*/
static uint32_t calculateElapsedTime( uint32_t later, uint32_t start )
{
    return later - start;
}

/**
*@名称 		getAckFromPacketType
*@功能 	    包类型转换
*@参数 		uint8_t packetType
*@返回值 	MQTTPubAckType_t
*@使用说明	
*/
static MQTTPubAckType_t getAckFromPacketType( uint8_t packetType )
{
    MQTTPubAckType_t ackType = MQTTPuback;

    switch( packetType )
    {
        case MQTT_PACKET_TYPE_PUBACK:
            ackType = MQTTPuback;
            break;

        case MQTT_PACKET_TYPE_PUBREC:
            ackType = MQTTPubrec;
            break;

        case MQTT_PACKET_TYPE_PUBREL:
            ackType = MQTTPubrel;
            break;

        case MQTT_PACKET_TYPE_PUBCOMP:
        default:

            /* This function is only called after checking the type is one of
             * the above four values, so packet type must be PUBCOMP here. */
            assert( packetType == MQTT_PACKET_TYPE_PUBCOMP );
            ackType = MQTTPubcomp;
            break;
    }

    return ackType;
}

/**
*@名称 		recvExact
*@功能 	    带超时的接收
*@参数 		const MQTTContext_t * pContext
*@参数 		size_t bytesToRecv
*@参数 		uint32_t timeoutMs
*@返回值 	int32_t
*@使用说明	
*/
static int32_t recvExact( const MQTTContext_t * pContext,
                          size_t bytesToRecv,
                          uint32_t timeoutMs )
{
    uint8_t * pIndex = NULL;
    size_t bytesRemaining = bytesToRecv;
    int32_t totalBytesRecvd = 0, bytesRecvd;
    uint32_t entryTimeMs = 0U, elapsedTimeMs = 0U;
    Transport_Recv_f recvFunc = NULL;
    MQTTGetCurrentTimeFunc_t getTimeStampMs = NULL;
    bool receiveError = false;

    assert( pContext != NULL );
    assert( bytesToRecv <= pContext->networkBuffer.size );
    assert( pContext->getTime != NULL );
    assert( pContext->transportInterface.Recv != NULL );
    assert( pContext->networkBuffer.pBuffer != NULL );

    pIndex = pContext->networkBuffer.pBuffer;
    recvFunc = pContext->transportInterface.Recv;
    getTimeStampMs = pContext->getTime;

    entryTimeMs = getTimeStampMs();

    while( ( bytesRemaining > 0U ) && ( receiveError == false ) )
    {
        bytesRecvd = recvFunc( pContext->transportInterface.Network_Context,
                               pIndex,
                               bytesRemaining );

        if( bytesRecvd < 0 )
        {
            LogError( ( "Network error while receiving packet: ReturnCode=%d.",
                        bytesRecvd ) );
            
            totalBytesRecvd = bytesRecvd;
            receiveError = true;
        }
        else
        {
            /* It is a bug in the application's transport receive implementation
             * if more bytes than expected are received. To avoid a possible
             * overflow in converting bytesRemaining from unsigned to signed,
             * this assert must exist after the check for bytesRecvd being
             * negative. */
            assert( ( size_t ) bytesRecvd <= bytesRemaining );

            bytesRemaining -= ( size_t ) bytesRecvd;
            totalBytesRecvd += ( int32_t ) bytesRecvd;
            pIndex += bytesRecvd;
            LogDebug( ( "BytesReceived=%d, BytesRemaining=%lu, "
                        "TotalBytesReceived=%d.",
                        bytesRecvd,
                        ( unsigned long ) bytesRemaining,
                        totalBytesRecvd ) );
        }

        elapsedTimeMs = calculateElapsedTime( getTimeStampMs(), entryTimeMs );

        if( ( bytesRemaining > 0U ) && ( elapsedTimeMs >= timeoutMs ) )
        {
            LogError( ( "Time expired while receiving packet." ) );
            receiveError = true;
        }
    }

    return totalBytesRecvd;
}

/**
*@名称 		discardPacket
*@功能 	    丢弃数据包
*@参数 		const MQTTContext_t * pContext
*@参数 		size_t remainingLength
*@参数 		uint32_t timeoutMs
*@返回值 	MQTTStatus_t
*@使用说明	
*/
static MQTTStatus_t discardPacket( const MQTTContext_t * pContext,
                                   size_t remainingLength,
                                   uint32_t timeoutMs )
{
    MQTTStatus_t status = MQTTRecvFailed;
    int32_t bytesReceived = 0;
    size_t bytesToReceive = 0U;
    uint32_t totalBytesReceived = 0U, entryTimeMs = 0U, elapsedTimeMs = 0U;
    uint32_t remainingTimeMs = timeoutMs;
    MQTTGetCurrentTimeFunc_t getTimeStampMs = NULL;
    bool receiveError = false;

    assert( pContext != NULL );
    assert( pContext->getTime != NULL );

    bytesToReceive = pContext->networkBuffer.size;
    getTimeStampMs = pContext->getTime;

    entryTimeMs = getTimeStampMs();

    while( ( totalBytesReceived < remainingLength ) && ( receiveError == false ) )
    {
        if( ( remainingLength - totalBytesReceived ) < bytesToReceive )
        {
            bytesToReceive = remainingLength - totalBytesReceived;
        }

        bytesReceived = recvExact( pContext, bytesToReceive, remainingTimeMs );

        if( bytesReceived != ( int32_t ) bytesToReceive )
        {
            LogError( ( "Receive error while discarding packet."
                        "ReceivedBytes=%d, ExpectedBytes=%lu.",
                        bytesReceived,
                        ( unsigned long ) bytesToReceive ) );
            
            receiveError = true;
        }
        else
        {
            totalBytesReceived += ( uint32_t ) bytesReceived;

            elapsedTimeMs = calculateElapsedTime( getTimeStampMs(), entryTimeMs );

            /* Update remaining time and check for timeout. */
            if( elapsedTimeMs < timeoutMs )
            {
                remainingTimeMs = timeoutMs - elapsedTimeMs;
            }
            else
            {
                LogError( ( "Time expired while discarding packet." ) );
                receiveError = true;
            }
        }
    }

    if( totalBytesReceived == remainingLength )
    {
        LogError( ( "Dumped packet. DumpedBytes=%d.",
                    totalBytesReceived ) );
        /* Packet dumped, so no data is available. */
        status = MQTTNoDataAvailable;
    }

    return status;
}

/**
*@名称 		receivePacket
*@功能 	    接收数据包
*@参数 		const MQTTContext_t * pContext
*@参数 		MQTTPacketInfo_t incomingPacket
*@参数 		uint32_t remainingTimeMs
*@返回值 	MQTTStatus_t
*@使用说明	
*/
static MQTTStatus_t receivePacket( const MQTTContext_t * pContext,
                                   MQTTPacketInfo_t incomingPacket,
                                   uint32_t remainingTimeMs )
{
    MQTTStatus_t status = MQTTSuccess;
    int32_t bytesReceived = 0;
    size_t bytesToReceive = 0U;

    assert( pContext != NULL );
    assert( pContext->networkBuffer.pBuffer != NULL );

    if( incomingPacket.remainingLength > pContext->networkBuffer.size )//数据过长，丢弃
    {
        LogError( ( "Incoming packet will be dumped: "
                    "Packet length exceeds network buffer size."
                    "PacketSize=%lu, NetworkBufferSize=%lu.",
                    ( unsigned long ) incomingPacket.remainingLength,
                    ( unsigned long ) pContext->networkBuffer.size ) );
        status = discardPacket( pContext,
                                incomingPacket.remainingLength,
                                remainingTimeMs );
    }
    else
    {
        bytesToReceive = incomingPacket.remainingLength;
        bytesReceived = recvExact( pContext, bytesToReceive, remainingTimeMs );

        if( bytesReceived == ( int32_t ) bytesToReceive )
        {
            /* Receive successful, bytesReceived == bytesToReceive. */
            LogInfo( ( "Packet received. ReceivedBytes=%d.",
                       bytesReceived ) );
        }
        else
        {
            LogError( ( "Packet reception failed. ReceivedBytes=%d, "
                        "ExpectedBytes=%lu.",
                        bytesReceived,
                        ( unsigned long ) bytesToReceive ) );
            status = MQTTRecvFailed;
        }
    }

    return status;
}

/**
*@名称 		getAckTypeToSend
*@功能 	    根据发布类型获取要发送的应答包类型
*@参数 		MQTTPublishState_t state 发布类型
*@返回值 	uint8_t 包类型
*@使用说明	
*/
static uint8_t getAckTypeToSend( MQTTPublishState_t state )
{
    uint8_t packetTypeByte = 0U;

    switch( state )
    {
        case MQTTPubAckSend:
            packetTypeByte = MQTT_PACKET_TYPE_PUBACK;
            break;

        case MQTTPubRecSend:
            packetTypeByte = MQTT_PACKET_TYPE_PUBREC;
            break;

        case MQTTPubRelSend:
            packetTypeByte = MQTT_PACKET_TYPE_PUBREL;
            break;

        case MQTTPubCompSend:
            packetTypeByte = MQTT_PACKET_TYPE_PUBCOMP;
            break;

        default:
            /* Take no action for states that do not require sending an ack. */
            break;
    }

    return packetTypeByte;
}

/**
*@名称 		sendPublishAcks
*@功能 	    发送PUBLISH应答
*@参数 		MQTTContext_t * pContext
*@参数 		uint16_t packetId
*@参数 		MQTTPublishState_t publishState
*@返回值 	MQTTStatus_t 
*@使用说明	
*/
static MQTTStatus_t sendPublishAcks( MQTTContext_t * pContext,
                                     uint16_t packetId,
                                     MQTTPublishState_t publishState )
{
    hal_mutex_lock(&pContext->mutex, 0xFFFFFFFF);

    MQTTStatus_t status = MQTTSuccess;
    MQTTPublishState_t newState = MQTTStateNull;
    int32_t bytesSent = 0;
    uint8_t packetTypeByte = 0U;
    MQTTPubAckType_t packetType;

    assert( pContext != NULL );

    packetTypeByte = getAckTypeToSend( publishState );

    if( packetTypeByte != 0U )
    {
        packetType = getAckFromPacketType( packetTypeByte );

        status = MQTT_SerializeAck( &( pContext->networkBuffer ),
                                    packetTypeByte,
                                    packetId );

        if( status == MQTTSuccess )
        {
            bytesSent = sendPacket( pContext,
                                    pContext->networkBuffer.pBuffer,
                                    MQTT_PUBLISH_ACK_PACKET_SIZE );
        }

        if( bytesSent == ( int32_t ) MQTT_PUBLISH_ACK_PACKET_SIZE )
        {
            pContext->controlPacketSent = true;
            status = MQTT_UpdateStateAck( pContext,
                                          packetId,
                                          packetType,
                                          MQTT_SEND,
                                          &newState );

            if( status != MQTTSuccess )
            {
                LogError( ( "Failed to update state of publish %u.", packetId ) );
            }
        }
        else
        {
            LogError( ( "Failed to send ACK packet: PacketType=%02x, "
                        "SentBytes=%d, "
                        "PacketSize=%lu.",
                        packetTypeByte,
                        bytesSent,
                        MQTT_PUBLISH_ACK_PACKET_SIZE ) );
            status = MQTTSendFailed;
        }
    }
    hal_mutex_unlock(&pContext->mutex);
    return status;
}

/**
*@名称 		handleKeepAlive
*@功能 	    发送保活PING
*@参数 		MQTTContext_t * pContext
*@返回值 	MQTTStatus_t 
*@使用说明	
*/
static MQTTStatus_t handleKeepAlive( MQTTContext_t * pContext )
{
    MQTTStatus_t status = MQTTSuccess;
    uint32_t now = 0U, keepAliveMs = 0U;

    assert( pContext != NULL );
    assert( pContext->getTime != NULL );

    now = pContext->getTime();
    keepAliveMs = 1000U * ( uint32_t ) pContext->keepAliveIntervalSec;

    if( keepAliveMs == 0U )
    {
        return status;
    }

    if( pContext->waitingForPingResp == true )
    {
        if( calculateElapsedTime( now, pContext->pingReqSendTimeMs ) >
            MQTT_PINGRESP_TIMEOUT_MS )
        {
            ENTITY_LOGW("[MQTT_KEEPALIVE][PINGRESP_TIMEOUT] age_ms=%u timeout_ms=%u "
                        "last_packet=%u ping_sent=%u\r\n",
                        (unsigned int)(now - pContext->pingReqSendTimeMs),
                        (unsigned int)MQTT_PINGRESP_TIMEOUT_MS,
                        (unsigned int)pContext->lastPacketTime,
                        (unsigned int)pContext->pingReqSendTimeMs);
            status = MQTTKeepAliveTimeout;
        }
        return status;
    }

    if( calculateElapsedTime( now, pContext->lastPacketTime ) > keepAliveMs )
    {
        pContext->lastPacketTime = now;
        status = MQTT_Ping( pContext );
    }

    return status;
}

/**
*@名称 		handleIncomingPublish
*@功能 	    处理收到的MQTT PUBLISH数据包
*@参数 		MQTTContext_t * pContext
*@参数 		MQTTPacketInfo_t * pIncomingPacket
*@返回值 	MQTTStatus_t 
*@使用说明	
*/
static MQTTStatus_t handleIncomingPublish( MQTTContext_t * pContext,
                                           MQTTPacketInfo_t * pIncomingPacket )
{
    MQTTStatus_t status = MQTTBadParameter;
    MQTTPublishState_t publishRecordState = MQTTStateNull;
    uint16_t packetIdentifier = 0U;
    MQTTPublishInfo_t publishInfo;
    MQTTDeserializedInfo_t deserializedInfo;
    bool duplicatePublish = false;

    assert( pContext != NULL );
    assert( pIncomingPacket != NULL );
    assert( pContext->appCallback != NULL );

    status = MQTT_DeserializePublish( pIncomingPacket, &packetIdentifier, &publishInfo );
    LogInfo( ( "De-serialized incoming PUBLISH packet: DeserializerResult=%s.",
               MQTT_Status_strerror( status ) ) );

    if( status == MQTTSuccess )
    {
        status = MQTT_UpdateStatePublish( pContext,
                                          packetIdentifier,
                                          MQTT_RECEIVE,
                                          publishInfo.qos,
                                          &publishRecordState );

        if( status == MQTTSuccess )
        {
            LogInfo( ( "State record updated. New state=%s.",
                       MQTT_State_strerror( publishRecordState ) ) );
        }

        /* Different cases in which an incoming publish with duplicate flag is
         * handled are as listed below.
         * 1. No collision - This is the first instance of the incoming publish
         *    packet received or an earlier received packet state is lost. This
         *    will be handled as a new incoming publish for both QoS1 and QoS2
         *    publishes.
         * 2. Collision - The incoming packet was received before and a state
         *    record is present in the state engine. For QoS1 and QoS2 publishes
         *    this case can happen at 2 different cases and handling is
         *    different.
         *    a. QoS1 - If a PUBACK is not successfully sent for the incoming
         *       publish due to a connection issue, it can result in broker
         *       sending out a duplicate publish with dup flag set, when a
         *       session is reestablished. It can result in a collision in
         *       state engine. This will be handled by processing the incoming
         *       publish as a new publish ignoring the
         *       #MQTTStateCollision status from the state engine. The publish
         *       data is not passed to the application.
         *    b. QoS2 - If a PUBREC is not successfully sent for the incoming
         *       publish or the PUBREC sent is not successfully received by the
         *       broker due to a connection issue, it can result in broker
         *       sending out a duplicate publish with dup flag set, when a
         *       session is reestablished. It can result in a collision in
         *       state engine. This will be handled by ignoring the
         *       #MQTTStateCollision status from the state engine. The publish
         *       data is not passed to the application. */
        else if( status == MQTTStateCollision )
        {
            status = MQTTSuccess;
            duplicatePublish = true;

            /* Calculate the state for the ack packet that needs to be sent out
             * for the duplicate incoming publish. */
            publishRecordState = MQTT_CalculateStatePublish( MQTT_RECEIVE,
                                                             publishInfo.qos );

            LogDebug( ( "Incoming publish packet with packet id %u already exists.",
                        packetIdentifier ) );

            if( publishInfo.dup == false )
            {
                LogError( ( "DUP flag is 0 for duplicate packet (MQTT-3.3.1.-1)." ) );
            }
        }
        else
        {
            LogError( ( "Error in updating publish state for incoming publish with packet id %u."
                        " Error is %s",
                        packetIdentifier,
                        MQTT_Status_strerror( status ) ) );
        }
    }

    if( status == MQTTSuccess )
    {
        /* Set fields of deserialized struct. */
        deserializedInfo.packetIdentifier = packetIdentifier;
        deserializedInfo.pPublishInfo = &publishInfo;
        deserializedInfo.deserializationResult = status;

        /* Invoke application callback to hand the buffer over to application
         * before sending acks.
         * Application callback will be invoked for all publishes, except for
         * duplicate incoming publishes. */
        if( duplicatePublish == false )
        {
            pContext->appCallback( pContext,
                                   pIncomingPacket,
                                   &deserializedInfo );
        }

        /* Send PUBACK or PUBREC if necessary. */
        status = sendPublishAcks( pContext,
                                  packetIdentifier,
                                  publishRecordState );
    }

    return status;
}

/**
*@名称 		handlePublishAcks
*@功能 	    处理收到的MQTT发布确认
*@参数 		MQTTContext_t * pContext
*@参数 		MQTTPacketInfo_t * pIncomingPacket
*@返回值 	MQTTStatus_t 
*@使用说明	
*/
static MQTTStatus_t handlePublishAcks( MQTTContext_t * pContext,
                                       MQTTPacketInfo_t * pIncomingPacket )
{
    MQTTStatus_t status = MQTTBadResponse;
    MQTTPublishState_t publishRecordState = MQTTStateNull;
    uint16_t packetIdentifier;
    MQTTPubAckType_t ackType;
    MQTTEventCallback_t appCallback;
    MQTTDeserializedInfo_t deserializedInfo;

    assert( pContext != NULL );
    assert( pIncomingPacket != NULL );
    assert( pContext->appCallback != NULL );

    appCallback = pContext->appCallback;

    ackType = getAckFromPacketType( pIncomingPacket->type );
    status = MQTT_DeserializeAck( pIncomingPacket, &packetIdentifier, NULL );
    LogInfo( ( "Ack packet deserialized with result: %s.",
               MQTT_Status_strerror( status ) ) );

    if( status == MQTTSuccess )
    {
        // rino_hal_mutex_lock(pContext->mutex);
        hal_mutex_lock(&pContext->mutex, 0xFFFFFFFF);

        status = MQTT_UpdateStateAck( pContext,
                                      packetIdentifier,
                                      ackType,
                                      MQTT_RECEIVE,
                                      &publishRecordState );
        // rino_hal_mutex_unlock(pContext->mutex);
        hal_mutex_unlock(&pContext->mutex);

        if( status == MQTTSuccess )
        {
            LogInfo( ( "State record updated. New state=%s.",
                       MQTT_State_strerror( publishRecordState ) ) );
        }
        else
        {
            LogError( ( "Updating the state engine for packet id %u"
                        " failed with error %s.",
                        packetIdentifier,
                        MQTT_Status_strerror( status ) ) );
        }
    }

    if( status == MQTTSuccess )
    {
        /* Set fields of deserialized struct. */
        deserializedInfo.packetIdentifier = packetIdentifier;
        deserializedInfo.deserializationResult = status;
        deserializedInfo.pPublishInfo = NULL;

        /* Invoke application callback to hand the buffer over to application
         * before sending acks. */
        appCallback( pContext, pIncomingPacket, &deserializedInfo );

        /* Send PUBREL or PUBCOMP if necessary. */
        status = sendPublishAcks( pContext,
                                  packetIdentifier,
                                  publishRecordState );
    }

    return status;
}

/**
*@名称 		handleIncomingAck
*@功能 	    处理收到的MQTT应答
*@参数 		MQTTContext_t * pContext
*@参数 		MQTTPacketInfo_t * pIncomingPacket
*@参数 		bool manageKeepAlive
*@返回值 	MQTTStatus_t 
*@使用说明	
*/
static MQTTStatus_t handleIncomingAck( MQTTContext_t * pContext,
                                       MQTTPacketInfo_t * pIncomingPacket,
                                       bool manageKeepAlive )
{
    MQTTStatus_t status = MQTTBadResponse;
    uint16_t packetIdentifier = MQTT_PACKET_ID_INVALID;
    MQTTDeserializedInfo_t deserializedInfo;

    /* We should always invoke the app callback unless we receive a PINGRESP
     * and are managing keep alive, or if we receive an unknown packet. We
     * initialize this to false since the callback must be invoked before
     * sending any PUBREL or PUBCOMP. However, for other cases, we invoke it
     * at the end to reduce the complexity of this function. */
    bool invokeAppCallback = false;
    MQTTEventCallback_t appCallback = NULL;

    assert( pContext != NULL );
    assert( pIncomingPacket != NULL );
    assert( pContext->appCallback != NULL );

    appCallback = pContext->appCallback;

    LogDebug( ( "Received packet of type %02x.", pIncomingPacket->type ) );

    switch( pIncomingPacket->type )
    {
        case MQTT_PACKET_TYPE_PUBACK:
        case MQTT_PACKET_TYPE_PUBREC:
        case MQTT_PACKET_TYPE_PUBREL:
        case MQTT_PACKET_TYPE_PUBCOMP:

            /* Handle all the publish acks. The app callback is invoked here. */
            status = handlePublishAcks( pContext, pIncomingPacket );

            break;

        case MQTT_PACKET_TYPE_PINGRESP:
            status = MQTT_DeserializeAck( pIncomingPacket, &packetIdentifier, NULL );
            invokeAppCallback = ( ( status == MQTTSuccess ) && ( manageKeepAlive == false ) ) ? true : false;

            if( ( status == MQTTSuccess ) && ( manageKeepAlive == true ) )
            {
                uint32_t now_ms = pContext->getTime();
                pContext->pingRespRecvTimeMs = now_ms;
                pContext->ping_rtt_ms = now_ms - pContext->pingReqSendTimeMs;
                ENTITY_LOGI("[MQTT_KEEPALIVE][PINGRESP_RX] rtt_ms=%u ping_sent=%u now=%u\r\n",
                            (unsigned int)pContext->ping_rtt_ms,
                            (unsigned int)pContext->pingReqSendTimeMs,
                            (unsigned int)now_ms);
                
                pContext->waitingForPingResp = false;
            }

            break;

        case MQTT_PACKET_TYPE_SUBACK:
        case MQTT_PACKET_TYPE_UNSUBACK:
            /* Deserialize and give these to the app provided callback. */
            status = MQTT_DeserializeAck( pIncomingPacket, &packetIdentifier, NULL );
            invokeAppCallback = ( ( status == MQTTSuccess ) || ( status == MQTTServerRefused ) ) ? true : false;
            break;

        default:
            /* Bad response from the server. */
            LogError( ( "Unexpected packet type from server: PacketType=%02x.",
                        pIncomingPacket->type ) );
            status = MQTTBadResponse;
            break;
    }

    if( invokeAppCallback == true )
    {
        /* Set fields of deserialized struct. */
        deserializedInfo.packetIdentifier = packetIdentifier;
        deserializedInfo.deserializationResult = status;
        deserializedInfo.pPublishInfo = NULL;
        appCallback( pContext, pIncomingPacket, &deserializedInfo );
        /* In case a SUBACK indicated refusal, reset the status to continue the loop. */
        status = MQTTSuccess;
    }

    return status;
}

/**
*@名称 		receiveSingleIteration
*@功能 	    单次接收
*@参数 		MQTTContext_t * pContext
*@参数 		uint32_t remainingTimeMs
*@参数 		bool manageKeepAlive
*@返回值 	MQTTStatus_t 
*@使用说明	
*/
static MQTTStatus_t receiveSingleIteration( MQTTContext_t * pContext,
                                            uint32_t remainingTimeMs,
                                            bool manageKeepAlive )
{
    MQTTStatus_t status = MQTTSuccess;
    MQTTPacketInfo_t incomingPacket;

    assert( pContext != NULL );
    assert( pContext->networkBuffer.pBuffer != NULL );

    //获取数据包长度
    status = MQTT_GetIncomingPacketTypeAndLength( pContext->transportInterface.Recv,
                                                  pContext->transportInterface.Network_Context,
                                                  &incomingPacket );

    if( status == MQTTNoDataAvailable )//无数据接收时发送保活心跳
    {
        if( manageKeepAlive == true )
        {
            /* Assign status so an error can be bubbled up to application,
             * but reset it on success. */
            status = handleKeepAlive( pContext );
        }

        if( status == MQTTSuccess )
        {
            /* Reset the status to indicate that we should not try to read
             * a packet from the transport interface. */
            status = MQTTNoDataAvailable;
        }
    }
    else if( status != MQTTSuccess )
    {
        LogError( ( "Receiving incoming packet length failed. Status=%s",
                    MQTT_Status_strerror( status ) ) );
    }
    else
    {
        /* Receive packet. Remaining time is recalculated before calling this
         * function. */
       // remainingTimeMs = 2000;      //2s timeout
       //由于 loop 循环里面没有处理获取到一个完整包数据之后更新超时时间，所以这里每次都需要强行设置接收一个完整包的剩余时间为指定的超时时间
        remainingTimeMs = pContext->timeout_ms; 
        status = receivePacket( pContext, incomingPacket, remainingTimeMs );//接收数据包
    }

    /* Handle received packet. If no data was read then this will not execute. */
    if( status == MQTTSuccess )
    {
        incomingPacket.pRemainingData = pContext->networkBuffer.pBuffer;

        /* PUBLISH packets allow flags in the lower four bits. For other
         * packet types, they are reserved. */
        if( ( incomingPacket.type & 0xF0U ) == MQTT_PACKET_TYPE_PUBLISH )
        {
            status = handleIncomingPublish( pContext, &incomingPacket );
        }
        else
        {
            status = handleIncomingAck( pContext, &incomingPacket, manageKeepAlive );
        }
    }

    if( status == MQTTNoDataAvailable )
    {
        /* No data available is not an error. Reset to MQTTSuccess so the
         * return code will indicate success. */
        status = MQTTSuccess;
    }

    return status;
}

/**
*@名称 		validateSubscribeUnsubscribeParams
*@功能 	    验证订阅参数
*@参数 		MQTTContext_t * pContext
*@参数 		const MQTTSubscribeInfo_t * pSubscriptionList
*@参数 		size_t subscriptionCount
*@参数 		uint16_t packetId
*@返回值 	MQTTStatus_t 
*@使用说明	
*/
static MQTTStatus_t validateSubscribeUnsubscribeParams( const MQTTContext_t * pContext,
                                                        const MQTTSubscribeInfo_t * pSubscriptionList,
                                                        size_t subscriptionCount,
                                                        uint16_t packetId )
{
    MQTTStatus_t status = MQTTSuccess;

    /* Validate all the parameters. */
    if( ( pContext == NULL ) || ( pSubscriptionList == NULL ) )
    {
        LogError( ( "Argument cannot be NULL: pContext=%p, "
                    "pSubscriptionList=%p.",
                    ( void * ) pContext,
                    ( void * ) pSubscriptionList ) );
        status = MQTTBadParameter;
    }
    else if( subscriptionCount == 0UL )
    {
        LogError( ( "Subscription count is 0." ) );
        status = MQTTBadParameter;
    }
    else if( packetId == 0U )
    {
        LogError( ( "Packet Id for subscription packet is 0." ) );
        status = MQTTBadParameter;
    }
    else
    {
        /* Empty else MISRA 15.7 */
    }

    return status;
}


/**
*@名称 		sendPublish
*@功能 	    发送序列化后的发布数据包
*@参数 		MQTTContext_t * pContext
*@参数 		const MQTTPublishInfo_t * pPublishInfo
*@参数 		size_t headerSize
*@返回值 	MQTTStatus_t 
*@使用说明	
*/
static MQTTStatus_t sendPublish( MQTTContext_t * pContext,
                                 const MQTTPublishInfo_t * pPublishInfo,
                                 size_t headerSize )
{
    MQTTStatus_t status = MQTTSuccess;
    int32_t bytesSent = 0;

    assert( pContext != NULL );
    assert( pPublishInfo != NULL );
    assert( headerSize > 0 );
    assert( pContext->networkBufferTX.pBuffer != NULL );
    assert( !( pPublishInfo->payloadLength > 0 ) || ( pPublishInfo->pPayload != NULL ) );

    //先发送数据头
    bytesSent = sendPacket( pContext,
                            pContext->networkBufferTX.pBuffer,
                            headerSize );

    if( bytesSent < 0 )
    {
        LogError( ( "Transport send failed for PUBLISH header." ) );
        status = MQTTSendFailed;
    }
    else
    {
        LogDebug( ( "Sent %d bytes of PUBLISH header.",
                    bytesSent ) );

        //发送负载数据 
        if( pPublishInfo->payloadLength > 0U )
        {
            bytesSent = sendPacket( pContext,
                                    pPublishInfo->pPayload,
                                    pPublishInfo->payloadLength );

            if( bytesSent < 0 )
            {
                LogError( ( "Transport send failed for PUBLISH payload." ) );
                status = MQTTSendFailed;
            }
            else
            {
                LogDebug( ( "Sent %d bytes of PUBLISH payload.",
                            bytesSent ) );
            }
        }
        else
        {
            LogDebug( ( "PUBLISH payload was not sent. Payload length was zero." ) );
        }
    }

    return status;
}

/**
*@名称 		receiveConnack
*@功能 	    接收一个连接应答数据包
*@参数 		MQTTContext_t * pContext
*@参数 		uint32_t timeoutMs
*@参数 		bool cleanSession
*@参数 		MQTTPacketInfo_t * pIncomingPacket
*@参数 		bool * pSessionPresent
*@返回值 	MQTTStatus_t 
*@使用说明	
*/
static MQTTStatus_t receiveConnack( const MQTTContext_t * pContext,
                                    uint32_t timeoutMs,
                                    bool cleanSession,
                                    MQTTPacketInfo_t * pIncomingPacket,
                                    bool * pSessionPresent )
{
    MQTTStatus_t status = MQTTSuccess;
    MQTTGetCurrentTimeFunc_t getTimeStamp = NULL;
    uint32_t entryTimeMs = 0U, remainingTimeMs = 0U, timeTakenMs = 0U;
    bool breakFromLoop = false;
    uint16_t loopCount = 0U;

    assert( pContext != NULL );
    assert( pIncomingPacket != NULL );
    assert( pContext->getTime != NULL );

    getTimeStamp = pContext->getTime;

    /* Get the entry time for the function. */
    entryTimeMs = getTimeStamp();

    do
    {
        /* Transport read for incoming CONNACK packet type and length.
         * MQTT_GetIncomingPacketTypeAndLength is a blocking call and it is
         * returned after a transport receive timeout, an error, or a successful
         * receive of packet type and length. */
        status = MQTT_GetIncomingPacketTypeAndLength( pContext->transportInterface.Recv,
                                                      pContext->transportInterface.Network_Context,
                                                      pIncomingPacket );

        /* The loop times out based on 2 conditions.
         * 1. If timeoutMs is greater than 0:
         *    Loop times out based on the timeout calculated by getTime()
         *    function.
         * 2. If timeoutMs is 0:
         *    Loop times out based on the maximum number of retries config
         *    MQTT_MAX_CONNACK_RECEIVE_RETRY_COUNT. This config will control
         *    maximum the number of retry attempts to read the CONNACK packet.
         *    A value of 0 for the config will try once to read CONNACK. */
        if( timeoutMs > 0U )
        {
            breakFromLoop = ( calculateElapsedTime( getTimeStamp(), entryTimeMs ) >= timeoutMs ) ? true : false;
        }
        else
        {
            breakFromLoop = ( loopCount >= MQTT_MAX_CONNACK_RECEIVE_RETRY_COUNT ) ? true : false;
            loopCount++;
        }

        /* Loop until there is data to read or if we have exceeded the timeout/retries. */
    } while( ( status == MQTTNoDataAvailable ) && ( breakFromLoop == false ) );

    if( status == MQTTSuccess )
    {
        /* Time taken in this function so far. */
        timeTakenMs = calculateElapsedTime( getTimeStamp(), entryTimeMs );

        if( timeTakenMs < timeoutMs )
        {
            /* Calculate remaining time for receiving the remainder of
             * the packet. */
            remainingTimeMs = timeoutMs - timeTakenMs;
        }

        /* Reading the remainder of the packet by transport recv.
         * Attempt to read once even if the timeout has expired.
         * Invoking receivePacket with remainingTime as 0 would attempt to
         * recv from network once. If using retries, the remainder of the
         * CONNACK packet is tried to be read only once. Reading once would be
         * good as the packet type and remaining length was already read. Hence,
         * the probability of the remaining 2 bytes available to read is very high. */
        if( pIncomingPacket->type == MQTT_PACKET_TYPE_CONNACK )
        {
            status = receivePacket( pContext,
                                    *pIncomingPacket,
                                    remainingTimeMs );
        }
        else
        {
            LogError( ( "Incorrect packet type %X received while expecting"
                        " CONNACK(%X).",
                        pIncomingPacket->type,
                        MQTT_PACKET_TYPE_CONNACK ) );
            status = MQTTBadResponse;
        }
    }

    if( status == MQTTSuccess )
    {
        /* Update the packet info pointer to the buffer read. */
        pIncomingPacket->pRemainingData = pContext->networkBuffer.pBuffer;

        /* Deserialize CONNACK. */
        status = MQTT_DeserializeAck( pIncomingPacket, NULL, pSessionPresent );
    }

    /* If a clean session is requested, a session present should not be set by
     * broker. */
    if( status == MQTTSuccess )
    {
        if( ( cleanSession == true ) && ( *pSessionPresent == true ) )
        {
            LogError( ( "Unexpected session present flag in CONNACK response from broker."
                        " CONNECT request with clean session was made with broker." ) );
            status = MQTTBadResponse;
        }
    }

    if( status == MQTTSuccess )
    {
        LogInfo( ( "Received MQTT CONNACK successfully from broker." ) );
    }
    else
    {
        LogError( ( "CONNACK recv failed with status = %s.",
                    MQTT_Status_strerror( status ) ) );
    }

    return status;
}

/**
*@名称 		handleSessionResumption
*@功能 	    为重新建立的会话发送应答或清除现有记录
*@参数 		MQTTContext_t * pContext
*@参数 		bool pSessionPresent
*@返回值 	MQTTStatus_t 
*@使用说明	
*/
static MQTTStatus_t handleSessionResumption( MQTTContext_t * pContext,
                                             bool sessionPresent )
{
    MQTTStatus_t status = MQTTSuccess;
    MQTTStateCursor_t cursor = MQTT_STATE_CURSOR_INITIALIZER;
    uint16_t packetId = MQTT_PACKET_ID_INVALID;
    MQTTPublishState_t state = MQTTStateNull;

    assert( pContext != NULL );

    if( sessionPresent == true )
    {
        /* Get the next packet ID for which a PUBREL need to be resent. */
        packetId = MQTT_PubrelToResend( pContext, &cursor, &state );

        /* Resend all the PUBREL acks after session is reestablished. */
        while( ( packetId != MQTT_PACKET_ID_INVALID ) &&
               ( status == MQTTSuccess ) )
        {
            status = sendPublishAcks( pContext, packetId, state );

            packetId = MQTT_PubrelToResend( pContext, &cursor, &state );
        }
    }
    else
    {
        /* Clear any existing records if a new session is established. */
        ( void ) memset( pContext->outgoingPublishRecords,
                         0x00,
                         sizeof( pContext->outgoingPublishRecords ) );
        ( void ) memset( pContext->incomingPublishRecords,
                         0x00,
                         sizeof( pContext->incomingPublishRecords ) );
    }

    return status;
}

/**
*@名称 		serializePublish
*@功能 	    序列化一个发布消息
*@参数 		MQTTContext_t * pContext
*@参数 		const MQTTPublishInfo_t * pPublishInfo
*@参数 		uint16_t packetId
*@参数 		size_t * const pHeaderSize
*@返回值 	MQTTStatus_t 
*@使用说明	只是序列化头部数据，负载在sendPublish中直接发送
*/
static MQTTStatus_t serializePublish( const MQTTContext_t * pContext,
                                      const MQTTPublishInfo_t * pPublishInfo,
                                      uint16_t packetId,
                                      size_t * const pHeaderSize )
{
    MQTTStatus_t status = MQTTSuccess;
    size_t remainingLength = 0UL, packetSize = 0UL;

    assert( pContext != NULL );
    assert( pPublishInfo != NULL );
    assert( pHeaderSize != NULL );

    //获取MQTT PUBLISH数据包的数据包大小和剩余长度
    status = MQTT_GetPublishPacketSize( pPublishInfo,
                                        &remainingLength,
                                        &packetSize );
    LogDebug( ( "PUBLISH packet size is %lu and remaining length is %lu.",
                ( unsigned long ) packetSize,
                ( unsigned long ) remainingLength ) );

    if( status == MQTTSuccess )
    {
        //序列化PUBLISH数据包头，负载在sendPublish中直接发送
        status = MQTT_SerializePublishHeader( pPublishInfo,
                                              packetId,
                                              remainingLength,
                                              &( pContext->networkBufferTX ),
                                              pHeaderSize );
        LogDebug( ( "Serialized PUBLISH header size is %lu.",
                    ( unsigned long ) *pHeaderSize ) );
    }

    return status;
}

/**
*@名称 		validatePublishParams
*@功能 	    发布参数验证
*@参数 		MQTTContext_t * pContext
*@参数 		const MQTTPublishInfo_t * pPublishInfo
*@参数 		uint16_t packetId
*@返回值 	MQTTStatus_t 
*@使用说明	
*/
static MQTTStatus_t validatePublishParams( const MQTTContext_t * pContext,
                                           const MQTTPublishInfo_t * pPublishInfo,
                                           uint16_t packetId )
{
    MQTTStatus_t status = MQTTSuccess;

    /* Validate arguments. */
    if( ( pContext == NULL ) || ( pPublishInfo == NULL ) )
    {
        LogError( ( "Argument cannot be NULL: pContext=%p, "
                    "pPublishInfo=%p.",
                    ( void * ) pContext,
                    ( void * ) pPublishInfo ) );
        status = MQTTBadParameter;
    }
    else if( ( pPublishInfo->qos != MQTTQoS0 ) && ( packetId == 0U ) )
    {
        LogError( ( "Packet Id is 0 for PUBLISH with QoS=%u.",
                    pPublishInfo->qos ) );
        status = MQTTBadParameter;
    }
    else if( ( pPublishInfo->payloadLength > 0U ) && ( pPublishInfo->pPayload == NULL ) )
    {
        LogError( ( "A nonzero payload length requires a non-NULL payload: "
                    "payloadLength=%lu, pPayload=%p.",
                    ( unsigned long ) pPublishInfo->payloadLength,
                    pPublishInfo->pPayload ) );
        status = MQTTBadParameter;
    }
    else
    {
        /* Empty else MISRA 15.7 */
    }

    return status;
}


/**
*@名称 		validatePublishParams
*@功能 	    当主题以通配符字符结尾时，执行特殊匹配处理
*@参数 		const char * pTopicFilter
*@参数 		uint16_t topicFilterLength
*@参数 		uint16_t filterIndex
*@返回值 	MQTTStatus_t 
*@使用说明	
*/
static bool matchEndWildcardsSpecialCases( const char * pTopicFilter,
                                           uint16_t topicFilterLength,
                                           uint16_t filterIndex )
{
    bool matchFound = false;

    assert( pTopicFilter != NULL );
    assert( topicFilterLength != 0 );

    /* Check if the topic filter has 2 remaining characters and it ends in
     * "/#". This check handles the case to match filter "sport/#" with topic
     * "sport". The reason is that the '#' wildcard represents the parent and
     * any number of child levels in the topic name.*/
    if( ( topicFilterLength >= 3U ) &&
        ( filterIndex == ( topicFilterLength - 3U ) ) &&
        ( pTopicFilter[ filterIndex + 1U ] == '/' ) &&
        ( pTopicFilter[ filterIndex + 2U ] == '#' ) )

    {
        matchFound = true;
    }

    /* Check if the next character is "#" or "+" and the topic filter ends in
     * "/#" or "/+". This check handles the cases to match:
     *
     * - Topic filter "sport/+" with topic "sport/".
     * - Topic filter "sport/#" with topic "sport/".
     */
    if( ( filterIndex == ( topicFilterLength - 2U ) ) &&
        ( pTopicFilter[ filterIndex ] == '/' ) )
    {
        /* Check that the last character is a wildcard. */
        matchFound = ( ( pTopicFilter[ filterIndex + 1U ] == '+' ) ||
                       ( pTopicFilter[ filterIndex + 1U ] == '#' ) ) ? true : false;
    }

    return matchFound;
}

/**
*@名称 		matchTopicFilter
*@功能 	    匹配主题名称和主题筛选器，允许使用通配符
*@参数 		const char * pTopicName
*@参数 		uint16_t topicNameLength
*@参数 		const char * pTopicFilter
*@参数 		uint16_t topicFilterLength
*@返回值 	bool 
*@使用说明	
*/
static bool matchTopicFilter( const char * pTopicName,
                              uint16_t topicNameLength,
                              const char * pTopicFilter,
                              uint16_t topicFilterLength )
{
    bool matchFound = false, shouldStopMatching = false;
    uint16_t nameIndex = 0, filterIndex = 0;

    assert( pTopicName != NULL );
    assert( topicNameLength != 0 );
    assert( pTopicFilter != NULL );
    assert( topicFilterLength != 0 );

    while( ( nameIndex < topicNameLength ) && ( filterIndex < topicFilterLength ) )
    {
        /* Check if the character in the topic name matches the corresponding
         * character in the topic filter string. */
        if( pTopicName[ nameIndex ] == pTopicFilter[ filterIndex ] )
        {
            /* If the topic name has been consumed but the topic filter has not
             * been consumed, match for special cases when the topic filter ends
             * with wildcard character. */
            if( nameIndex == ( topicNameLength - 1U ) )
            {
                matchFound = matchEndWildcardsSpecialCases( pTopicFilter,
                                                            topicFilterLength,
                                                            filterIndex );
            }
        }
        else
        {
            /* Check for matching wildcards. */
            shouldStopMatching = matchWildcards( pTopicName,
                                                 topicNameLength,
                                                 pTopicFilter,
                                                 topicFilterLength,
                                                 &nameIndex,
                                                 &filterIndex,
                                                 &matchFound );
        }

        if( ( matchFound == true ) || ( shouldStopMatching == true ) )
        {
            break;
        }

        /* Increment indexes. */
        nameIndex++;
        filterIndex++;
    }

    if( matchFound == false )
    {
        /* If the end of both strings has been reached, they match. This represents the
         * case when the topic filter contains the '+' wildcard at a non-starting position.
         * For example, when matching either of "sport/+/player" OR "sport/hockey/+" topic
         * filters with "sport/hockey/player" topic name. */
        matchFound = ( ( nameIndex == topicNameLength ) &&
                       ( filterIndex == topicFilterLength ) ) ? true : false;
    }

    return matchFound;
}

//-----------------------------------------------------------------------------//
/**
*@名称 		MQTT_Init
*@功能 	    初始化MQTT上下文
*@参数 		MQTTContext_t * pContext
*@参数 		const Transport_Interface_t * pTransportInterface
*@参数 		MQTTGetCurrentTimeFunc_t getTimeFunction
*@参数 		MQTTEventCallback_t userCallback
*@参数 		const MQTTFixedBuffer_t * pNetworkBuffer
*@参数 		const MQTTFixedBuffer_t * pNetworkBufferTX
*@参数 		void * userData
*@返回值 	MQTTStatus_t 
*@使用说明	
*/
MQTTStatus_t MQTT_Init( MQTTContext_t * pContext,
                        const Transport_Interface_t * pTransportInterface,
                        MQTTGetCurrentTimeFunc_t getTimeFunction,
                        MQTTEventCallback_t userCallback,
                        const MQTTFixedBuffer_t * pNetworkBuffer,
                        const MQTTFixedBuffer_t * pNetworkBufferTX,
                        void * userData )
{
    MQTTStatus_t status = MQTTSuccess;

    /* Validate arguments. */
    if( ( pContext == NULL ) || ( pTransportInterface == NULL ) ||
        ( pNetworkBuffer == NULL ) )
    {
        LogError( ( "Argument cannot be NULL: pContext=%p, "
                    "pTransportInterface=%p, "
                    "pNetworkBuffer=%p",
                    ( void * ) pContext,
                    ( void * ) pTransportInterface,
                    ( void * ) pNetworkBuffer ) );
        status = MQTTBadParameter;
    }
    else if( getTimeFunction == NULL )
    {
        LogError( ( "Invalid parameter: getTimeFunction is NULL" ) );
        status = MQTTBadParameter;
    }
    else if( userCallback == NULL )
    {
        LogError( ( "Invalid parameter: userCallback is NULL" ) );
        status = MQTTBadParameter;
    }
    else if( pTransportInterface->Recv == NULL )
    {
        LogError( ( "Invalid parameter: pTransportInterface->recv is NULL" ) );
        status = MQTTBadParameter;
    }
    else if( pTransportInterface->Send == NULL )
    {
        LogError( ( "Invalid parameter: pTransportInterface->send is NULL" ) );
        status = MQTTBadParameter;
    }
    else
    {
        ( void ) memset( pContext, 0x00, sizeof( MQTTContext_t ) );

        pContext->connectStatus = MQTTNotConnected;
        pContext->transportInterface = *pTransportInterface;//网络传输接口
        pContext->getTime = getTimeFunction;//获取时间戳函数
        pContext->appCallback = userCallback;//应用层回调
        pContext->networkBuffer = *pNetworkBuffer;//网络数据缓存区
        pContext->networkBufferTX = *pNetworkBufferTX;////网络发送数据缓存区
        pContext->userData = userData;//用户数据

        /* Zero is not a valid packet ID per MQTT spec. Start from 1. */
        pContext->nextPacketId = 1;//ID从1开始
    }

    return status;
}

/**
*@名称 		MQTT_Connect
*@功能 	    初始化MQTT上下文
*@参数 		MQTTContext_t * pContext
*@参数 		const MQTTConnectInfo_t * pConnectInfo
*@参数 		const MQTTPublishInfo_t * pWillInfo
*@参数 		uint32_t timeoutMs
*@参数 		bool * pSessionPresent
*@返回值 	MQTTStatus_t 
*@使用说明	缓存区是 networkBuffer
*/
MQTTStatus_t MQTT_Connect( MQTTContext_t * pContext,
                           const MQTTConnectInfo_t * pConnectInfo,
                           const MQTTPublishInfo_t * pWillInfo,
                           uint32_t timeoutMs,
                           bool * pSessionPresent )
{
    size_t remainingLength = 0UL, packetSize = 0UL;
    int32_t bytesSent;
    MQTTStatus_t status = MQTTSuccess;
    MQTTPacketInfo_t incomingPacket = { 0 };

    incomingPacket.type = ( uint8_t ) 0;

    if( ( pContext == NULL ) || ( pConnectInfo == NULL ) || ( pSessionPresent == NULL ) )
    {
        LogError( ( "Argument cannot be NULL: pContext=%p, "
                    "pConnectInfo=%p, pSessionPresent=%p.",
                    ( void * ) pContext,
                    ( void * ) pConnectInfo,
                    ( void * ) pSessionPresent ) );
        status = MQTTBadParameter;
    }

    if( status == MQTTSuccess )
    {
        //获取连接数据包大小
        status = MQTT_GetConnectPacketSize( pConnectInfo,
                                            pWillInfo,
                                            &remainingLength,
                                            &packetSize );
        LogDebug( ( "CONNECT packet size is %lu and remaining length is %lu.",
                    ( unsigned long ) packetSize,
                    ( unsigned long ) remainingLength ) );
    }

    if( status == MQTTSuccess )
    {
        //序列化连接数据包到 pContext->networkBuffer
        status = MQTT_SerializeConnect( pConnectInfo,
                                        pWillInfo,
                                        remainingLength,
                                        &( pContext->networkBuffer ) );
    }

    if( status == MQTTSuccess )
    {
        //发送序列化数据包
        bytesSent = sendPacket( pContext,
                                pContext->networkBuffer.pBuffer,
                                packetSize );

        if( bytesSent < 0 )
        {
            LogError( ( "Transport send failed for CONNECT packet." ) );
            status = MQTTSendFailed;
        }
        else
        {
            LogDebug( ( "Sent %d bytes of CONNECT packet.",
                        bytesSent ) );
        }
    }

    //获取连接应答
    if( status == MQTTSuccess )
    {
        status = receiveConnack( pContext,
                                 timeoutMs,
                                 pConnectInfo->cleanSession,
                                 &incomingPacket,
                                 pSessionPresent );
    }

    if( status == MQTTSuccess )
    {
        //为重新建立的会话发送应答或清除现有记录
        status = handleSessionResumption( pContext, *pSessionPresent );
    }

    if( status == MQTTSuccess )
    {
        LogInfo( ( "MQTT connection established with the broker." ) );
        pContext->connectStatus = MQTTConnected;
        //成功连接后初始化保活周期
        pContext->keepAliveIntervalSec = pConnectInfo->keepAliveSeconds;
        pContext->waitingForPingResp = false;
        pContext->pingReqSendTimeMs = 0U;
    }
    else
    {
        LogError( ( "MQTT connection failed with status = %s.",
                    MQTT_Status_strerror( status ) ) );
    }

    return status;
}

/**
*@名称 		MQTT_Subscribe
*@功能 	    MQTT订阅
*@参数 		MQTTContext_t * pContext
*@参数 		const MQTTSubscribeInfo_t * pSubscriptionList
*@参数 		size_t subscriptionCount
*@参数 		uint16_t packetId
*@返回值 	MQTTStatus_t 
*@使用说明	缓存区是 networkBufferTX
*/
MQTTStatus_t MQTT_Subscribe( MQTTContext_t * pContext,
                             const MQTTSubscribeInfo_t * pSubscriptionList,
                             size_t subscriptionCount,
                             uint16_t packetId )
{
    size_t remainingLength = 0UL, packetSize = 0UL;
    int32_t bytesSent = 0;

    //验证参数
    MQTTStatus_t status = validateSubscribeUnsubscribeParams( pContext,
                                                              pSubscriptionList,
                                                              subscriptionCount,
                                                              packetId );

    if( status == MQTTSuccess )
    {
        //获取订阅数据包大小
        status = MQTT_GetSubscribePacketSize( pSubscriptionList,
                                              subscriptionCount,
                                              &remainingLength,
                                              &packetSize );
        LogDebug( ( "SUBSCRIBE packet size is %lu and remaining length is %lu.",
                    ( unsigned long ) packetSize,
                    ( unsigned long ) remainingLength ) );
    }

    if( status == MQTTSuccess )
    {
        //序列化订阅数据包
        status = MQTT_SerializeSubscribe( pSubscriptionList,
                                          subscriptionCount,
                                          packetId,
                                          remainingLength,
                                          &( pContext->networkBufferTX ) );
    }

    if( status == MQTTSuccess )
    {
        //发送序列化后的数据包
        bytesSent = sendPacket( pContext,
                                pContext->networkBufferTX.pBuffer,
                                packetSize );

        if( bytesSent < 0 )
        {
            LogError( ( "Transport send failed for SUBSCRIBE packet." ) );
            status = MQTTSendFailed;
        }
        else
        {
            LogDebug( ( "Sent %d bytes of SUBSCRIBE packet.",
                        bytesSent ) );
        }
    }

    return status;
}

/**
*@名称 		MQTT_Publish
*@功能 	    MQTT发布
*@参数 		MQTTContext_t * pContext
*@参数 		const MQTTPublishInfo_t * pPublishInfo
*@参数 		uint16_t packetId
*@返回值 	MQTTStatus_t 
*@使用说明	缓存区是 networkBufferTX
*/
MQTTStatus_t MQTT_Publish( MQTTContext_t * pContext,
                           const MQTTPublishInfo_t * pPublishInfo,
                           uint16_t packetId )
{
    size_t headerSize = 0UL;
    MQTTPublishState_t publishStatus = MQTTStateNull;

    //发布参数验证
    MQTTStatus_t status = validatePublishParams( pContext, pPublishInfo, packetId );

    if( status == MQTTSuccess )
    {
        //序列化发布数据包
        status = serializePublish( pContext,
                                   pPublishInfo,
                                   packetId,
                                   &headerSize );
    }

    if( ( status == MQTTSuccess ) && ( pPublishInfo->qos > MQTTQoS0 ) )
    {
        /* Reserve state for publish message. Only to be done for QoS1 or QoS2. */
        status = MQTT_ReserveState( pContext,
                                    packetId,
                                    pPublishInfo->qos );

        /* State already exists for a duplicate packet.
         * If a state doesn't exist, it will be handled as a new publish in
         * state engine. */
        if( ( status == MQTTStateCollision ) && ( pPublishInfo->dup == true ) )
        {
            status = MQTTSuccess;
        }
    }

    if( status == MQTTSuccess )
    {
        //发送序列化数据包，包含头部及负载
        status = sendPublish( pContext,
                              pPublishInfo,
                              headerSize );
    }

    if( ( status == MQTTSuccess ) && ( pPublishInfo->qos > MQTTQoS0 ) )
    {
        /* Update state machine after PUBLISH is sent.
         * Only to be done for QoS1 or QoS2. */
        status = MQTT_UpdateStatePublish( pContext,
                                          packetId,
                                          MQTT_SEND,
                                          pPublishInfo->qos,
                                          &publishStatus );

        if( status != MQTTSuccess )
        {
            LogError( ( "Update state for publish failed with status %s."
                        " However PUBLISH packet was sent to the broker."
                        " Any further handling of ACKs for the packet Id"
                        " will fail.",
                        MQTT_Status_strerror( status ) ) );
        }
    }

    if( status != MQTTSuccess )
    {
        LogError( ( "MQTT PUBLISH failed with status %s.",
                    MQTT_Status_strerror( status ) ) );
    }

    return status;
}

/**
*@名称 		MQTT_Ping
*@功能 	    MQTT保活PING
*@参数 		MQTTContext_t * pContext
*@返回值 	MQTTStatus_t 
*@使用说明	缓存区是 networkBufferTX
*/
MQTTStatus_t MQTT_Ping( MQTTContext_t * pContext )
{
    hal_mutex_lock(&pContext->mutex, 0xFFFFFFFF); 

    int32_t bytesSent = 0;
    MQTTStatus_t status = MQTTSuccess;
    size_t packetSize = 0U;

    if( pContext == NULL )
    {
        LogError( ( "pContext is NULL." ) );
        status = MQTTBadParameter;
    }

    if( status == MQTTSuccess )
    {
        /* Get MQTT PINGREQ packet size. */
        status = MQTT_GetPingreqPacketSize( &packetSize );

        if( status == MQTTSuccess )
        {
            LogDebug( ( "MQTT PINGREQ packet size is %lu.",
                        ( unsigned long ) packetSize ) );
        }
        else
        {
            LogError( ( "Failed to get the PINGREQ packet size." ) );
        }
    }

    if( status == MQTTSuccess )
    {
        /* Serialize MQTT PINGREQ. */
        status = MQTT_SerializePingreq( &( pContext->networkBuffer ) );
    }

    if( status == MQTTSuccess )
    {
        /* Send the serialized PINGREQ packet to transport layer. */
        bytesSent = sendPacket( pContext,
                                pContext->networkBuffer.pBuffer,
                                packetSize );

        /* It is an error to not send the entire PINGREQ packet. */
        if( bytesSent < 0 )
        {
            LogError( ( "Transport send failed for PINGREQ packet." ) );
            status = MQTTSendFailed;
        }
        else
        {
            pContext->pingReqSendTimeMs = pContext->lastPacketTime;
            pContext->waitingForPingResp = true;
            ENTITY_LOGI("[MQTT_KEEPALIVE][PINGREQ_SEND] bytes=%d packet_size=%u sent_ms=%u\r\n",
                        bytesSent,
                        (unsigned int)packetSize,
                        (unsigned int)pContext->pingReqSendTimeMs);
        }
    }
    hal_mutex_unlock(&pContext->mutex); 
    return status;
}

/**
*@名称 		MQTT_Unsubscribe
*@功能 	    MQTT取消订阅
*@参数 		MQTTContext_t * pContext
*@参数 		const MQTTSubscribeInfo_t * pSubscriptionList
*@参数 		size_t subscriptionCount
*@参数 		uint16_t packetId
*@返回值 	MQTTStatus_t 
*@使用说明	缓存区是 networkBufferTX
*/
MQTTStatus_t MQTT_Unsubscribe( MQTTContext_t * pContext,
                               const MQTTSubscribeInfo_t * pSubscriptionList,
                               size_t subscriptionCount,
                               uint16_t packetId )
{
    size_t remainingLength = 0UL, packetSize = 0UL;
    int32_t bytesSent = 0;

    //验证参数
    MQTTStatus_t status = validateSubscribeUnsubscribeParams( pContext,
                                                              pSubscriptionList,
                                                              subscriptionCount,
                                                              packetId );

    if( status == MQTTSuccess )
    {
        //获取取消订阅数据包大小
        status = MQTT_GetUnsubscribePacketSize( pSubscriptionList,
                                                subscriptionCount,
                                                &remainingLength,
                                                &packetSize );
        LogDebug( ( "UNSUBSCRIBE packet size is %lu and remaining length is %lu.",
                    ( unsigned long ) packetSize,
                    ( unsigned long ) remainingLength ) );
    }

    if( status == MQTTSuccess )
    {
        //序列化取消订阅数据包
        status = MQTT_SerializeUnsubscribe( pSubscriptionList,
                                            subscriptionCount,
                                            packetId,
                                            remainingLength,
                                            &( pContext->networkBufferTX ) );
    }

    if( status == MQTTSuccess )
    {
        //发送取消订阅序列化数据包
        bytesSent = sendPacket( pContext,
                                pContext->networkBufferTX.pBuffer,
                                packetSize );

        if( bytesSent < 0 )
        {
            LogError( ( "Transport send failed for UNSUBSCRIBE packet." ) );
            status = MQTTSendFailed;
        }
        else
        {
            LogDebug( ( "Sent %d bytes of UNSUBSCRIBE packet.",
                        bytesSent ) );
        }
    }

    return status;
}

/**
*@名称 		MQTT_Disconnect
*@功能 	    MQTT断开连接
*@参数 		MQTTContext_t * pContext
*@返回值 	MQTTStatus_t 
*@使用说明	缓存区是 networkBuffer
*/
MQTTStatus_t MQTT_Disconnect( MQTTContext_t * pContext )
{
    size_t packetSize = 0U;
    int32_t bytesSent = 0;
    MQTTStatus_t status = MQTTSuccess;

    /* Validate arguments. */
    if( pContext == NULL )
    {
        LogError( ( "pContext cannot be NULL." ) );
        status = MQTTBadParameter;
    }

    if( status == MQTTSuccess )
    {
        //获取断连数据包大小
        status = MQTT_GetDisconnectPacketSize( &packetSize );
        LogDebug( ( "MQTT DISCONNECT packet size is %lu.",
                    ( unsigned long ) packetSize ) );
    }

    if( status == MQTTSuccess )
    {
        //序列化断连数据包
        status = MQTT_SerializeDisconnect( &( pContext->networkBuffer ) );
    }

    if( status == MQTTSuccess )
    {
        //发送断连序列化数据包
        bytesSent = sendPacket( pContext,
                                pContext->networkBuffer.pBuffer,
                                packetSize );

        if( bytesSent < 0 )
        {
            LogError( ( "Transport send failed for DISCONNECT packet." ) );
            status = MQTTSendFailed;
        }
        else
        {
            LogDebug( ( "Sent %d bytes of DISCONNECT packet.",
                        bytesSent ) );
        }
    }

    if( status == MQTTSuccess )
    {
        LogInfo( ( "Disconnected from the broker." ) );
        pContext->connectStatus = MQTTNotConnected;
    }

    return status;
}

/**
*@名称 		MQTT_ProcessLoop
*@功能 	    循环以从传输接口接收数据包, 并处理保活
*@参数 		MQTTContext_t * pContext
*@参数 		uint32_t timeoutMs
*@返回值 	MQTTStatus_t 
*@使用说明	
*/
MQTTStatus_t MQTT_ProcessLoop( MQTTContext_t * pContext,
                               uint32_t timeoutMs )
{
    MQTTStatus_t status = MQTTBadParameter;
    uint32_t entryTimeMs = 0U, remainingTimeMs = timeoutMs, elapsedTimeMs = 0U;

    if( pContext == NULL )
    {
        LogError( ( "Invalid input parameter: MQTT Context cannot be NULL." ) );
    }
    else if( pContext->getTime == NULL )
    {
        LogError( ( "Invalid input parameter: MQTT Context must have valid getTime." ) );
    }
    else if( pContext->networkBuffer.pBuffer == NULL )
    {
        LogError( ( "Invalid input parameter: The MQTT context's networkBuffer must not be NULL." ) );
    }
    else
    {
        entryTimeMs = pContext->getTime();
        pContext->controlPacketSent = false;
        status = MQTTSuccess;
    }

    while( status == MQTTSuccess )
    {
        //单次接收数据，如果没有接收数据，则发送保活数据
        status = receiveSingleIteration( pContext, remainingTimeMs, true );

        //数据接收失败
        if( status != MQTTSuccess )
        {
            LogError( ( "Exiting process loop due to failure: ErrorStatus=%s",
                        MQTT_Status_strerror( status ) ) );
        }
        else
        {
            /* Recalculate remaining time and check if loop should exit. This is
             * done at the end so the loop will run at least a single iteration. */
            elapsedTimeMs = calculateElapsedTime( pContext->getTime(),
                                                  entryTimeMs );

            if( elapsedTimeMs >= timeoutMs )//接收超时
            {
                break;
            }

            remainingTimeMs = timeoutMs - elapsedTimeMs;
        }
    }

    return status;
}

/**
*@名称 		MQTT_ReceiveLoop
*@功能 	    循环以从传输接口接收数据包。不处理保活
*@参数 		MQTTContext_t * pContext
*@参数 		uint32_t timeoutMs
*@返回值 	MQTTStatus_t 
*@使用说明	
*/
MQTTStatus_t MQTT_ReceiveLoop( MQTTContext_t * pContext,
                               uint32_t timeoutMs )
{
    MQTTStatus_t status = MQTTBadParameter;
    uint32_t entryTimeMs = 0U, remainingTimeMs = timeoutMs, elapsedTimeMs = 0U;

    if( pContext == NULL )
    {
        LogError( ( "Invalid input parameter: MQTT Context cannot be NULL." ) );
    }
    else if( pContext->getTime == NULL )
    {
        LogError( ( "Invalid input parameter: MQTT Context must have a valid getTime function." ) );
    }
    else if( pContext->networkBuffer.pBuffer == NULL )
    {
        LogError( ( "Invalid input parameter: MQTT context's networkBuffer must not be NULL." ) );
    }
    else
    {
        entryTimeMs = pContext->getTime();
        status = MQTTSuccess;
    }

    while( status == MQTTSuccess )
    {
        status = receiveSingleIteration( pContext, remainingTimeMs, false );

        /* We don't need to break here since the status is already checked in
         * the loop condition, and we do not want multiple breaks in a loop. */
        if( status != MQTTSuccess )
        {
            LogError( ( "Exiting receive loop. Error status=%s",
                        MQTT_Status_strerror( status ) ) );
        }
        else
        {
            /* Recalculate remaining time and check if loop should exit. This is
             * done at the end so the loop will run at least a single iteration. */
            elapsedTimeMs = calculateElapsedTime( pContext->getTime(), entryTimeMs );

            if( elapsedTimeMs >= timeoutMs )
            {
                break;
            }

            remainingTimeMs = timeoutMs - elapsedTimeMs;
        }
    }

    return status;
}

/**
*@名称 		MQTT_GetPacketId
*@功能 	    获取根据MQTT 3.1.1规范有效的数据包ID
*@参数 		MQTTContext_t * pContext
*@返回值 	uint16_t 
*@使用说明	
*/
uint16_t MQTT_GetPacketId( MQTTContext_t * pContext )
{
    uint16_t packetId = 0U;

    if( pContext != NULL )
    {
        packetId = pContext->nextPacketId;

        /* A packet ID of zero is not a valid packet ID. When the max ID
         * is reached the next one should start at 1. */
        if( pContext->nextPacketId == ( uint16_t ) UINT16_MAX )
        {
            pContext->nextPacketId = 1;
        }
        else
        {
            pContext->nextPacketId++;
        }
    }

    return packetId;
}

/**
*@名称 		MQTT_MatchTopic
*@功能 	    主题名称匹配
*@参数 		const char * pTopicName
*@参数 		const uint16_t topicNameLength
*@参数 		const char * pTopicFilter
*@参数 		const uint16_t topicFilterLength
*@参数 		bool * pIsMatch
*@返回值 	MQTTStatus_t 
*@使用说明	
*/
MQTTStatus_t MQTT_MatchTopic( const char * pTopicName,
                              const uint16_t topicNameLength,
                              const char * pTopicFilter,
                              const uint16_t topicFilterLength,
                              bool * pIsMatch )
{
    MQTTStatus_t status = MQTTSuccess;
    bool topicFilterStartsWithWildcard = false;
    bool matchStatus = false;

    if( ( pTopicName == NULL ) || ( topicNameLength == 0u ) )
    {
        LogError( ( "Invalid paramater: Topic name should be non-NULL and its "
                    "length should be > 0: TopicName=%p, TopicNameLength=%u",
                    ( void * ) pTopicName,
                    topicNameLength ) );

        status = MQTTBadParameter;
    }
    else if( ( pTopicFilter == NULL ) || ( topicFilterLength == 0u ) )
    {
        LogError( ( "Invalid paramater: Topic filter should be non-NULL and "
                    "its length should be > 0: TopicName=%p, TopicFilterLength=%u",
                    ( void * ) pTopicFilter,
                    topicFilterLength ) );
        status = MQTTBadParameter;
    }
    else if( pIsMatch == NULL )
    {
        LogError( ( "Invalid paramater: Output parameter, pIsMatch, is NULL" ) );
        status = MQTTBadParameter;
    }
    else
    {
        /* Check for an exact match if the incoming topic name and the registered
         * topic filter length match. */
        if( topicNameLength == topicFilterLength )
        {
            matchStatus = ( strncmp( pTopicName, pTopicFilter, topicNameLength ) == 0 ) ? true : false;
        }

        if( matchStatus == false )
        {
            /* If an exact match was not found, match against wildcard characters in
             * topic filter.*/

            /* Determine if topic filter starts with a wildcard. */
            topicFilterStartsWithWildcard = ( ( pTopicFilter[ 0 ] == '+' ) ||
                                              ( pTopicFilter[ 0 ] == '#' ) ) ? true : false;

            /* Note: According to the MQTT 3.1.1 specification, incoming PUBLISH topic names
             * starting with "$" character cannot be matched against topic filter starting with
             * a wildcard, i.e. for example, "$SYS/sport" cannot be matched with "#" or
             * "+/sport" topic filters. */
            if( !( ( pTopicName[ 0 ] == '$' ) && ( topicFilterStartsWithWildcard == true ) ) )
            {
                matchStatus = matchTopicFilter( pTopicName, topicNameLength, pTopicFilter, topicFilterLength );
            }
        }

        /* Update the output parameter with the match result. */
        *pIsMatch = matchStatus;
    }

    return status;
}

/**
*@名称 		MQTT_GetSubAckStatusCodes
*@功能 	    获取订阅应答状态码
*@参数 		const MQTTPacketInfo_t * pSubackPacket
*@参数 		uint8_t ** pPayloadStart
*@参数 		size_t * pPayloadSize
*@返回值 	MQTTStatus_t 
*@使用说明	解析包含状态代码的MQTT SUBACK数据包的有效载荷对应于原始主题筛选器订阅请求,订阅数据包
*/
MQTTStatus_t MQTT_GetSubAckStatusCodes( const MQTTPacketInfo_t * pSubackPacket,
                                        uint8_t ** pPayloadStart,
                                        size_t * pPayloadSize )
{
    MQTTStatus_t status = MQTTSuccess;

    if( pSubackPacket == NULL )
    {
        LogError( ( "Invalid parameter: pSubackPacket is NULL." ) );
        status = MQTTBadParameter;
    }
    else if( pPayloadStart == NULL )
    {
        LogError( ( "Invalid parameter: pPayloadStart is NULL." ) );
        status = MQTTBadParameter;
    }
    else if( pPayloadSize == NULL )
    {
        LogError( ( "Invalid parameter: pPayloadSize is NULL." ) );
        status = MQTTBadParameter;
    }
    else if( pSubackPacket->type != MQTT_PACKET_TYPE_SUBACK )
    {
        LogError( ( "Invalid parameter: Input packet is not a SUBACK packet: "
                    "ExpectedType=%02x, InputType=%02x",
                    MQTT_PACKET_TYPE_SUBACK, pSubackPacket->type ) );
        status = MQTTBadParameter;
    }
    else if( pSubackPacket->pRemainingData == NULL )
    {
        LogError( ( "Invalid parameter: pSubackPacket->pRemainingData is NULL" ) );
        status = MQTTBadParameter;
    }

    /* A SUBACK must have a remaining length of at least 3 to accommodate the
     * packet identifier and at least 1 return code. */
    else if( pSubackPacket->remainingLength < 3U )
    {
        LogError( ( "Invalid parameter: Packet remaining length is invalid: "
                    "Should be greater than 2 for SUBACK packet: InputRemainingLength=%lu",
                    ( unsigned long ) pSubackPacket->remainingLength ) );
        status = MQTTBadParameter;
    }
    else
    {
        /* According to the MQTT 3.1.1 protocol specification, the "Remaining Length" field is a
         * length of the variable header (2 bytes) plus the length of the payload.
         * Therefore, we add 2 positions for the starting address of the payload, and
         * subtract 2 bytes from the remaining length for the length of the payload.*/
        *pPayloadStart = pSubackPacket->pRemainingData + ( ( uint16_t ) sizeof( uint16_t ) );
        *pPayloadSize = pSubackPacket->remainingLength - sizeof( uint16_t );
    }

    return status;
}

/*-----------------------------------------------------------*/
/**
*@名称 		MQTT_Status_strerror
*@功能 	    MQTT状态的字符串转换错误代码
*@参数 		MQTTStatus_t status
*@返回值 	char * 
*@使用说明	解析包含状态代码的MQTT SUBACK数据包的有效载荷对应于原始主题筛选器订阅请求,订阅数据包
*/
const char * MQTT_Status_strerror( MQTTStatus_t status )
{
    const char * str = NULL;

    switch( status )
    {
        case MQTTSuccess:
            str = "MQTTSuccess";
            break;

        case MQTTBadParameter:
            str = "MQTTBadParameter";
            break;

        case MQTTNoMemory:
            str = "MQTTNoMemory";
            break;

        case MQTTSendFailed:
            str = "MQTTSendFailed";
            break;

        case MQTTRecvFailed:
            str = "MQTTRecvFailed";
            break;

        case MQTTBadResponse:
            str = "MQTTBadResponse";
            break;

        case MQTTServerRefused:
            str = "MQTTServerRefused";
            break;

        case MQTTNoDataAvailable:
            str = "MQTTNoDataAvailable";
            break;

        case MQTTIllegalState:
            str = "MQTTIllegalState";
            break;

        case MQTTStateCollision:
            str = "MQTTStateCollision";
            break;

        case MQTTKeepAliveTimeout:
            str = "MQTTKeepAliveTimeout";
            break;

        case MQTTNotAuthorized:
            str = "Connection not authorized";
            break;

        default:
            str = "Invalid MQTT Status code";
            break;
    }

    return str;
}
