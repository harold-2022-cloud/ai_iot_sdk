#include "ai_rtc_facade_backend.h"

#include <stdio.h>
#include <string.h>

#define AI_RTC_FACADE_MAX_TOKEN_LEN 512u
#define AI_RTC_FACADE_MAX_CHANNEL_LEN 128u
#define AI_RTC_FACADE_MAX_APP_ID_LEN 64u
#define AI_RTC_FACADE_MAX_USER_ACCOUNT_LEN 255u
#define AI_RTC_FACADE_MAX_CONTROL_PEER_ID_LEN 255u

#define AI_RTC_FACADE_CORE_LOG(...)      \
    do                                   \
    {                                    \
        printf("[AI_RTC_FACADE_CORE] "); \
        printf(__VA_ARGS__);             \
        printf("\n");                    \
    } while (0)

typedef struct
{
    bool initialized;
    bool joined;
    Ai_Rtc_Facade_State_t state;
    Ai_Rtc_Facade_Config_t config;
    Ai_Rtc_Facade_Callbacks_t callbacks;
    char rtc_token[AI_RTC_FACADE_MAX_TOKEN_LEN + 1u];
    char channel_name[AI_RTC_FACADE_MAX_CHANNEL_LEN + 1u];
    char app_id[AI_RTC_FACADE_MAX_APP_ID_LEN + 1u];
    char user_account[AI_RTC_FACADE_MAX_USER_ACCOUNT_LEN + 1u];
    char control_peer_id[AI_RTC_FACADE_MAX_CONTROL_PEER_ID_LEN + 1u];
    char control_token[AI_RTC_FACADE_MAX_TOKEN_LEN + 1u];
    uint32_t uid;
    bool enable_audio_ai_qos;
} Ai_Rtc_Facade_Context_t;

static Ai_Rtc_Facade_Context_t s_ctx;

static int copy_string(char *dst, size_t dst_size, const char *src)
{
    size_t len;

    if (dst == NULL || dst_size == 0u || src == NULL || src[0] == '\0')
    {
        return AI_RTC_FACADE_ERR_INVALID_ARG;
    }

    len = strlen(src);
    if (len >= dst_size)
    {
        return AI_RTC_FACADE_ERR_INVALID_ARG;
    }

    memcpy(dst, src, len + 1u);
    return AI_RTC_FACADE_OK;
}

static int copy_optional_string(char *dst, size_t dst_size, const char *src)
{
    size_t len;

    if (dst == NULL || dst_size == 0u)
    {
        return AI_RTC_FACADE_ERR_INVALID_ARG;
    }
    dst[0] = '\0';
    if (src == NULL || src[0] == '\0')
    {
        return AI_RTC_FACADE_OK;
    }

    len = strlen(src);
    if (len >= dst_size)
    {
        return AI_RTC_FACADE_ERR_INVALID_ARG;
    }

    memcpy(dst, src, len + 1u);
    return AI_RTC_FACADE_OK;
}

static const char *optional_config_string(const char *value)
{
    return (value != NULL && value[0] != '\0') ? value : NULL;
}

static void emit_event(Ai_Rtc_Facade_Event_t event, Ai_Rtc_Facade_State_t state, int detail)
{
    Ai_Rtc_Facade_State_Cb on_state = s_ctx.callbacks.on_state;
    void *user = s_ctx.callbacks.user;

    s_ctx.state = state;
    if (state != AI_RTC_FACADE_STATE_JOINED)
    {
        s_ctx.joined = false;
    }
    AI_RTC_FACADE_CORE_LOG("event=%d state=%d detail=%d joined=%d",
                           (int)event,
                           (int)state,
                           detail,
                           s_ctx.joined ? 1 : 0);
    if (on_state != NULL)
    {
        on_state(event, state, detail, user);
    }
}

int Ai_Rtc_Facade_Init(const Ai_Rtc_Facade_Config_t *config,
                       const Ai_Rtc_Facade_Callbacks_t *callbacks)
{
    memset(&s_ctx, 0, sizeof(s_ctx));
    s_ctx.state = AI_RTC_FACADE_STATE_IDLE;

    if (config != NULL)
    {
        s_ctx.config = *config;
    }
    if (callbacks != NULL)
    {
        s_ctx.callbacks = *callbacks;
    }

    s_ctx.initialized = true;
    return AI_RTC_FACADE_OK;
}

void Ai_Rtc_Facade_Deinit(void)
{
    if (s_ctx.initialized)
    {
        const Ai_Rtc_Facade_Backend_t *backend;

        (void)Ai_Rtc_Facade_Stop();
        backend = Ai_Rtc_Facade_Get_Backend();
        if (backend != NULL && backend->shutdown != NULL)
        {
            (void)backend->shutdown();
        }
    }
    memset(&s_ctx, 0, sizeof(s_ctx));
    s_ctx.state = AI_RTC_FACADE_STATE_IDLE;
}

int Ai_Rtc_Facade_On_Token_Result(const Ai_Rtc_Facade_Token_Result_t *result)
{
    Ai_Rtc_Facade_Backend_Start_Config_t backend_config;
    const Ai_Rtc_Facade_Backend_t *backend;
    int rc;

    if (!s_ctx.initialized || result == NULL)
    {
        return AI_RTC_FACADE_ERR_INVALID_ARG;
    }
    if (result->result != 0)
    {
        emit_event(AI_RTC_FACADE_EVENT_FAILED, AI_RTC_FACADE_STATE_FAILED, result->result);
        return AI_RTC_FACADE_ERR_INTERNAL;
    }
    if (s_ctx.state == AI_RTC_FACADE_STATE_JOINED ||
        s_ctx.state == AI_RTC_FACADE_STATE_RECONNECTING)
    {
        backend = Ai_Rtc_Facade_Get_Backend();
        if (backend == NULL || backend->renew_token == NULL ||
            copy_string(s_ctx.rtc_token, sizeof(s_ctx.rtc_token), result->rtc_token) != AI_RTC_FACADE_OK)
        {
            return AI_RTC_FACADE_ERR_INVALID_ARG;
        }
        return backend->renew_token(s_ctx.rtc_token);
    }
    if (s_ctx.state == AI_RTC_FACADE_STATE_STARTING ||
        s_ctx.state == AI_RTC_FACADE_STATE_JOINING)
    {
        return AI_RTC_FACADE_ERR_BUSY;
    }
    if (copy_string(s_ctx.rtc_token, sizeof(s_ctx.rtc_token), result->rtc_token) != AI_RTC_FACADE_OK ||
        copy_string(s_ctx.channel_name, sizeof(s_ctx.channel_name), result->channel_name) != AI_RTC_FACADE_OK ||
        copy_string(s_ctx.app_id, sizeof(s_ctx.app_id), result->app_id) != AI_RTC_FACADE_OK ||
        copy_optional_string(s_ctx.user_account, sizeof(s_ctx.user_account), result->user_account) != AI_RTC_FACADE_OK ||
        copy_optional_string(s_ctx.control_peer_id, sizeof(s_ctx.control_peer_id), result->control_peer_id) != AI_RTC_FACADE_OK ||
        copy_optional_string(s_ctx.control_token, sizeof(s_ctx.control_token), result->control_token) != AI_RTC_FACADE_OK)
    {
        return AI_RTC_FACADE_ERR_INVALID_ARG;
    }

    s_ctx.uid = (uint32_t)result->uid;
    s_ctx.enable_audio_ai_qos = result->enable_audio_ai_qos;
    emit_event(AI_RTC_FACADE_EVENT_STATE_CHANGED, AI_RTC_FACADE_STATE_TOKEN_READY, 0);

    backend = Ai_Rtc_Facade_Get_Backend();
    if (backend == NULL || backend->start == NULL)
    {
        emit_event(AI_RTC_FACADE_EVENT_FAILED, AI_RTC_FACADE_STATE_FAILED, AI_RTC_FACADE_ERR_INTERNAL);
        return AI_RTC_FACADE_ERR_INTERNAL;
    }

    backend_config.app_id = s_ctx.app_id;
    backend_config.channel_name = s_ctx.channel_name;
    backend_config.rtc_token = s_ctx.rtc_token;
    backend_config.uid = s_ctx.uid;
    backend_config.user_account = optional_config_string(s_ctx.user_account);
    backend_config.control_peer_id = optional_config_string(s_ctx.control_peer_id);
    backend_config.control_token = optional_config_string(s_ctx.control_token);
    backend_config.enable_audio_ai_qos = s_ctx.enable_audio_ai_qos;
    backend_config.enable_audio = s_ctx.config.enable_audio;
    backend_config.enable_video = s_ctx.config.enable_video;
    backend_config.join_timeout_ms = s_ctx.config.join_timeout_ms;

    emit_event(AI_RTC_FACADE_EVENT_STATE_CHANGED, AI_RTC_FACADE_STATE_STARTING, 0);
    emit_event(AI_RTC_FACADE_EVENT_STATE_CHANGED, AI_RTC_FACADE_STATE_JOINING, 0);
    rc = backend->start(&backend_config);
    if (rc != AI_RTC_FACADE_OK)
    {
        emit_event(AI_RTC_FACADE_EVENT_FAILED, AI_RTC_FACADE_STATE_FAILED, rc);
        return rc;
    }

    return AI_RTC_FACADE_OK;
}

int Ai_Rtc_Facade_Stop(void)
{
    const Ai_Rtc_Facade_Backend_t *backend;
    int rc = AI_RTC_FACADE_OK;

    if (!s_ctx.initialized)
    {
        return AI_RTC_FACADE_OK;
    }
    if (s_ctx.state == AI_RTC_FACADE_STATE_IDLE)
    {
        return AI_RTC_FACADE_OK;
    }
    if (s_ctx.state == AI_RTC_FACADE_STATE_STARTING ||
        s_ctx.state == AI_RTC_FACADE_STATE_JOINING ||
        s_ctx.state == AI_RTC_FACADE_STATE_STOPPING)
    {
        return AI_RTC_FACADE_ERR_BUSY;
    }

    emit_event(AI_RTC_FACADE_EVENT_STATE_CHANGED, AI_RTC_FACADE_STATE_STOPPING, 0);
    backend = Ai_Rtc_Facade_Get_Backend();
    if (backend != NULL && backend->stop != NULL)
    {
        rc = backend->stop();
    }
    emit_event(AI_RTC_FACADE_EVENT_STOPPED, AI_RTC_FACADE_STATE_IDLE, rc);
    return rc;
}

Ai_Rtc_Facade_State_t Ai_Rtc_Facade_Get_State(void)
{
    return s_ctx.state;
}

bool Ai_Rtc_Facade_Is_Joined(void)
{
    return s_ctx.joined;
}

int Ai_Rtc_Facade_Wait_Joined(uint32_t timeout_ms)
{
    const Ai_Rtc_Facade_Backend_t *backend;
    int rc;

    if (!s_ctx.initialized)
    {
        return AI_RTC_FACADE_ERR_NOT_READY;
    }
    if (s_ctx.joined)
    {
        return AI_RTC_FACADE_OK;
    }

    backend = Ai_Rtc_Facade_Get_Backend();
    if (backend == NULL || backend->wait_joined == NULL)
    {
        return AI_RTC_FACADE_ERR_NOT_READY;
    }

    rc = backend->wait_joined(timeout_ms);
    return (rc == AI_RTC_FACADE_OK && s_ctx.joined) ? AI_RTC_FACADE_OK : rc;
}

int Ai_Rtc_Facade_Send_Audio(const Ai_Rtc_Facade_Audio_Frame_t *frame)
{
    const Ai_Rtc_Facade_Backend_t *backend;

    if (!s_ctx.initialized || frame == NULL || frame->data == NULL || frame->len == 0u)
    {
        return AI_RTC_FACADE_ERR_INVALID_ARG;
    }
    if (!s_ctx.joined)
    {
        return AI_RTC_FACADE_ERR_NOT_READY;
    }

    backend = Ai_Rtc_Facade_Get_Backend();
    if (backend == NULL || backend->send_audio == NULL)
    {
        return AI_RTC_FACADE_ERR_INTERNAL;
    }
    return backend->send_audio(frame);
}

int Ai_Rtc_Facade_Send_Datastream(const uint8_t *data, size_t len)
{
    const Ai_Rtc_Facade_Backend_t *backend;

    if (!s_ctx.initialized || data == NULL || len == 0u)
    {
        return AI_RTC_FACADE_ERR_INVALID_ARG;
    }
    if (!s_ctx.joined)
    {
        return AI_RTC_FACADE_ERR_NOT_READY;
    }

    backend = Ai_Rtc_Facade_Get_Backend();
    if (backend == NULL || backend->send_datastream == NULL)
    {
        return AI_RTC_FACADE_ERR_INTERNAL;
    }
    return backend->send_datastream(data, len);
}

int Ai_Rtc_Facade_Send_Video(const Ai_Rtc_Facade_Video_Frame_t *frame)
{
    const Ai_Rtc_Facade_Backend_t *backend;

    if (!s_ctx.initialized || frame == NULL || frame->data == NULL || frame->len == 0u)
    {
        return AI_RTC_FACADE_ERR_INVALID_ARG;
    }
    if (!s_ctx.config.enable_video || !s_ctx.joined)
    {
        return AI_RTC_FACADE_ERR_NOT_READY;
    }

    backend = Ai_Rtc_Facade_Get_Backend();
    if (backend == NULL || backend->send_video == NULL)
    {
        return AI_RTC_FACADE_ERR_INTERNAL;
    }
    return backend->send_video(frame);
}

void Ai_Rtc_Facade_Backend_Notify_Joined(int detail)
{
    s_ctx.joined = true;
    emit_event(AI_RTC_FACADE_EVENT_JOINED, AI_RTC_FACADE_STATE_JOINED, detail);
}

void Ai_Rtc_Facade_Backend_Notify_Reconnecting(int detail)
{
    emit_event(AI_RTC_FACADE_EVENT_RECONNECTING, AI_RTC_FACADE_STATE_RECONNECTING, detail);
}

void Ai_Rtc_Facade_Backend_Notify_Rejoined(int detail)
{
    s_ctx.joined = true;
    emit_event(AI_RTC_FACADE_EVENT_REJOINED, AI_RTC_FACADE_STATE_JOINED, detail);
}

void Ai_Rtc_Facade_Backend_Notify_Remote_User_Joined(uint32_t uid)
{
    emit_event(AI_RTC_FACADE_EVENT_REMOTE_USER_JOINED, s_ctx.state, (int)uid);
}

void Ai_Rtc_Facade_Backend_Notify_Remote_User_Offline(uint32_t uid)
{
    emit_event(AI_RTC_FACADE_EVENT_REMOTE_USER_OFFLINE, s_ctx.state, (int)uid);
}

void Ai_Rtc_Facade_Backend_Notify_Token_Will_Expire(void)
{
    emit_event(AI_RTC_FACADE_EVENT_TOKEN_WILL_EXPIRE, s_ctx.state, 0);
}

void Ai_Rtc_Facade_Backend_Notify_Token_Expired(int detail)
{
    emit_event(AI_RTC_FACADE_EVENT_TOKEN_EXPIRED, AI_RTC_FACADE_STATE_FAILED, detail);
}

void Ai_Rtc_Facade_Backend_Notify_Failed(int detail)
{
    emit_event(AI_RTC_FACADE_EVENT_FAILED, AI_RTC_FACADE_STATE_FAILED, detail);
}

void Ai_Rtc_Facade_Backend_Notify_Audio_Rx(const Ai_Rtc_Facade_Audio_Frame_t *frame)
{
    const bool joined = s_ctx.joined;
    Ai_Rtc_Facade_Audio_Rx_Cb on_audio_rx = s_ctx.callbacks.on_audio_rx;
    void *user = s_ctx.callbacks.user;

    if (joined && frame != NULL && on_audio_rx != NULL)
    {
        (void)on_audio_rx(frame, user);
    }
}

void Ai_Rtc_Facade_Backend_Notify_Video_Rx(const Ai_Rtc_Facade_Video_Frame_t *frame)
{
    const bool joined = s_ctx.joined;
    Ai_Rtc_Facade_Video_Rx_Cb on_video_rx = s_ctx.callbacks.on_video_rx;
    void *user = s_ctx.callbacks.user;

    if (joined && frame != NULL && on_video_rx != NULL)
    {
        (void)on_video_rx(frame, user);
    }
}

void Ai_Rtc_Facade_Backend_Notify_Datastream_Rx(const Ai_Rtc_Facade_Datastream_Message_t *message)
{
    const bool joined = s_ctx.joined;
    Ai_Rtc_Facade_Datastream_Rx_Cb on_datastream_rx = s_ctx.callbacks.on_datastream_rx;
    void *user = s_ctx.callbacks.user;

    if (joined && message != NULL && on_datastream_rx != NULL)
    {
        (void)on_datastream_rx(message, user);
    }
}
