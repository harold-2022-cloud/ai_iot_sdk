#include "ai_rtc_agora_control_transport.h"

#include "ai_rtc_facade.h"
#include "ai_rtc_facade_backend.h"

#if AI_RTC_AGORA_CONTROL_TRANSPORT == AI_RTC_AGORA_CONTROL_TRANSPORT_RTM

static Ai_Rtc_Agora_Control_State_t *s_rtm_state;

static bool non_empty_string(const char *value)
{
    return value != NULL && value[0] != '\0';
}

static void on_rtm_event(const char *rtm_uid, rtm_event_type_e event_type, rtm_err_code_e err_code)
{
    (void)rtm_uid;
    if (s_rtm_state == NULL)
    {
        return;
    }
    if (event_type == RTM_EVENT_TYPE_LOGIN && err_code == ERR_RTM_OK)
    {
        s_rtm_state->rtm_logged_in = true;
        return;
    }
    if (event_type == RTM_EVENT_TYPE_KICKOFF || event_type == RTM_EVENT_TYPE_EXIT)
    {
        s_rtm_state->rtm_logged_in = false;
    }
}

static void on_rtm_data(const char *rtm_uid, const void *msg, size_t msg_len, const char *custom_type)
{
    Ai_Rtc_Facade_Datastream_Message_t message;

    (void)rtm_uid;
    (void)custom_type;
    if (s_rtm_state == NULL || msg == NULL || msg_len == 0u)
    {
        return;
    }

    message.stream_id = -1;
    message.sender_uid = 0u;
    message.data = (const uint8_t *)msg;
    message.len = msg_len;
    message.sent_ts = 0u;
    Ai_Rtc_Facade_Backend_Notify_Datastream_Rx(&message);
}

static void on_rtm_send_data_result(const char *rtm_uid, uint32_t msg_id, rtm_msg_state_e state)
{
    (void)rtm_uid;
    (void)msg_id;
    (void)state;
}

void Ai_Rtc_Agora_Control_Reset(Ai_Rtc_Agora_Control_State_t *state)
{
    if (state == NULL)
    {
        return;
    }
    if (s_rtm_state == state)
    {
        s_rtm_state = NULL;
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
    const char *token;
    agora_rtm_handler_t handler;

    if (state == NULL || config == NULL ||
        !non_empty_string(config->user_account) ||
        !non_empty_string(config->control_peer_id) ||
        !non_empty_string(config->rtc_token))
    {
        return AI_RTC_FACADE_ERR_INVALID_ARG;
    }

    Ai_Rtc_Agora_Control_Reset(state);
    state->control_peer_id = config->control_peer_id;
    token = non_empty_string(config->control_token) ? config->control_token : config->rtc_token;

    handler.on_rtm_event = on_rtm_event;
    handler.on_rtm_data = on_rtm_data;
    handler.on_rtm_send_data_result = on_rtm_send_data_result;

    s_rtm_state = state;
    if (agora_rtc_login_rtm(config->user_account, token, &handler) < 0)
    {
        Ai_Rtc_Agora_Control_Reset(state);
        return AI_RTC_FACADE_ERR_INTERNAL;
    }
    state->rtm_login_started = true;
    return AI_RTC_FACADE_OK;
}

int Ai_Rtc_Agora_Control_On_Rtc_Joined(Ai_Rtc_Agora_Control_State_t *state,
                                       connection_id_t conn_id)
{
    (void)conn_id;
    return (state == NULL) ? AI_RTC_FACADE_ERR_INVALID_ARG : AI_RTC_FACADE_OK;
}

int Ai_Rtc_Agora_Control_Stop(Ai_Rtc_Agora_Control_State_t *state)
{
    int rc = AI_RTC_FACADE_OK;
    bool should_logout;

    if (state == NULL)
    {
        return AI_RTC_FACADE_OK;
    }

    should_logout = state->rtm_login_started || state->rtm_logged_in;
    if (should_logout && agora_rtc_logout_rtm() < 0)
    {
        rc = AI_RTC_FACADE_ERR_INTERNAL;
    }
    Ai_Rtc_Agora_Control_Reset(state);
    return rc;
}

bool Ai_Rtc_Agora_Control_Is_Ready(const Ai_Rtc_Agora_Control_State_t *state)
{
    return state != NULL && state->rtm_logged_in && non_empty_string(state->control_peer_id);
}

int Ai_Rtc_Agora_Control_Send(Ai_Rtc_Agora_Control_State_t *state,
                              connection_id_t conn_id,
                              const uint8_t *data,
                              size_t len)
{
    (void)conn_id;
    if (!Ai_Rtc_Agora_Control_Is_Ready(state) || data == NULL || len == 0u)
    {
        return AI_RTC_FACADE_ERR_NOT_READY;
    }

    state->next_rtm_msg_id++;
    return (agora_rtc_send_rtm_data(state->control_peer_id, data, len, state->next_rtm_msg_id, NULL) < 0)
        ? AI_RTC_FACADE_ERR_INTERNAL
        : AI_RTC_FACADE_OK;
}

#endif
