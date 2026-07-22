#include "ai_rtc_agora_control_transport.h"

#include "ai_rtc_facade.h"

#if AI_RTC_AGORA_CONTROL_TRANSPORT == AI_RTC_AGORA_CONTROL_TRANSPORT_DATASTREAM

static bool has_text(const char *value)
{
    return value != NULL && value[0] != '\0';
}

void Ai_Rtc_Agora_Control_Reset(Ai_Rtc_Agora_Control_State_t *state)
{
    if (state == NULL)
    {
        return;
    }
    state->stream_id = -1;
    state->rtm_logged_in = false;
    state->rtm_login_started = false;
    state->next_rtm_msg_id = 0u;
    state->control_peer_id = NULL;
}

int Ai_Rtc_Agora_Control_Start(Ai_Rtc_Agora_Control_State_t *state,
                               const Ai_Rtc_Agora_Control_Config_t *config)
{
    if (state == NULL || config == NULL || config->conn_id == CONNECTION_ID_INVALID)
    {
        return AI_RTC_FACADE_ERR_INVALID_ARG;
    }
    Ai_Rtc_Agora_Control_Reset(state);
    state->control_peer_id = config->control_peer_id;
    return AI_RTC_FACADE_OK;
}

int Ai_Rtc_Agora_Control_On_Rtc_Joined(Ai_Rtc_Agora_Control_State_t *state,
                                       connection_id_t conn_id)
{
    if (state == NULL || conn_id == CONNECTION_ID_INVALID)
    {
        return AI_RTC_FACADE_ERR_INVALID_ARG;
    }
    if (!has_text(state->control_peer_id))
    {
        return AI_RTC_FACADE_OK;
    }
    if (state->stream_id < 0 &&
        agora_rtc_create_data_stream(conn_id, &state->stream_id, true, true) < 0)
    {
        state->stream_id = -1;
        return AI_RTC_FACADE_ERR_INTERNAL;
    }
    return AI_RTC_FACADE_OK;
}

int Ai_Rtc_Agora_Control_Stop(Ai_Rtc_Agora_Control_State_t *state)
{
    Ai_Rtc_Agora_Control_Reset(state);
    return AI_RTC_FACADE_OK;
}

bool Ai_Rtc_Agora_Control_Is_Ready(const Ai_Rtc_Agora_Control_State_t *state)
{
    return state != NULL && state->stream_id >= 0;
}

int Ai_Rtc_Agora_Control_Send(Ai_Rtc_Agora_Control_State_t *state,
                              connection_id_t conn_id,
                              const uint8_t *data,
                              size_t len)
{
    if (!Ai_Rtc_Agora_Control_Is_Ready(state) ||
        conn_id == CONNECTION_ID_INVALID ||
        data == NULL ||
        len == 0u)
    {
        return AI_RTC_FACADE_ERR_NOT_READY;
    }

    return (agora_rtc_send_stream_message(conn_id, state->stream_id, (const char *)data, len) < 0)
        ? AI_RTC_FACADE_ERR_INTERNAL
        : AI_RTC_FACADE_OK;
}

#endif
