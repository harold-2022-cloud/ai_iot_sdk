#include "ai_rtc_agora_port.h"
#include "agora_rtc_api.h"

#include <stdbool.h>

typedef struct
{
    int sleep_calls;
    uint32_t now_ms;
    int join_after_sleeps;
    bool joined_emitted;
} Ai_Rtc_Agora_Port_Stub_State_t;

static Ai_Rtc_Agora_Port_Stub_State_t s_port_stub;

void Ai_Rtc_Agora_Port_Stub_Reset(void)
{
    s_port_stub.sleep_calls = 0;
    s_port_stub.now_ms = 0;
    s_port_stub.join_after_sleeps = 0;
    s_port_stub.joined_emitted = false;
}

void Ai_Rtc_Agora_Port_Stub_Join_After_Sleeps(int count)
{
    s_port_stub.join_after_sleeps = count;
    s_port_stub.joined_emitted = false;
}

int Ai_Rtc_Agora_Port_Stub_Sleep_Calls(void)
{
    return s_port_stub.sleep_calls;
}

static int stub_network_register(Ai_Rtc_Agora_Port_Network_Cb cb, void *user)
{
    (void)cb;
    (void)user;
    return 0;
}

static void stub_network_unregister(void)
{
}

static int stub_network_refresh(void)
{
    return 0;
}

static void stub_sleep_ms(uint32_t ms)
{
    s_port_stub.sleep_calls++;
    s_port_stub.now_ms += ms;
    if (!s_port_stub.joined_emitted &&
        s_port_stub.join_after_sleeps > 0 &&
        s_port_stub.sleep_calls >= s_port_stub.join_after_sleeps)
    {
        s_port_stub.joined_emitted = true;
        Agora_Rtc_Stub_Emit_Joined();
    }
}

static uint32_t stub_timestamp_ms(void)
{
    return s_port_stub.now_ms;
}

static const Ai_Rtc_Agora_Port_t s_stub_port = {
    .sleep_ms = stub_sleep_ms,
    .timestamp_ms = stub_timestamp_ms,
    .network_register = stub_network_register,
    .network_unregister = stub_network_unregister,
    .network_refresh = stub_network_refresh,
};

const Ai_Rtc_Agora_Port_t *Ai_Rtc_Agora_Port_Get(void)
{
    return &s_stub_port;
}
