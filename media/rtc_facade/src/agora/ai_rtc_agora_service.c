#include "ai_rtc_agora_service.h"

#include "ai_rtc_agora_control_transport.h"
#include "ai_rtc_agora_datastream.h"
#include "ai_rtc_agora_port.h"
#include "agora_rtc_api.h"

#include <stdio.h>
#include <string.h>

#define AI_RTC_AGORA_SERVICE_MAX_APP_ID_LEN 64u
#define AI_RTC_AGORA_SERVICE_PCM_DURATION_MS 20
#define AI_RTC_AGORA_JOIN_WAIT_POLL_MS 10u

#define AI_RTC_AGORA_LOG(...)      \
    do                             \
    {                              \
        printf("[AI_RTC_AGORA] "); \
        printf(__VA_ARGS__);       \
        printf("\n");              \
    } while (0)

typedef enum
{
    AI_RTC_AGORA_SESSION_IDLE = 0,
    AI_RTC_AGORA_SESSION_JOINING,
    AI_RTC_AGORA_SESSION_JOINED,
    AI_RTC_AGORA_SESSION_RECONNECTING,
    AI_RTC_AGORA_SESSION_LEAVING,
    AI_RTC_AGORA_SESSION_FAILED,
} Ai_Rtc_Agora_Session_Phase_t;

typedef struct
{
    bool sdk_initialized;
    volatile bool session_active;
    volatile bool joined;
    volatile bool remote_user_joined;
    connection_id_t conn_id;
    Ai_Rtc_Agora_Session_Phase_t phase;
    Ai_Rtc_Agora_Control_State_t control_state;
    uint32_t generation;
    uint32_t stale_callbacks;
    uint32_t audio_not_ready_logs;
    uint32_t datastream_not_ready_logs;
    char app_id[AI_RTC_AGORA_SERVICE_MAX_APP_ID_LEN + 1u];
    const Ai_Rtc_Agora_Port_t *port;
} Ai_Rtc_Agora_Service_Context_t;

static Ai_Rtc_Agora_Service_Context_t s_agora = {
    .conn_id = CONNECTION_ID_INVALID,
    .phase = AI_RTC_AGORA_SESSION_IDLE,
    .control_state.stream_id = -1,
};

#if defined(__GNUC__)
__attribute__((weak)) const Ai_Rtc_Agora_Port_t *Ai_Rtc_Agora_Port_Get(void)
{
    return NULL;
}
#endif

static Ai_Rtc_Facade_Audio_Format_t map_agora_audio_format(const audio_frame_info_t *info)
{
    if (info == NULL)
    {
        return AI_RTC_FACADE_AUDIO_FORMAT_PCM16;
    }
    switch (info->data_type)
    {
        case AUDIO_DATA_TYPE_G722:
            return AI_RTC_FACADE_AUDIO_FORMAT_G722;
        case AUDIO_DATA_TYPE_OPUS:
            return AI_RTC_FACADE_AUDIO_FORMAT_OPUS;
        case AUDIO_DATA_TYPE_PCM:
        default:
            return AI_RTC_FACADE_AUDIO_FORMAT_PCM16;
    }
}

static audio_data_type_e map_facade_audio_format(Ai_Rtc_Facade_Audio_Format_t format)
{
    switch (format)
    {
        case AI_RTC_FACADE_AUDIO_FORMAT_G722:
            return AUDIO_DATA_TYPE_G722;
        case AI_RTC_FACADE_AUDIO_FORMAT_OPUS:
            return AUDIO_DATA_TYPE_OPUS;
        case AI_RTC_FACADE_AUDIO_FORMAT_PCM16:
        default:
            return AUDIO_DATA_TYPE_PCM;
    }
}

static video_data_type_e map_facade_video_format(Ai_Rtc_Facade_Video_Format_t format)
{
    switch (format)
    {
        case AI_RTC_FACADE_VIDEO_FORMAT_JPEG:
            return VIDEO_DATA_TYPE_GENERIC_JPEG;
        case AI_RTC_FACADE_VIDEO_FORMAT_H264:
        default:
            return VIDEO_DATA_TYPE_H264;
    }
}

static Ai_Rtc_Facade_Video_Format_t map_agora_video_format(const video_frame_info_t *info)
{
    if (info != NULL && info->data_type == VIDEO_DATA_TYPE_GENERIC_JPEG)
    {
        return AI_RTC_FACADE_VIDEO_FORMAT_JPEG;
    }
    return AI_RTC_FACADE_VIDEO_FORMAT_H264;
}

static video_frame_type_e map_facade_video_frame_type(uint32_t flags)
{
    return ((flags & AI_RTC_FACADE_VIDEO_FLAG_KEYFRAME) != 0u)
               ? VIDEO_FRAME_KEY
               : VIDEO_FRAME_DELTA;
}

static uint32_t map_agora_video_flags(const video_frame_info_t *info)
{
    if (info != NULL && info->frame_type == VIDEO_FRAME_KEY)
    {
        return AI_RTC_FACADE_VIDEO_FLAG_KEYFRAME;
    }
    return 0u;
}

static network_event_e map_port_network_event(Ai_Rtc_Agora_Port_Network_Event_t event)
{
    switch (event)
    {
        case AI_RTC_AGORA_PORT_NETWORK_UP:
            return NETWORK_EVENT_UP;
        case AI_RTC_AGORA_PORT_NETWORK_CHANGE:
            return NETWORK_EVENT_CHANGE;
        case AI_RTC_AGORA_PORT_NETWORK_DOWN:
        default:
            return NETWORK_EVENT_DOWN;
    }
}

static bool service_has_text(const char *value)
{
    return value != NULL && value[0] != '\0';
}

static bool service_should_log_not_ready(uint32_t *counter)
{
    uint32_t value;

    if (counter == NULL)
    {
        return false;
    }
    value = *counter;
    *counter = value + 1u;
    return value < 8u || (value % 100u) == 0u;
}

static void on_port_network_event(Ai_Rtc_Agora_Port_Network_Event_t event, void *user)
{
    (void)user;
    if (s_agora.sdk_initialized)
    {
        (void)agora_rtc_notify_network_event(map_port_network_event(event));
    }
}

static bool service_accept_callback(connection_id_t conn_id)
{
    if (s_agora.session_active && s_agora.conn_id == conn_id)
    {
        return true;
    }
    s_agora.stale_callbacks++;
    AI_RTC_AGORA_LOG("callback stale conn=%d current=%d active=%d stale=%lu",
                     (int)conn_id,
                     (int)s_agora.conn_id,
                     s_agora.session_active ? 1 : 0,
                     (unsigned long)s_agora.stale_callbacks);
    return false;
}

static void service_unregister_network(void)
{
    if (s_agora.port != NULL && s_agora.port->network_unregister != NULL)
    {
        s_agora.port->network_unregister();
    }
    s_agora.port = NULL;
}

static int service_register_network(void)
{
    s_agora.port = Ai_Rtc_Agora_Port_Get();
    if (s_agora.port == NULL)
    {
        return AI_RTC_FACADE_OK;
    }
    if (s_agora.port->network_register != NULL)
    {
        int rc = s_agora.port->network_register(on_port_network_event, NULL);
        if (rc != 0)
        {
            return AI_RTC_FACADE_ERR_INTERNAL;
        }
    }
    if (s_agora.port->network_refresh != NULL)
    {
        (void)s_agora.port->network_refresh();
    }
    return AI_RTC_FACADE_OK;
}

static void service_reset_session_state(void)
{
    s_agora.session_active = false;
    s_agora.joined = false;
    s_agora.remote_user_joined = false;
    s_agora.conn_id = CONNECTION_ID_INVALID;
    s_agora.phase = AI_RTC_AGORA_SESSION_IDLE;
    s_agora.stale_callbacks = 0u;
    s_agora.audio_not_ready_logs = 0u;
    s_agora.datastream_not_ready_logs = 0u;
    (void)Ai_Rtc_Agora_Control_Stop(&s_agora.control_state);
    Ai_Rtc_Agora_Datastream_Reset();
}

static int service_stop_session(void)
{
    int rc = AI_RTC_FACADE_OK;
    connection_id_t conn_id = s_agora.conn_id;

    if (s_agora.phase == AI_RTC_AGORA_SESSION_JOINING)
    {
        AI_RTC_AGORA_LOG("stop busy phase=JOINING conn=%d gen=%lu",
                         (int)s_agora.conn_id,
                         (unsigned long)s_agora.generation);
        return AI_RTC_FACADE_ERR_BUSY;
    }

    s_agora.session_active = false;
    s_agora.joined = false;
    s_agora.remote_user_joined = false;
    if (conn_id != CONNECTION_ID_INVALID)
    {
        s_agora.phase = AI_RTC_AGORA_SESSION_LEAVING;
        AI_RTC_AGORA_LOG("leave conn=%d gen=%lu", (int)conn_id, (unsigned long)s_agora.generation);
        if (agora_rtc_leave_channel(conn_id) < 0)
        {
            rc = AI_RTC_FACADE_ERR_INTERNAL;
        }
        AI_RTC_AGORA_LOG("destroy_conn conn=%d gen=%lu", (int)conn_id, (unsigned long)s_agora.generation);
        if (agora_rtc_destroy_connection(conn_id) < 0)
        {
            rc = AI_RTC_FACADE_ERR_INTERNAL;
        }
    }

    s_agora.conn_id = CONNECTION_ID_INVALID;
    s_agora.phase = AI_RTC_AGORA_SESSION_IDLE;
    (void)Ai_Rtc_Agora_Control_Stop(&s_agora.control_state);
    Ai_Rtc_Agora_Datastream_Reset();
    return rc;
}

static int service_shutdown_sdk(void)
{
    int rc = AI_RTC_FACADE_OK;

    service_unregister_network();
    if (s_agora.sdk_initialized && agora_rtc_fini() < 0)
    {
        rc = AI_RTC_FACADE_ERR_INTERNAL;
    }
    s_agora.sdk_initialized = false;
    s_agora.app_id[0] = '\0';
    return rc;
}

static void on_error(connection_id_t conn_id, int code, const char *msg)
{
    (void)msg;
    if (!service_accept_callback(conn_id))
    {
        return;
    }
    AI_RTC_AGORA_LOG("callback error conn=%d code=%d", (int)conn_id, code);
    s_agora.phase = AI_RTC_AGORA_SESSION_FAILED;
    Ai_Rtc_Facade_Backend_Notify_Failed(code);
}

static void on_join_channel_success(connection_id_t conn_id, uint32_t uid, int elapsed_ms)
{
    (void)uid;
    if (!service_accept_callback(conn_id))
    {
        return;
    }
    s_agora.conn_id = conn_id;
    s_agora.joined = true;
    s_agora.phase = AI_RTC_AGORA_SESSION_JOINED;
    AI_RTC_AGORA_LOG("callback joined conn=%d uid=%lu elapsed_ms=%d gen=%lu",
                     (int)conn_id,
                     (unsigned long)uid,
                     elapsed_ms,
                     (unsigned long)s_agora.generation);
    (void)Ai_Rtc_Agora_Control_On_Rtc_Joined(&s_agora.control_state, conn_id);
    Ai_Rtc_Facade_Backend_Notify_Joined(elapsed_ms);
}

static void on_reconnecting(connection_id_t conn_id)
{
    if (!service_accept_callback(conn_id))
    {
        return;
    }
    s_agora.joined = false;
    s_agora.remote_user_joined = false;
    s_agora.phase = AI_RTC_AGORA_SESSION_RECONNECTING;
    AI_RTC_AGORA_LOG("callback reconnecting conn=%d gen=%lu",
                     (int)conn_id,
                     (unsigned long)s_agora.generation);
    Ai_Rtc_Facade_Backend_Notify_Reconnecting(0);
}

static void on_connection_lost(connection_id_t conn_id)
{
    if (!service_accept_callback(conn_id))
    {
        return;
    }
    s_agora.joined = false;
    s_agora.remote_user_joined = false;
    s_agora.phase = AI_RTC_AGORA_SESSION_FAILED;
    AI_RTC_AGORA_LOG("callback connection_lost conn=%d gen=%lu",
                     (int)conn_id,
                     (unsigned long)s_agora.generation);
    Ai_Rtc_Facade_Backend_Notify_Failed(ERR_NET_DOWN);
}

static void on_rejoin_channel_success(connection_id_t conn_id, uint32_t uid, int elapsed_ms)
{
    (void)uid;
    if (!service_accept_callback(conn_id))
    {
        return;
    }
    s_agora.conn_id = conn_id;
    s_agora.joined = true;
    s_agora.remote_user_joined = false;
    s_agora.phase = AI_RTC_AGORA_SESSION_JOINED;
    AI_RTC_AGORA_LOG("callback rejoined conn=%d uid=%lu elapsed_ms=%d gen=%lu",
                     (int)conn_id,
                     (unsigned long)uid,
                     elapsed_ms,
                     (unsigned long)s_agora.generation);
    (void)Ai_Rtc_Agora_Control_On_Rtc_Joined(&s_agora.control_state, conn_id);
    Ai_Rtc_Facade_Backend_Notify_Rejoined(elapsed_ms);
}

static void on_token_privilege_will_expire(connection_id_t conn_id, const char *token)
{
    (void)token;
    if (!service_accept_callback(conn_id))
    {
        return;
    }
    AI_RTC_AGORA_LOG("callback token_will_expire conn=%d gen=%lu",
                     (int)conn_id,
                     (unsigned long)s_agora.generation);
    Ai_Rtc_Facade_Backend_Notify_Token_Will_Expire();
}

static void on_user_joined(connection_id_t conn_id, uint32_t uid, int elapsed_ms)
{
    (void)elapsed_ms;
    if (!service_accept_callback(conn_id))
    {
        return;
    }
    s_agora.remote_user_joined = true;
    AI_RTC_AGORA_LOG("callback remote_user_joined conn=%d uid=%lu gen=%lu",
                     (int)conn_id,
                     (unsigned long)uid,
                     (unsigned long)s_agora.generation);
    Ai_Rtc_Facade_Backend_Notify_Remote_User_Joined(uid);
}

static void on_user_offline(connection_id_t conn_id, uint32_t uid, int reason)
{
    (void)reason;
    if (!service_accept_callback(conn_id))
    {
        return;
    }
    s_agora.remote_user_joined = false;
    AI_RTC_AGORA_LOG("callback remote_user_offline conn=%d uid=%lu reason=%d gen=%lu",
                     (int)conn_id,
                     (unsigned long)uid,
                     reason,
                     (unsigned long)s_agora.generation);
    Ai_Rtc_Facade_Backend_Notify_Remote_User_Offline(uid);
}

static void on_audio_data(connection_id_t conn_id,
                          uint32_t uid,
                          uint16_t sent_ts,
                          const void *data_ptr,
                          size_t data_len,
                          const audio_frame_info_t *info_ptr)
{
    Ai_Rtc_Facade_Audio_Frame_t frame;

    (void)uid;
    if (!service_accept_callback(conn_id))
    {
        return;
    }
    memset(&frame, 0, sizeof(frame));
    frame.data = (const uint8_t *)data_ptr;
    frame.len = data_len;
    frame.format = map_agora_audio_format(info_ptr);
    frame.timestamp_ms = sent_ts;
    Ai_Rtc_Facade_Backend_Notify_Audio_Rx(&frame);
}

static void on_video_data(connection_id_t conn_id,
                          uint32_t uid,
                          uint16_t sent_ts,
                          const void *data_ptr,
                          size_t data_len,
                          const video_frame_info_t *info_ptr)
{
    Ai_Rtc_Facade_Video_Frame_t frame;

    (void)uid;
    if (!service_accept_callback(conn_id) || !s_agora.joined)
    {
        return;
    }
    memset(&frame, 0, sizeof(frame));
    frame.data = (const uint8_t *)data_ptr;
    frame.len = data_len;
    frame.format = map_agora_video_format(info_ptr);
    frame.frame_rate_hz = (info_ptr != NULL) ? info_ptr->frame_rate : 0u;
    frame.timestamp_ms = sent_ts;
    frame.flags = map_agora_video_flags(info_ptr);
    Ai_Rtc_Facade_Backend_Notify_Video_Rx(&frame);
}

static void on_target_bitrate_changed(connection_id_t conn_id, uint32_t target_bps)
{
    (void)target_bps;
    if (!service_accept_callback(conn_id))
    {
        return;
    }
}

static void on_key_frame_gen_req(connection_id_t conn_id, uint32_t uid, video_stream_type_e stream_type)
{
    (void)uid;
    (void)stream_type;
    if (!service_accept_callback(conn_id))
    {
        return;
    }
}

static void on_stream_message(connection_id_t conn_id,
                              uint32_t uid,
                              int stream_id,
                              const char *data,
                              size_t length,
                              uint64_t sent_ts)
{
    Ai_Rtc_Facade_Datastream_Message_t message;

    if (!service_accept_callback(conn_id))
    {
        return;
    }
    memset(&message, 0, sizeof(message));
    message.stream_id = stream_id;
    message.sender_uid = uid;
    message.data = (const uint8_t *)data;
    message.len = length;
    message.sent_ts = sent_ts;
    Ai_Rtc_Facade_Backend_Notify_Datastream_Rx(&message);
}

static agora_rtc_event_handler_t make_event_handler(void)
{
    agora_rtc_event_handler_t handler;

    memset(&handler, 0, sizeof(handler));
    handler.on_error = on_error;
    handler.on_join_channel_success = on_join_channel_success;
    handler.on_reconnecting = on_reconnecting;
    handler.on_connection_lost = on_connection_lost;
    handler.on_rejoin_channel_success = on_rejoin_channel_success;
    handler.on_token_privilege_will_expire = on_token_privilege_will_expire;
    handler.on_user_joined = on_user_joined;
    handler.on_user_offline = on_user_offline;
    handler.on_audio_data = on_audio_data;
    handler.on_video_data = on_video_data;
    handler.on_target_bitrate_changed = on_target_bitrate_changed;
    handler.on_key_frame_gen_req = on_key_frame_gen_req;
    handler.on_stream_message = on_stream_message;
    return handler;
}

int Ai_Rtc_Agora_Service_Start(const Ai_Rtc_Facade_Backend_Start_Config_t *config)
{
    rtc_service_option_t service_option;
    rtc_channel_options_t channel_options;
    agora_rtc_event_handler_t handler;
    size_t app_id_len;
    int rc;

    if (config == NULL || config->app_id == NULL || config->channel_name == NULL || config->rtc_token == NULL)
    {
        return AI_RTC_FACADE_ERR_INVALID_ARG;
    }
    app_id_len = strlen(config->app_id);
    if (app_id_len == 0u || app_id_len > AI_RTC_AGORA_SERVICE_MAX_APP_ID_LEN)
    {
        return AI_RTC_FACADE_ERR_INVALID_ARG;
    }

    memset(&service_option, 0, sizeof(service_option));
    memset(&channel_options, 0, sizeof(channel_options));
    handler = make_event_handler();

    service_option.area_code = AREA_CODE_GLOB;
    service_option.log_cfg.log_disable = true;
    service_option.log_cfg.log_level = RTC_LOG_WARNING;
    service_option.use_string_uid = service_has_text(config->user_account);

    if (s_agora.sdk_initialized && strcmp(s_agora.app_id, config->app_id) != 0)
    {
        AI_RTC_AGORA_LOG("app_id changed; shutdown existing sdk");
        (void)Ai_Rtc_Agora_Service_Shutdown();
    }

    if (!s_agora.sdk_initialized)
    {
        AI_RTC_AGORA_LOG("sdk init app_id_len=%lu use_string_uid=%d",
                         (unsigned long)app_id_len,
                         service_option.use_string_uid ? 1 : 0);
        rc = agora_rtc_init(config->app_id, &handler, &service_option);
        if (rc < 0)
        {
            AI_RTC_AGORA_LOG("sdk init failed rc=%d", rc);
            return AI_RTC_FACADE_ERR_INTERNAL;
        }
        s_agora.sdk_initialized = true;
        memcpy(s_agora.app_id, config->app_id, app_id_len + 1u);

        rc = service_register_network();
        if (rc != AI_RTC_FACADE_OK)
        {
            AI_RTC_AGORA_LOG("network register failed rc=%d", rc);
            (void)Ai_Rtc_Agora_Service_Shutdown();
            return rc;
        }
    }

    s_agora.session_active = false;
    s_agora.joined = false;
    s_agora.remote_user_joined = false;
    s_agora.conn_id = CONNECTION_ID_INVALID;
    s_agora.phase = AI_RTC_AGORA_SESSION_IDLE;
    s_agora.stale_callbacks = 0u;
    s_agora.audio_not_ready_logs = 0u;
    s_agora.datastream_not_ready_logs = 0u;
    Ai_Rtc_Agora_Control_Reset(&s_agora.control_state);

    rc = agora_rtc_create_connection(&s_agora.conn_id);
    if (rc < 0)
    {
        AI_RTC_AGORA_LOG("create_conn failed rc=%d", rc);
        (void)service_shutdown_sdk();
        return AI_RTC_FACADE_ERR_INTERNAL;
    }
    s_agora.session_active = true;
    s_agora.phase = AI_RTC_AGORA_SESSION_JOINING;
    s_agora.generation++;
    AI_RTC_AGORA_LOG("create_conn ok conn=%d gen=%lu",
                     (int)s_agora.conn_id,
                     (unsigned long)s_agora.generation);
    {
        const Ai_Rtc_Agora_Control_Config_t control_config = {
            .conn_id = s_agora.conn_id,
            .rtc_token = config->rtc_token,
            .user_account = config->user_account,
            .control_peer_id = config->control_peer_id,
            .control_token = config->control_token,
        };

        rc = Ai_Rtc_Agora_Control_Start(&s_agora.control_state, &control_config);
        if (rc != AI_RTC_FACADE_OK)
        {
            connection_id_t failed_conn_id = s_agora.conn_id;

            AI_RTC_AGORA_LOG("control start failed conn=%d rc=%d gen=%lu",
                             (int)failed_conn_id,
                             rc,
                             (unsigned long)s_agora.generation);
            s_agora.session_active = false;
            s_agora.conn_id = CONNECTION_ID_INVALID;
            s_agora.phase = AI_RTC_AGORA_SESSION_FAILED;
            AI_RTC_AGORA_LOG("destroy_conn conn=%d gen=%lu", (int)failed_conn_id, (unsigned long)s_agora.generation);
            (void)agora_rtc_destroy_connection(failed_conn_id);
            (void)service_shutdown_sdk();
            return rc;
        }
    }

    channel_options.auto_subscribe_audio = config->enable_audio;
    channel_options.auto_subscribe_video = config->enable_video;
    channel_options.enable_audio_downlink_aec = false;
    channel_options.enable_audio_ai_qos = config->enable_audio_ai_qos;
    channel_options.enable_audio_decode = false;
    channel_options.enable_audio_jitter_buffer = config->enable_audio;
    channel_options.audio_codec_opt.audio_codec_type = AUDIO_CODEC_TYPE_G722;
    channel_options.audio_codec_opt.pcm_sample_rate = 16000;
    channel_options.audio_codec_opt.pcm_channel_num = 1;
    channel_options.audio_codec_opt.pcm_duration = AI_RTC_AGORA_SERVICE_PCM_DURATION_MS;

    if (service_has_text(config->user_account))
    {
        AI_RTC_AGORA_LOG("join conn=%d mode=user_account uid=%lu audio=%d video=%d control_peer=%d",
                         (int)s_agora.conn_id,
                         (unsigned long)config->uid,
                         config->enable_audio ? 1 : 0,
                         config->enable_video ? 1 : 0,
                         service_has_text(config->control_peer_id) ? 1 : 0);
        rc = agora_rtc_join_channel_with_user_account(s_agora.conn_id,
                                                      config->channel_name,
                                                      config->user_account,
                                                      config->rtc_token,
                                                      &channel_options);
    }
    else
    {
        AI_RTC_AGORA_LOG("join conn=%d mode=numeric uid=%lu audio=%d video=%d control_peer=%d",
                         (int)s_agora.conn_id,
                         (unsigned long)config->uid,
                         config->enable_audio ? 1 : 0,
                         config->enable_video ? 1 : 0,
                         service_has_text(config->control_peer_id) ? 1 : 0);
        rc = agora_rtc_join_channel(s_agora.conn_id,
                                    config->channel_name,
                                    config->uid,
                                    config->rtc_token,
                                    &channel_options);
    }
    if (rc < 0)
    {
        connection_id_t failed_conn_id = s_agora.conn_id;

        AI_RTC_AGORA_LOG("join failed conn=%d rc=%d gen=%lu",
                         (int)failed_conn_id,
                         rc,
                         (unsigned long)s_agora.generation);
        s_agora.session_active = false;
        s_agora.conn_id = CONNECTION_ID_INVALID;
        s_agora.phase = AI_RTC_AGORA_SESSION_FAILED;
        AI_RTC_AGORA_LOG("destroy_conn conn=%d gen=%lu", (int)failed_conn_id, (unsigned long)s_agora.generation);
        (void)agora_rtc_destroy_connection(failed_conn_id);
        s_agora.conn_id = CONNECTION_ID_INVALID;
        (void)service_shutdown_sdk();
        return AI_RTC_FACADE_ERR_INTERNAL;
    }

    return AI_RTC_FACADE_OK;
}

int Ai_Rtc_Agora_Service_Stop(void)
{
    int rc = AI_RTC_FACADE_OK;

    if (service_stop_session() != AI_RTC_FACADE_OK)
    {
        rc = AI_RTC_FACADE_ERR_INTERNAL;
    }
    service_reset_session_state();
    return rc;
}

int Ai_Rtc_Agora_Service_Wait_Joined(uint32_t timeout_ms)
{
    const Ai_Rtc_Agora_Port_t *port = s_agora.port;
    uint32_t waited_ms = 0;
    uint32_t start_ms = 0;
    bool has_timestamp = false;

    if (s_agora.joined)
    {
        return AI_RTC_FACADE_OK;
    }
    if (!s_agora.session_active || timeout_ms == 0u)
    {
        return AI_RTC_FACADE_ERR_NOT_READY;
    }

    if (port == NULL)
    {
        port = Ai_Rtc_Agora_Port_Get();
    }
    if (port == NULL || port->sleep_ms == NULL)
    {
        return AI_RTC_FACADE_ERR_NOT_READY;
    }

    if (port->timestamp_ms != NULL)
    {
        start_ms = port->timestamp_ms();
        has_timestamp = true;
    }

    while (s_agora.session_active && !s_agora.joined && waited_ms < timeout_ms)
    {
        uint32_t remaining_ms = timeout_ms - waited_ms;
        uint32_t step_ms = (remaining_ms < AI_RTC_AGORA_JOIN_WAIT_POLL_MS)
                               ? remaining_ms
                               : AI_RTC_AGORA_JOIN_WAIT_POLL_MS;

        port->sleep_ms(step_ms);
        if (has_timestamp)
        {
            waited_ms = port->timestamp_ms() - start_ms;
        }
        else
        {
            waited_ms += step_ms;
        }
    }

    return s_agora.joined ? AI_RTC_FACADE_OK : AI_RTC_FACADE_ERR_NOT_READY;
}

int Ai_Rtc_Agora_Service_Send_Audio(const Ai_Rtc_Facade_Audio_Frame_t *frame)
{
    const bool active = s_agora.session_active;
    const bool joined = s_agora.joined;
    const bool remote_user_joined = s_agora.remote_user_joined;
    const connection_id_t conn_id = s_agora.conn_id;
    audio_frame_info_t info;

    if (frame == NULL || frame->data == NULL || frame->len == 0u)
    {
        return AI_RTC_FACADE_ERR_NOT_READY;
    }
    if (!active || !joined || !remote_user_joined || conn_id == CONNECTION_ID_INVALID)
    {
        if (service_should_log_not_ready(&s_agora.audio_not_ready_logs))
        {
            AI_RTC_AGORA_LOG("send_audio not_ready active=%d joined=%d remote_user_joined=%d conn=%d gen=%lu",
                             active ? 1 : 0,
                             joined ? 1 : 0,
                             remote_user_joined ? 1 : 0,
                             (int)conn_id,
                             (unsigned long)s_agora.generation);
        }
        return AI_RTC_FACADE_ERR_NOT_READY;
    }

    memset(&info, 0, sizeof(info));
    info.data_type = map_facade_audio_format(frame->format);
    return (agora_rtc_send_audio_data(conn_id, frame->data, frame->len, &info) < 0)
        ? AI_RTC_FACADE_ERR_INTERNAL
        : AI_RTC_FACADE_OK;
}

int Ai_Rtc_Agora_Service_Send_Video(const Ai_Rtc_Facade_Video_Frame_t *frame)
{
    const bool active = s_agora.session_active;
    const bool joined = s_agora.joined;
    const connection_id_t conn_id = s_agora.conn_id;
    video_frame_info_t info;

    if (frame == NULL || frame->data == NULL || frame->len == 0u)
    {
        return AI_RTC_FACADE_ERR_NOT_READY;
    }
    if (!active || !joined || conn_id == CONNECTION_ID_INVALID)
    {
        return AI_RTC_FACADE_ERR_NOT_READY;
    }

    memset(&info, 0, sizeof(info));
    info.data_type = map_facade_video_format(frame->format);
    info.stream_type = VIDEO_STREAM_HIGH;
    info.frame_type = map_facade_video_frame_type(frame->flags);
    info.frame_rate = (video_frame_rate_e)frame->frame_rate_hz;
    info.rotation = VIDEO_ORIENTATION_0;

    return (agora_rtc_send_video_data(conn_id, frame->data, frame->len, &info) < 0)
        ? AI_RTC_FACADE_ERR_INTERNAL
        : AI_RTC_FACADE_OK;
}

int Ai_Rtc_Agora_Service_Send_Datastream(const uint8_t *data, size_t len)
{
    const bool active = s_agora.session_active;
    const bool joined = s_agora.joined;
    const bool control_ready = Ai_Rtc_Agora_Control_Is_Ready(&s_agora.control_state);
    const connection_id_t conn_id = s_agora.conn_id;

    if (!active || !joined || !control_ready || conn_id == CONNECTION_ID_INVALID || data == NULL || len == 0u)
    {
        if (service_should_log_not_ready(&s_agora.datastream_not_ready_logs))
        {
            AI_RTC_AGORA_LOG("send_datastream not_ready joined=%d control_ready=%d data=%d len=%lu conn=%d gen=%lu",
                             joined ? 1 : 0,
                             control_ready ? 1 : 0,
                             data != NULL ? 1 : 0,
                             (unsigned long)len,
                             (int)conn_id,
                             (unsigned long)s_agora.generation);
        }
        return AI_RTC_FACADE_ERR_NOT_READY;
    }

    return Ai_Rtc_Agora_Control_Send(&s_agora.control_state, conn_id, data, len);
}

int Ai_Rtc_Agora_Service_Renew_Token(const char *token)
{
    if (!s_agora.sdk_initialized || s_agora.conn_id == CONNECTION_ID_INVALID || token == NULL)
    {
        return AI_RTC_FACADE_ERR_NOT_READY;
    }
    AI_RTC_AGORA_LOG("renew_token conn=%d gen=%lu", (int)s_agora.conn_id, (unsigned long)s_agora.generation);
    return (agora_rtc_renew_token(s_agora.conn_id, token) < 0) ? AI_RTC_FACADE_ERR_INTERNAL : AI_RTC_FACADE_OK;
}

int Ai_Rtc_Agora_Service_Shutdown(void)
{
    int rc = AI_RTC_FACADE_OK;

    if (service_stop_session() != AI_RTC_FACADE_OK)
    {
        rc = AI_RTC_FACADE_ERR_INTERNAL;
    }
    if (service_shutdown_sdk() != AI_RTC_FACADE_OK)
    {
        rc = AI_RTC_FACADE_ERR_INTERNAL;
    }
    service_reset_session_state();
    return rc;
}
