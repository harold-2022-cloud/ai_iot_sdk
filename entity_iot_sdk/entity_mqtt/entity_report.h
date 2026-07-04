#pragma once

#include "cJSON.h"
#include "entity_iot_cloud.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    ENTITY_REPORT_TRANSPORT_MQTT = 0,
    ENTITY_REPORT_TRANSPORT_HTTP = 1,
} Entity_Report_Transport_e;

void Entity_Report_Transport_Set(Entity_Report_Transport_e transport);
Entity_Report_Transport_e Entity_Report_Transport_Get(void);
int Entity_Report_Http_Endpoint_Set(const char *url);

int Entity_Report_Worker_Init(void);
void Entity_Event_Report_Send(cJSON *root, Mqtt_Qos_Type_e qos);

#ifdef __cplusplus
}
#endif
