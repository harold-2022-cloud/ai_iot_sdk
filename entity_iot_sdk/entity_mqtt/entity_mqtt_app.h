//entity_mqtt_app.h
#pragma once

#include "entity_mqtt_client.h"

typedef struct 
{
    void (*Entity_Mqtt_Msg_Event_Cb)(void);  //事件消息回调
    void (*Entity_Mqtt_Msg_Cmd_Cb)(void);    //命令消息回调
}Entity_Mqtt_App_Msg_Reach_Callback_t;

typedef void (*Entity_Mqtt_App_Ready_Callback_f)(void);//连上MQTT后并上报设备信息后的回调处理 ，设备可能还需要上报自身DP点。

typedef struct
{
    bool app_connected;
    bool ctx_connected;
    uint8_t state;
    uint32_t keepalive_ms;
    uint32_t last_rx_age_ms;
    bool wait_ping;
    uint32_t pingreq_age_ms;
    uint8_t broker_state;
    uint32_t broker_pingresp_timeout_ms;
    uint32_t broker_last_inbound_alive_age_ms;
    uint32_t last_pingresp_rtt_ms;
    uint32_t mqtt_task_progress_age_ms;
    uint32_t mqtt_task_last_loop_gap_ms;
    uint32_t mqtt_task_max_loop_gap_ms;
    uint32_t mqtt_task_last_poll_ms;
    uint32_t mqtt_task_max_poll_ms;
    uint32_t mqtt_task_stack_hwm;
    uint32_t mqtt_core_snapshot_wait_ms;
    uint32_t mqtt_event_count;
    uint32_t mqtt_last_event_age_ms;
    uint32_t mqtt_callback_active_age_ms;
    uint32_t mqtt_callback_last_ms;
    uint32_t mqtt_callback_max_ms;
    uint32_t mqtt_callback_slow_count;
    uint32_t pending_qos1;
    uint32_t oldest_qos1_age_ms;
    uint16_t oldest_qos1_msgid;
    uint32_t agent_queue_depth;
    uint8_t mqtt_task_core;
    uint8_t mqtt_task_priority;
    bool mqtt_callback_active;
} Entity_Mqtt_App_Health_Snapshot_t;

typedef enum
{
    ENTITY_MQTT_AI_PUBLISH_READY = 0,
    ENTITY_MQTT_AI_PUBLISH_DEFERRED_PROBING,
    ENTITY_MQTT_AI_PUBLISH_BLOCKED_DISCONNECTED,
    ENTITY_MQTT_AI_PUBLISH_BLOCKED_DEAD,
    ENTITY_MQTT_AI_PUBLISH_BLOCKED_PENDING,
    ENTITY_MQTT_AI_PUBLISH_FAILED,
} Entity_Mqtt_Ai_Publish_Result_t;

//注册MQTT准备就绪回调
void Register_Entity_Mqtt_App_Ready_Cb(Entity_Mqtt_App_Ready_Callback_f cb);

//准备就绪回调（返回值拷貝，線程安全）
Entity_Mqtt_App_Ready_Callback_f Get_Entity_Mqtt_App_Ready_Cbs(void);

//注册MQTT消息到达回调
void Register_Entity_Mqtt_App_Msg_Reach_Cbs(Entity_Mqtt_App_Msg_Reach_Callback_t *cbs);

//获取MQTT消息到达回调（返回值拷貝，線程安全）
Entity_Mqtt_App_Msg_Reach_Callback_t Get_Entity_Mqtt_App_Msg_Reach_Cbs(void);

//创建MQTT任务
int Entity_Mqtt_Client_Task_Start(void);

//删除MQTT任务
int Entity_Mqtt_Client_Task_Stop(void);

//MQTT应用订阅主题
int Entity_Mqtt_App_Topic_Subscribe(Topic_Type_e type_e, int qos);

//MQTT应用取消订阅主题
int Entity_Mqtt_App_Topic_Unsubscribe(Topic_Type_e type_e, int qos);

//MQTT应用发布主题
int Entity_Mqtt_App_Topic_Publish(Topic_Type_e type_e, const char *data, int len, int qos, int retained);

//查询MQTT是否已连接
bool Entity_Mqtt_App_Is_Connected(void);

//AI 请求发布前检查 MQTT RX/PUBACK 健康；必要时主动重连并返回 false
bool Entity_Mqtt_App_Prepare_Ai_Publish(void);
Entity_Mqtt_Ai_Publish_Result_t Entity_Mqtt_App_Prepare_Ai_Publish_Result(void);

//AI token 等待期间只检查 MQTT RX/PUBACK 健康，不触发重连
bool Entity_Mqtt_App_Is_Ai_Link_Healthy(void);

//获取 MQTT 健康诊断快照，供 NET_QUALITY 单行日志使用
bool Entity_Mqtt_App_Get_Health_Snapshot(Entity_Mqtt_App_Health_Snapshot_t *snapshot);

//MQTT 状态名
const char *Entity_Mqtt_App_State_Name(uint8_t state);
const char *Entity_Mqtt_App_Broker_State_Name(uint8_t state);

//主动要求 MQTT 重连；AI token 超时等关键路径使用
bool Entity_Mqtt_App_Force_Reconnect(const char *reason);

/* 兼容旧调用点：esp-mqtt keepalive 已恢复为内建维护，此函数仅记录 no-op 日志。 */
void Entity_Mqtt_App_Reset_Keepalive(void);
