#include "entity_mqtt_client.h"

const char *Entity_Mqtt_Effective_State_Str(Entity_Mqtt_Effective_State_t state)
{
    switch (state)
    {
        case ENTITY_MQTT_EFFECTIVE_DISCONNECTED: return "DISCONNECTED";
        case ENTITY_MQTT_EFFECTIVE_CONNECTING:   return "CONNECTING";
        case ENTITY_MQTT_EFFECTIVE_SUBSCRIBING:  return "SUBSCRIBING";
        case ENTITY_MQTT_EFFECTIVE_READY:        return "READY";
        case ENTITY_MQTT_EFFECTIVE_PROBING:      return "PROBING";
        case ENTITY_MQTT_EFFECTIVE_DEAD:         return "DEAD";
        default:                                 return "UNKNOWN";
    }
}

Entity_Mqtt_Effective_State_t Entity_Mqtt_Effective_State_From_Input(const Entity_Mqtt_Context_t *context,
                                                                      const Entity_Mqtt_Effective_Input_t *input)
{
    if (context == NULL || context->Mqtt_Client == NULL)
    {
        return ENTITY_MQTT_EFFECTIVE_DISCONNECTED;
    }

    switch ((Entity_Mqtt_Process_State_e)context->State)
    {
        case ENTITY_MQTT_IDLE_STATE:
            return ENTITY_MQTT_EFFECTIVE_DISCONNECTED;

        case ENTITY_MQTT_CONNCET_STATE:
        case ENTITY_MQTT_RECONNECT_STATE:
            return ENTITY_MQTT_EFFECTIVE_CONNECTING;

        case ENTITY_MQTT_SUBSCRIBING_STATE:
        case ENTITY_MQTT_SUBSCRIBE_COMPLETE_STATE:
            return ENTITY_MQTT_EFFECTIVE_SUBSCRIBING;

        case ENTITY_MQTT_YIELD_STATE:
        default:
            break;
    }

    if (!context->Is_Connected)
    {
        return ENTITY_MQTT_EFFECTIVE_DISCONNECTED;
    }

    if (input == NULL || !input->app_connected)
    {
        return ENTITY_MQTT_EFFECTIVE_DISCONNECTED;
    }

    if (input->stale_puback)
    {
        return ENTITY_MQTT_EFFECTIVE_DEAD;
    }

    if (!input->broker_ok)
    {
        return ENTITY_MQTT_EFFECTIVE_READY;
    }

    switch (input->broker_state)
    {
        case MQTT_BROKER_LIVENESS_READY:
            return ENTITY_MQTT_EFFECTIVE_READY;

        case MQTT_BROKER_LIVENESS_PROBING:
            return ENTITY_MQTT_EFFECTIVE_PROBING;

        case MQTT_BROKER_LIVENESS_DEAD:
        case MQTT_BROKER_LIVENESS_DISCONNECTED:
            return ENTITY_MQTT_EFFECTIVE_DEAD;

        case MQTT_BROKER_LIVENESS_CONNECTING:
            return ENTITY_MQTT_EFFECTIVE_CONNECTING;

        case MQTT_BROKER_LIVENESS_SUBSCRIBING:
            return ENTITY_MQTT_EFFECTIVE_SUBSCRIBING;

        default:
            return ENTITY_MQTT_EFFECTIVE_DEAD;
    }
}

bool Entity_Mqtt_Effective_Can_Publish(Entity_Mqtt_Effective_State_t state)
{
    return state == ENTITY_MQTT_EFFECTIVE_READY;
}

bool Entity_Mqtt_Effective_Can_Drain(Entity_Mqtt_Effective_State_t state)
{
    return state == ENTITY_MQTT_EFFECTIVE_READY;
}

bool Entity_Mqtt_Effective_Should_Reconnect(Entity_Mqtt_Effective_State_t state)
{
    return state == ENTITY_MQTT_EFFECTIVE_DISCONNECTED ||
           state == ENTITY_MQTT_EFFECTIVE_DEAD;
}
