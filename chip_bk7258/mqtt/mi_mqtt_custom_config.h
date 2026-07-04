//mi_mqtt_custom_config.h
#pragma once


//待确认的最大PUBLISH消息的默认值
#define MQTT_STATE_ARRAY_MAX_COUNT    ( 10U )

//在保活时发送PING时应答的超时时间 
#define MQTT_PINGRESP_TIMEOUT_MS      ( 1000U * 30U )

//接收缓存区大小
#define MQTT_RX_BUFFER_SIZE         ( 1024U * 10U)

//发送缓存区大小
#define MQTT_TX_BUFFER_SIZE         ( 1024U * 4U )

















