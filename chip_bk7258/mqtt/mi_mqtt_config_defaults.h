//mi_mqtt_config_defaults.h
#pragma once


#ifdef DOXYGEN
    #define MI_MQTT_DO_NOT_USE_CUSTOM_CONFIG
#endif

//待确认的最大PUBLISH消息的默认值
#ifndef MQTT_STATE_ARRAY_MAX_COUNT
    #define MQTT_STATE_ARRAY_MAX_COUNT    ( 10U )
#endif

//在建立连接时，超时前，未收到应答重试的次数
#ifndef MQTT_MAX_CONNACK_RECEIVE_RETRY_COUNT
    #define MQTT_MAX_CONNACK_RECEIVE_RETRY_COUNT    ( 5U )
#endif

//在保活时发送PING时应答的超时时间 
#ifndef MQTT_PINGRESP_TIMEOUT_MS
    #define MQTT_PINGRESP_TIMEOUT_MS    ( 1000U * 30U )
#endif


//接收缓存区大小
#ifndef MQTT_RX_BUFFER_SIZE
#define MQTT_RX_BUFFER_SIZE         ( 1024U * 10U)
#endif

//发送缓存区大小
#ifndef MQTT_TX_BUFFER_SIZE
#define MQTT_TX_BUFFER_SIZE         ( 1024U * 4U )
#endif


//错误输出，默认关闭
#ifndef LogError
    #define LogError( message )
#endif

//警告输出，默认关闭
#ifndef LogWarn
    #define LogWarn( message )
#endif

//信息输出，默认关闭
#ifndef LogInfo
    #define LogInfo( message )
#endif

//调试输出，默认关闭
#ifndef LogDebug
    #define LogDebug( message )
#endif



















