//mi_mqtt_client.h  — esp-mqtt 版本，移除 coreMQTT 依賴
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "mi_mqtt_types.h"

//MQTT客户端状态
typedef enum Mqtt_Client_Status
{
    MQTT_STATUS_SUCCESS = 0,
    MQTT_STATUS_INVALID_PARAM,
    MQTT_STATUS_CONNECT_FAILED,
    MQTT_STATUS_NOT_AUTHORIZED,
    MQTT_STATUS_NETWORK_INIT_FAILED,
    MQTT_STATUS_NETWORK_CONNECT_FAILED,
    MQTT_STATUS_NETWORK_TIMEOUT,
} Mqtt_Client_Status_t;

//服务质量
typedef enum Mqtt_Client_Qos
{
    MQTT_QOS_0 = 0,
    MQTT_QOS_1 = 1,
    MQTT_QOS_2 = 2,
} Mqtt_Client_Qos_t;

typedef enum Mqtt_Client_Broker_Liveness
{
    MQTT_BROKER_LIVENESS_DISCONNECTED = 0,
    MQTT_BROKER_LIVENESS_CONNECTING,
    MQTT_BROKER_LIVENESS_SUBSCRIBING,
    MQTT_BROKER_LIVENESS_READY,
    MQTT_BROKER_LIVENESS_PROBING,
    MQTT_BROKER_LIVENESS_DEAD,
} Mqtt_Client_Broker_Liveness_t;

//MQTT消息相关
typedef struct Mqtt_Client_Message
{
    const char    *Topic;
    const uint8_t *Payload;
    size_t         Length;
    Mqtt_Client_Qos_t Qos;
} Mqtt_Client_Message_t;

//MQTT客户端配置信息
typedef struct
{
    Tls_Connect_Params_t Tls_Connect_Params;
    Tcp_Connect_Params_t Tcp_Connect_Params;
    uint16_t     Keepalive;
    const char  *Client_Id;
    const char  *Username;
    const char  *Password;
    void        *Userdata;
    void (*On_Connected)  (void *client_context, void *userdata);
    void (*On_Disconnected)(void *client_context, void *userdata);
    void (*On_Message)    (void *client_context, uint16_t msgid, const Mqtt_Client_Message_t *msg, void *userdata);
    void (*On_Published)  (void *client_context, uint16_t msgid, void *userdata);
    void (*On_Subscribed) (void *client_context, uint16_t msgid, void *userdata);
    void (*On_Unsubscribed)(void *client_context, uint16_t msgid, void *userdata);
} Mqtt_Client_Config_t;

typedef struct
{
    Mqtt_Client_Broker_Liveness_t state;
    uint32_t keepalive_ms;
    uint32_t pingresp_timeout_ms;
    bool wait_ping;
    uint32_t pingreq_age_ms;
    uint32_t last_inbound_alive_age_ms;
    uint32_t last_pingresp_rtt_ms;
    uint32_t task_progress_age_ms;
    uint32_t task_last_loop_gap_ms;
    uint32_t task_max_loop_gap_ms;
    uint32_t task_last_poll_ms;
    uint32_t task_max_poll_ms;
    uint32_t task_stack_hwm;
    uint32_t core_snapshot_wait_ms;
    uint32_t event_count;
    uint32_t last_event_age_ms;
    uint32_t callback_active_age_ms;
    uint32_t callback_last_ms;
    uint32_t callback_max_ms;
    uint32_t callback_slow_count;
    uint8_t task_core;
    uint8_t task_priority;
    bool callback_active;
} Mqtt_Client_Broker_Liveness_Snapshot_t;

/* 不透明 context —— 内部使用 esp_mqtt_client_handle_t，外部只用 void* */
struct Mqtt_Client_Context;
typedef struct Mqtt_Client_Context Mqtt_Client_Context_t;

void                *Mqtt_Client_New(void);
void                 Mqtt_Client_Free(void *client_context);
Mqtt_Client_Status_t Mqtt_Client_Init(void *client_context, const Mqtt_Client_Config_t *config);
Mqtt_Client_Status_t Mqtt_Client_Deinit(void *client_context);
Mqtt_Client_Status_t Mqtt_Client_Connect(void *client_context);
Mqtt_Client_Status_t Mqtt_Client_Disconnect(void *client_context);
uint16_t             Mqtt_Client_Subscribe(void *client_context, const char *topic, uint8_t qos);
uint16_t             Mqtt_Client_Unsubscribe(void *client_context, const char *topic, uint8_t qos);
int                  Mqtt_Client_Publish(void *client_context, const char *topic, const uint8_t *payload, size_t length, uint8_t qos);
Mqtt_Client_Status_t Mqtt_Client_Yield(void *client_context);
uint32_t             Mqtt_Client_Last_Rx_Age_Ms(void *client_context);
bool                 Mqtt_Client_Pending_Qos1_Snapshot(void *client_context, uint32_t *pending_count, uint32_t *oldest_age_ms, uint16_t *oldest_msg_id);
bool                 Mqtt_Client_Pingresp_Snapshot(void *client_context, bool *wait_ping, uint32_t *pingreq_age_ms);
bool                 Mqtt_Client_Pingresp_Diag_Snapshot(void *client_context, bool *wait_ping, uint32_t *pingreq_age_ms, uint32_t *last_pingresp_rtt_ms);
const char          *Mqtt_Client_Broker_Liveness_Str(Mqtt_Client_Broker_Liveness_t state);
bool                 Mqtt_Client_Broker_Liveness_Snapshot(void *client_context, Mqtt_Client_Broker_Liveness_Snapshot_t *snapshot);

/* no-op after migration — esp-mqtt handles keepalive internally */
void                 Mqtt_Client_Reset_Keepalive(void *client_context);

char    *Mqtt_Client_Interface_Local_Ip(void *client_context, char *buf, int max_size);
char    *Mqtt_Client_Interface_Mac(void *client_context, char *buf, int max_size);
char    *Mqtt_Client_Interface_Name(void *client_context, char *buf, int max_size);
uint32_t Mqtt_Client_Ping_Rtt_Ms(void *client_context);
