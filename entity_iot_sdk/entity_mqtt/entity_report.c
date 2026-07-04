#include "entity_report.h"

#include "entity_http_event_report.h"
#include "entity_iot_func.h"
#include "entity_log.h"
#include "entity_mqtt_app.h"

#include <stdio.h>
#include <string.h>

#define ENTITY_REPORT_HTTP_URL_MAX_LEN      256
#define ENTITY_REPORT_QUEUE_LEN             8
#define ENTITY_REPORT_WORKER_STACK_SIZE     6144
#define ENTITY_REPORT_WORKER_PRIORITY       4

typedef struct
{
    char *json;
    size_t len;
    Mqtt_Qos_Type_e qos;
    Entity_Report_Transport_e transport;
} Entity_Report_Work_Item_t;

static Entity_Report_Transport_e s_report_transport = ENTITY_REPORT_TRANSPORT_MQTT;
static char s_http_endpoint[ENTITY_REPORT_HTTP_URL_MAX_LEN];
static Entity_Critical_t s_report_lock = NULL;
static unsigned char s_report_lock_ready = 0;
static Entity_Queue_t s_report_queue = NULL;
static Entity_Thread_t s_report_worker = NULL;
static unsigned char s_report_worker_ready = 0;

static void Entity_Report_Lock_Init(void)
{
    if ((s_report_lock_ready == 0) && Entity_Critical_Create)
    {
        if (Entity_Critical_Create(&s_report_lock) == 0)
        {
            s_report_lock_ready = 1;
        }
    }
}

static void Entity_Report_Lock(void)
{
    Entity_Report_Lock_Init();
    if (s_report_lock_ready && Entity_Critical_Enter)
    {
        Entity_Critical_Enter(&s_report_lock);
    }
}

static void Entity_Report_Unlock(void)
{
    if (s_report_lock_ready && Entity_Critical_Exit)
    {
        Entity_Critical_Exit(&s_report_lock);
    }
}

void Entity_Report_Transport_Set(Entity_Report_Transport_e transport)
{
    if ((transport != ENTITY_REPORT_TRANSPORT_MQTT) &&
        (transport != ENTITY_REPORT_TRANSPORT_HTTP))
    {
        ENTITY_LOGE("[REPORT_TRANSPORT] invalid transport=%d", (int)transport);
        return;
    }

    Entity_Report_Lock();
    s_report_transport = transport;
    Entity_Report_Unlock();

    ENTITY_LOGI("[REPORT_TRANSPORT] set transport=%s",
                (transport == ENTITY_REPORT_TRANSPORT_HTTP) ? "http" : "mqtt");
}

Entity_Report_Transport_e Entity_Report_Transport_Get(void)
{
    Entity_Report_Transport_e transport;

    Entity_Report_Lock();
    transport = s_report_transport;
    Entity_Report_Unlock();

    return transport;
}

int Entity_Report_Http_Endpoint_Set(const char *url)
{
    size_t len;

    if ((url == NULL) || (url[0] == '\0'))
    {
        ENTITY_LOGE("[REPORT_TRANSPORT] http endpoint invalid");
        return -1;
    }

    len = strlen(url);
    if (len >= sizeof(s_http_endpoint))
    {
        ENTITY_LOGE("[REPORT_TRANSPORT] http endpoint too long len=%u max=%u",
                    (unsigned int)len,
                    (unsigned int)(sizeof(s_http_endpoint) - 1));
        return -1;
    }

    Entity_Report_Lock();
    memcpy(s_http_endpoint, url, len + 1);
    Entity_Report_Unlock();

    ENTITY_LOGI("[REPORT_TRANSPORT] http endpoint set len=%u", (unsigned int)len);
    return 0;
}

static void Entity_Report_Http_Endpoint_Copy(char *out, size_t out_len)
{
    if ((out == NULL) || (out_len == 0))
    {
        return;
    }

    Entity_Report_Lock();
    (void)snprintf(out, out_len, "%s", s_http_endpoint);
    Entity_Report_Unlock();
}

static void Entity_Report_Work_Item_Free(Entity_Report_Work_Item_t *item)
{
    if ((item != NULL) && (item->json != NULL))
    {
        cJSON_free(item->json);
        item->json = NULL;
    }
}

static void *Entity_Report_Worker_Thread(void *arg)
{
    (void)arg;

    while (1)
    {
        Entity_Report_Work_Item_t item = {0};
        uint32_t msg_size = sizeof(item);

        if (Entity_Msg_Queue_Wait(&s_report_queue, &item, &msg_size, ENTITY_WAIT_FOREVER) != 0)
        {
            continue;
        }

        if ((item.json == NULL) || (item.len == 0))
        {
            Entity_Report_Work_Item_Free(&item);
            continue;
        }

        if (item.transport == ENTITY_REPORT_TRANSPORT_HTTP)
        {
            char endpoint[ENTITY_REPORT_HTTP_URL_MAX_LEN] = {0};
            Entity_Report_Http_Endpoint_Copy(endpoint, sizeof(endpoint));
            if (endpoint[0] == '\0')
            {
                ENTITY_LOGE("[REPORT_TRANSPORT] drop http report: endpoint not set len=%u",
                            (unsigned int)item.len);
            }
            else
            {
                int rc = Entity_Http_Event_Report_Post_Json(endpoint, item.json, item.len);
                ENTITY_LOGI("[REPORT_TRANSPORT] http report sent len=%u rc=%d",
                            (unsigned int)item.len,
                            rc);
            }
        }
        else
        {
            int rc = Entity_Mqtt_App_Topic_Publish(TOPIC_TYPE_EVENT_PUBLISH,
                                                   item.json,
                                                   (int)item.len,
                                                   item.qos,
                                                   0);
            ENTITY_LOGI("[REPORT_TRANSPORT] mqtt report queued len=%u qos=%d rc=%d",
                        (unsigned int)item.len,
                        (int)item.qos,
                        rc);
        }

        Entity_Report_Work_Item_Free(&item);
    }

    return NULL;
}

int Entity_Report_Worker_Init(void)
{
    int ret;

    Entity_Report_Lock_Init();
    if (s_report_worker_ready)
    {
        return 0;
    }

    ret = Entity_Msg_Queue_Create(&s_report_queue,
                                  ENTITY_REPORT_QUEUE_LEN,
                                  sizeof(Entity_Report_Work_Item_t));
    if (ret != 0)
    {
        ENTITY_LOGE("[REPORT_TRANSPORT] queue create failed ret=%d", ret);
        return -1;
    }

    ret = Entity_Pthread_Create(&s_report_worker,
                                "report worker",
                                ENTITY_REPORT_WORKER_STACK_SIZE,
                                ENTITY_REPORT_WORKER_PRIORITY,
                                Entity_Report_Worker_Thread,
                                NULL);
    if (ret != 0)
    {
        ENTITY_LOGE("[REPORT_TRANSPORT] worker create failed ret=%d", ret);
        Entity_Msg_Queue_Delete(&s_report_queue);
        return -1;
    }

    s_report_worker_ready = 1;
    ENTITY_LOGI("[REPORT_TRANSPORT] worker started queue=%u stack=%u priority=%u",
                (unsigned int)ENTITY_REPORT_QUEUE_LEN,
                (unsigned int)ENTITY_REPORT_WORKER_STACK_SIZE,
                (unsigned int)ENTITY_REPORT_WORKER_PRIORITY);
    return 0;
}

void Entity_Event_Report_Send(cJSON *root, Mqtt_Qos_Type_e qos)
{
    Entity_Report_Work_Item_t item = {0};

    if (root == NULL)
    {
        ENTITY_LOGE("[REPORT_TRANSPORT] drop report: root is null");
        return;
    }

    item.json = cJSON_PrintUnformatted(root);
    if (item.json == NULL)
    {
        ENTITY_LOGE("[REPORT_TRANSPORT] drop report: json encode failed");
        return;
    }

    item.len = strlen(item.json);
    item.qos = qos;
    item.transport = Entity_Report_Transport_Get();

    if (!s_report_worker_ready)
    {
        if (Entity_Report_Worker_Init() != 0)
        {
            ENTITY_LOGE("[REPORT_TRANSPORT] drop report: worker not ready len=%u",
                        (unsigned int)item.len);
            Entity_Report_Work_Item_Free(&item);
            return;
        }
    }

    if (Entity_Msg_Queue_Send(&s_report_queue, &item, sizeof(item), 0) != 0)
    {
        ENTITY_LOGE("[REPORT_TRANSPORT] queue full, drop report len=%u transport=%d",
                    (unsigned int)item.len,
                    (int)item.transport);
        Entity_Report_Work_Item_Free(&item);
        return;
    }

    ENTITY_LOGD("[REPORT_TRANSPORT] report enqueued len=%u transport=%d qos=%d depth=%u/%u",
                (unsigned int)item.len,
                (int)item.transport,
                (int)item.qos,
                (unsigned int)Entity_Msg_Queue_Get_Msg_Num(&s_report_queue),
                (unsigned int)ENTITY_REPORT_QUEUE_LEN);
}
