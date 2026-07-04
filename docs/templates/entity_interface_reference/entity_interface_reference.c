#include "entity_interface_reference.h"

#include "entity_iot_func.h"
#include "entity_log.h"

#include <string.h>

#define ENTITY_INTERFACE_REF_WORK_QUEUE_DEPTH      8
#define ENTITY_INTERFACE_REF_WORKER_STACK_SIZE     6144
#define ENTITY_INTERFACE_REF_WORKER_PRIORITY       4

static Entity_Interface_Reference_Config_t s_config;
static Entity_Interface_Reference_Hooks s_hooks;
static Entity_Queue_t s_work_queue = 0;
static Entity_Thread_t s_worker_thread = 0;
static unsigned char s_worker_started = 0;
static unsigned char s_interface_inited = 0;

static void Entity_Interface_Reference_Handle_Work(const Entity_Interface_Reference_Work_t *work)
{
    if (work == 0)
    {
        return;
    }

    switch (work->type)
    {
        case ENTITY_INTERFACE_REF_WORK_REPORT_PROPERTY:
            if (s_config.report_property != 0)
            {
                s_config.report_property(work->value, work->payload, s_config.user);
            }
            break;
        case ENTITY_INTERFACE_REF_WORK_ENTER_PROVISIONING:
            if (s_config.enter_provisioning != 0)
            {
                s_config.enter_provisioning((unsigned char)work->value, s_config.user);
            }
            break;
        case ENTITY_INTERFACE_REF_WORK_DEVICE_ACCESS_REQUEST:
            if (s_config.request_device_access != 0)
            {
                (void)s_config.request_device_access(s_config.user);
            }
            break;
        case ENTITY_INTERFACE_REF_WORK_DEVICE_ACCESS_STOP:
            if (s_config.stop_device_access != 0)
            {
                s_config.stop_device_access(s_config.user);
            }
            break;
        case ENTITY_INTERFACE_REF_WORK_MANUAL_RESET:
            if (s_config.manual_reset != 0)
            {
                s_config.manual_reset((unsigned char)work->value, s_config.user);
            }
            break;
        case ENTITY_INTERFACE_REF_WORK_MANUAL_CONFIG_NET:
            if (s_config.manual_config_net != 0)
            {
                s_config.manual_config_net((unsigned char)work->value, s_config.user);
            }
            break;
        case ENTITY_INTERFACE_REF_WORK_PRODUCT_CUSTOM:
            if (s_config.product_custom != 0)
            {
                s_config.product_custom(work, s_config.user);
            }
            break;
        default:
            ENTITY_LOGW("[ENTITY_INTERFACE_REF] unknown work type=%d", (int)work->type);
            break;
    }
}

static void *Entity_Interface_Reference_Worker(void *arg)
{
    (void)arg;

    while (1)
    {
        Entity_Interface_Reference_Work_t work;
        uint32_t msg_size = sizeof(work);
        memset(&work, 0, sizeof(work));

        if (Entity_Msg_Queue_Wait(&s_work_queue, &work, &msg_size, ENTITY_WAIT_FOREVER) != 0)
        {
            continue;
        }

        Entity_Interface_Reference_Handle_Work(&work);
    }

    return 0;
}

static int Entity_Interface_Reference_Enqueue_Scalar_Work(Entity_Interface_Reference_Work_Type_e type,
                                                          uint32_t value,
                                                          void *payload)
{
    Entity_Interface_Reference_Work_t work;
    memset(&work, 0, sizeof(work));
    work.type = type;
    work.value = value;
    work.payload = payload;
    return Entity_Interface_Reference_Work_Enqueue(&work);
}

int Entity_Interface_Reference_Work_Enqueue(const Entity_Interface_Reference_Work_t *work)
{
    if (work == 0)
    {
        return -1;
    }

    if (!s_worker_started || (s_work_queue == 0) || (Entity_Msg_Queue_Send == 0))
    {
        ENTITY_LOGW("[ENTITY_INTERFACE_REF] drop work type=%d: worker not ready", (int)work->type);
        return -1;
    }

    if (Entity_Msg_Queue_Send(&s_work_queue, (void *)work, sizeof(*work), 0) != 0)
    {
        ENTITY_LOGE("[ENTITY_INTERFACE_REF] queue full, drop work type=%d", (int)work->type);
        return -1;
    }

    return 0;
}

int Entity_Interface_Reference_Init(const Entity_Interface_Reference_Config_t *config)
{
    int ret;

    if (config != 0)
    {
        s_config = *config;
    }

    if (s_worker_started)
    {
        return 0;
    }

    if ((Entity_Msg_Queue_Create == 0) || (Entity_Pthread_Create == 0))
    {
        ENTITY_LOGE("[ENTITY_INTERFACE_REF] queue/thread functions are not bound");
        return -1;
    }

    ret = Entity_Msg_Queue_Create(&s_work_queue,
                                  ENTITY_INTERFACE_REF_WORK_QUEUE_DEPTH,
                                  sizeof(Entity_Interface_Reference_Work_t));
    if (ret != 0)
    {
        ENTITY_LOGE("[ENTITY_INTERFACE_REF] queue create failed ret=%d", ret);
        return -1;
    }

    ret = Entity_Pthread_Create(&s_worker_thread,
                                "entity_if_ref",
                                ENTITY_INTERFACE_REF_WORKER_STACK_SIZE,
                                ENTITY_INTERFACE_REF_WORKER_PRIORITY,
                                (void *)Entity_Interface_Reference_Worker,
                                0);
    if (ret != 0)
    {
        ENTITY_LOGE("[ENTITY_INTERFACE_REF] worker create failed ret=%d", ret);
        if (Entity_Msg_Queue_Delete != 0)
        {
            Entity_Msg_Queue_Delete(&s_work_queue);
        }
        s_work_queue = 0;
        return -1;
    }

    s_worker_started = 1;
    ENTITY_LOGI("[ENTITY_INTERFACE_REF] worker started");
    return 0;
}

void Entity_Product_Hooks_Register(const Entity_Interface_Reference_Hooks *hooks)
{
    if (hooks == 0)
    {
        memset(&s_hooks, 0, sizeof(s_hooks));
        return;
    }

    s_hooks = *hooks;
}

void Entity_Product_Hooks_On_Dev_Status(unsigned char status)
{
    if (s_hooks.on_dev_status_change != 0)
    {
        s_hooks.on_dev_status_change(status);
    }
}

void Entity_Product_Hooks_On_Dp_Received(void *dp_obj)
{
    if (s_hooks.on_dp_obj_received != 0)
    {
        s_hooks.on_dp_obj_received(dp_obj);
    }
}

void Entity_Product_Hooks_On_Ai_Token_Result(const Entity_Interface_Reference_Ai_Token_Result_t *result)
{
    if (s_hooks.on_ai_token_result != 0)
    {
        s_hooks.on_ai_token_result(result, s_hooks.ai_token_user);
    }
}

void Entity_Iot_Interface_Init(void)
{
    if (s_interface_inited)
    {
        return;
    }
    s_interface_inited = 1;

    if (s_config.init_system_imports != 0)
    {
        s_config.init_system_imports();
    }
    if (s_config.init_periph_imports != 0)
    {
        s_config.init_periph_imports();
    }
    if (s_config.init_mqtt_imports != 0)
    {
        s_config.init_mqtt_imports();
    }
}

bool Entity_Device_Access_Export_Interface(void)
{
    return Entity_Interface_Reference_Enqueue_Scalar_Work(
               ENTITY_INTERFACE_REF_WORK_DEVICE_ACCESS_REQUEST, 0, 0) == 0;
}

void Entity_Device_Access_Stop_Export_Interface(void)
{
    (void)Entity_Interface_Reference_Enqueue_Scalar_Work(
        ENTITY_INTERFACE_REF_WORK_DEVICE_ACCESS_STOP, 0, 0);
}

void Entity_Manual_Reset_Export_Interface(unsigned char need_clear)
{
    (void)Entity_Interface_Reference_Enqueue_Scalar_Work(
        ENTITY_INTERFACE_REF_WORK_MANUAL_RESET, need_clear, 0);
}

void Entity_Manual_Config_Net_Export_Interface(unsigned char need_clear)
{
    (void)Entity_Interface_Reference_Enqueue_Scalar_Work(
        ENTITY_INTERFACE_REF_WORK_MANUAL_CONFIG_NET, need_clear, 0);
}

/*
 * Callback safety rule:
 * Product callbacks must not call heavy Entity APIs from product callbacks.
 * In key/GPIO/timer/BLE/WiFi or network callback contexts, they should create
 * Entity_Interface_Reference_Work_t and call Entity_Interface_Reference_Work_Enqueue()
 * instead.
 *
 * Product-owned behavior belongs behind Entity_Interface_Reference_Config_t:
 * DP identifiers, key mapping, audio/RTC policy, provisioning UX, and cloud payload
 * construction stay in the product copy of this template, not in Entity core.
 */
