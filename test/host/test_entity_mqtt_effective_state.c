#include "entity_mqtt_client.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static int s_failures;

static void expect_state(const char *name,
                         Entity_Mqtt_Process_State_e process_state,
                         bool ctx_connected,
                         bool app_connected,
                         bool broker_ok,
                         Mqtt_Client_Broker_Liveness_t broker_state,
                         bool stale_puback,
                         Entity_Mqtt_Effective_State_t expected_state,
                         bool expected_publish,
                         bool expected_drain,
                         bool expected_reconnect)
{
    Entity_Mqtt_Context_t ctx;
    Entity_Mqtt_Effective_Input_t input;

    memset(&ctx, 0, sizeof(ctx));
    memset(&input, 0, sizeof(input));

    ctx.Mqtt_Client = (void *)0x1;
    ctx.State = (uint8_t)process_state;
    ctx.Is_Connected = ctx_connected;

    input.app_connected = app_connected;
    input.broker_ok = broker_ok;
    input.broker_state = broker_state;
    input.stale_puback = stale_puback;

    Entity_Mqtt_Effective_State_t actual =
        Entity_Mqtt_Effective_State_From_Input(&ctx, &input);

    bool can_publish = Entity_Mqtt_Effective_Can_Publish(actual);
    bool can_drain = Entity_Mqtt_Effective_Can_Drain(actual);
    bool should_reconnect = Entity_Mqtt_Effective_Should_Reconnect(actual);

    if (actual != expected_state ||
        can_publish != expected_publish ||
        can_drain != expected_drain ||
        should_reconnect != expected_reconnect)
    {
        printf("FAIL %s: state=%s expected=%s publish=%d/%d drain=%d/%d reconnect=%d/%d\n",
               name,
               Entity_Mqtt_Effective_State_Str(actual),
               Entity_Mqtt_Effective_State_Str(expected_state),
               can_publish ? 1 : 0,
               expected_publish ? 1 : 0,
               can_drain ? 1 : 0,
               expected_drain ? 1 : 0,
               should_reconnect ? 1 : 0,
               expected_reconnect ? 1 : 0);
        s_failures++;
    }
}

int main(void)
{
    expect_state("idle disconnected",
                 ENTITY_MQTT_IDLE_STATE,
                 false,
                 false,
                 true,
                 MQTT_BROKER_LIVENESS_DISCONNECTED,
                 false,
                 ENTITY_MQTT_EFFECTIVE_DISCONNECTED,
                 false,
                 false,
                 true);

    expect_state("connecting does not reconnect again",
                 ENTITY_MQTT_CONNCET_STATE,
                 false,
                 false,
                 true,
                 MQTT_BROKER_LIVENESS_DISCONNECTED,
                 false,
                 ENTITY_MQTT_EFFECTIVE_CONNECTING,
                 false,
                 false,
                 false);

    expect_state("subscribing waits",
                 ENTITY_MQTT_SUBSCRIBING_STATE,
                 false,
                 false,
                 true,
                 MQTT_BROKER_LIVENESS_READY,
                 false,
                 ENTITY_MQTT_EFFECTIVE_SUBSCRIBING,
                 false,
                 false,
                 false);

    expect_state("ready publishes and drains",
                 ENTITY_MQTT_YIELD_STATE,
                 true,
                 true,
                 true,
                 MQTT_BROKER_LIVENESS_READY,
                 false,
                 ENTITY_MQTT_EFFECTIVE_READY,
                 true,
                 true,
                 false);

    expect_state("probing does not reconnect or drain",
                 ENTITY_MQTT_YIELD_STATE,
                 true,
                 true,
                 true,
                 MQTT_BROKER_LIVENESS_PROBING,
                 false,
                 ENTITY_MQTT_EFFECTIVE_PROBING,
                 false,
                 false,
                 false);

    expect_state("dead reconnects",
                 ENTITY_MQTT_YIELD_STATE,
                 true,
                 true,
                 true,
                 MQTT_BROKER_LIVENESS_DEAD,
                 false,
                 ENTITY_MQTT_EFFECTIVE_DEAD,
                 false,
                 false,
                 true);

    expect_state("broker disconnected reconnects",
                 ENTITY_MQTT_YIELD_STATE,
                 true,
                 true,
                 true,
                 MQTT_BROKER_LIVENESS_DISCONNECTED,
                 false,
                 ENTITY_MQTT_EFFECTIVE_DEAD,
                 false,
                 false,
                 true);

    expect_state("stale puback is dead",
                 ENTITY_MQTT_YIELD_STATE,
                 true,
                 true,
                 true,
                 MQTT_BROKER_LIVENESS_READY,
                 true,
                 ENTITY_MQTT_EFFECTIVE_DEAD,
                 false,
                 false,
                 true);

    expect_state("broker snapshot unavailable falls back to ready when app and ctx are ready",
                 ENTITY_MQTT_YIELD_STATE,
                 true,
                 true,
                 false,
                 MQTT_BROKER_LIVENESS_DISCONNECTED,
                 false,
                 ENTITY_MQTT_EFFECTIVE_READY,
                 true,
                 true,
                 false);

    if (s_failures != 0)
    {
        printf("test_entity_mqtt_effective_state failed: %d failures\n", s_failures);
        return 1;
    }

    printf("test_entity_mqtt_effective_state passed\n");
    return 0;
}
