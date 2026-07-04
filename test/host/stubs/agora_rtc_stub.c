#include "agora_rtc_api.h"

#include <string.h>

static Agora_Rtc_Stub_State_t s_stub;
static agora_rtc_event_handler_t s_handler;
static agora_rtm_handler_t s_rtm_handler;

const char *agora_rtc_get_version(void)
{
    return "host-stub";
}

const char *agora_rtc_err_2_str(int err)
{
    (void)err;
    return "host-stub-error";
}

int agora_rtc_init(const char *app_id, const agora_rtc_event_handler_t *event_handler, rtc_service_option_t *option)
{
    (void)option;
    s_stub.init_calls++;
    s_stub.last_app_id = app_id;
    if (event_handler != NULL)
    {
        s_handler = *event_handler;
    }
    return 0;
}

int agora_rtc_fini(void)
{
    s_stub.fini_calls++;
    return 0;
}

int agora_rtc_create_connection(connection_id_t *conn_id)
{
    s_stub.create_connection_calls++;
    s_stub.conn_id = 1000u + (connection_id_t)s_stub.create_connection_calls;
    if (conn_id != NULL)
    {
        *conn_id = s_stub.conn_id;
    }
    return 0;
}

int agora_rtc_destroy_connection(connection_id_t conn_id)
{
    s_stub.last_destroy_conn_id = conn_id;
    s_stub.destroy_connection_calls++;
    return 0;
}

int agora_rtc_join_channel(connection_id_t conn_id,
                           const char *channel_name,
                           uint32_t uid,
                           const char *token,
                           rtc_channel_options_t *options)
{
    s_stub.last_join_conn_id = conn_id;
    s_stub.join_channel_calls++;
    s_stub.last_channel_name = channel_name;
    s_stub.last_uid = uid;
    s_stub.last_token = token;
    if (options != NULL)
    {
        s_stub.last_auto_subscribe_audio = options->auto_subscribe_audio;
        s_stub.last_auto_subscribe_video = options->auto_subscribe_video;
        s_stub.last_enable_audio_jitter_buffer = options->enable_audio_jitter_buffer;
        s_stub.last_enable_audio_downlink_aec = options->enable_audio_downlink_aec;
        s_stub.last_enable_audio_ai_qos = options->enable_audio_ai_qos;
        s_stub.last_enable_audio_decode = options->enable_audio_decode;
        s_stub.last_audio_codec_type = options->audio_codec_opt.audio_codec_type;
        s_stub.last_pcm_sample_rate = options->audio_codec_opt.pcm_sample_rate;
        s_stub.last_pcm_channel_num = options->audio_codec_opt.pcm_channel_num;
        s_stub.last_pcm_duration = options->audio_codec_opt.pcm_duration;
    }
    return 0;
}

int agora_rtc_join_channel_with_user_account(connection_id_t conn_id,
                                             const char *channel_name,
                                             const char *user_account,
                                             const char *token,
                                             rtc_channel_options_t *options)
{
    s_stub.last_join_conn_id = conn_id;
    s_stub.join_channel_with_user_account_calls++;
    s_stub.last_channel_name = channel_name;
    s_stub.last_user_account = user_account;
    s_stub.last_token = token;
    if (options != NULL)
    {
        s_stub.last_auto_subscribe_audio = options->auto_subscribe_audio;
        s_stub.last_auto_subscribe_video = options->auto_subscribe_video;
        s_stub.last_enable_audio_jitter_buffer = options->enable_audio_jitter_buffer;
        s_stub.last_enable_audio_downlink_aec = options->enable_audio_downlink_aec;
        s_stub.last_enable_audio_ai_qos = options->enable_audio_ai_qos;
        s_stub.last_enable_audio_decode = options->enable_audio_decode;
        s_stub.last_audio_codec_type = options->audio_codec_opt.audio_codec_type;
        s_stub.last_pcm_sample_rate = options->audio_codec_opt.pcm_sample_rate;
        s_stub.last_pcm_channel_num = options->audio_codec_opt.pcm_channel_num;
        s_stub.last_pcm_duration = options->audio_codec_opt.pcm_duration;
    }
    return 0;
}

int agora_rtc_leave_channel(connection_id_t conn_id)
{
    s_stub.last_leave_conn_id = conn_id;
    s_stub.leave_channel_calls++;
    return 0;
}

int agora_rtc_renew_token(connection_id_t conn_id, const char *token)
{
    (void)conn_id;
    s_stub.renew_token_calls++;
    s_stub.last_renew_token = token;
    return 0;
}

int agora_rtc_send_audio_data(connection_id_t conn_id,
                              const void *data_ptr,
                              size_t data_len,
                              audio_frame_info_t *info_ptr)
{
    (void)conn_id;
    s_stub.send_audio_calls++;
    s_stub.last_audio_data = data_ptr;
    s_stub.last_audio_len = data_len;
    if (info_ptr != NULL)
    {
        s_stub.last_audio_type = info_ptr->data_type;
    }
    return 0;
}

int agora_rtc_send_video_data(connection_id_t conn_id,
                              const void *data_ptr,
                              size_t data_len,
                              video_frame_info_t *info_ptr)
{
    (void)conn_id;
    s_stub.send_video_calls++;
    s_stub.last_video_data = data_ptr;
    s_stub.last_video_len = data_len;
    if (info_ptr != NULL)
    {
        s_stub.last_video_type = info_ptr->data_type;
        s_stub.last_video_frame_type = info_ptr->frame_type;
        s_stub.last_video_stream_type = info_ptr->stream_type;
        s_stub.last_video_frame_rate = info_ptr->frame_rate;
    }
    return 0;
}

int agora_rtc_create_data_stream(connection_id_t conn_id, int *stream_id, bool reliable, bool ordered)
{
    (void)conn_id;
    s_stub.create_data_stream_calls++;
    s_stub.stream_id = 55;
    s_stub.last_reliable = reliable;
    s_stub.last_ordered = ordered;
    if (stream_id != NULL)
    {
        *stream_id = s_stub.stream_id;
    }
    return 0;
}

int agora_rtc_send_stream_message(connection_id_t conn_id, int stream_id, const char *data, size_t length)
{
    (void)conn_id;
    s_stub.send_stream_calls++;
    s_stub.stream_id = stream_id;
    s_stub.last_stream_data = data;
    s_stub.last_stream_len = length;
    return 0;
}

int agora_rtc_login_rtm(const char *rtm_uid, const char *rtm_token, const agora_rtm_handler_t *handler)
{
    s_stub.login_rtm_calls++;
    s_stub.last_rtm_uid = rtm_uid;
    s_stub.last_rtm_token = rtm_token;
    if (handler != NULL)
    {
        s_rtm_handler = *handler;
    }
    return 0;
}

int agora_rtc_logout_rtm(void)
{
    s_stub.logout_rtm_calls++;
    memset(&s_rtm_handler, 0, sizeof(s_rtm_handler));
    return 0;
}

int agora_rtc_send_rtm_data(const char *rtm_uid,
                            const void *msg,
                            size_t msg_len,
                            uint32_t msg_id,
                            const char *custom_type)
{
    (void)custom_type;
    s_stub.send_rtm_calls++;
    s_stub.last_rtm_peer_uid = rtm_uid;
    s_stub.last_rtm_data = msg;
    s_stub.last_rtm_len = msg_len;
    s_stub.last_rtm_msg_id = msg_id;
    return 0;
}

int agora_rtc_notify_network_event(network_event_e event)
{
    s_stub.notify_network_calls++;
    s_stub.last_network_event = event;
    return 0;
}

void Agora_Rtc_Stub_Reset(void)
{
    memset(&s_stub, 0, sizeof(s_stub));
    memset(&s_handler, 0, sizeof(s_handler));
    memset(&s_rtm_handler, 0, sizeof(s_rtm_handler));
}

Agora_Rtc_Stub_State_t *Agora_Rtc_Stub_State(void)
{
    return &s_stub;
}

void Agora_Rtc_Stub_Emit_Joined(void)
{
    Agora_Rtc_Stub_Emit_Joined_For_Conn(s_stub.conn_id);
}

void Agora_Rtc_Stub_Emit_Joined_For_Conn(connection_id_t conn_id)
{
    if (s_handler.on_join_channel_success != NULL)
    {
        s_handler.on_join_channel_success(conn_id, s_stub.last_uid, 123);
    }
}

void Agora_Rtc_Stub_Emit_Reconnecting(void)
{
    Agora_Rtc_Stub_Emit_Reconnecting_For_Conn(s_stub.conn_id);
}

void Agora_Rtc_Stub_Emit_Reconnecting_For_Conn(connection_id_t conn_id)
{
    if (s_handler.on_reconnecting != NULL)
    {
        s_handler.on_reconnecting(conn_id);
    }
}

void Agora_Rtc_Stub_Emit_Rejoined(void)
{
    Agora_Rtc_Stub_Emit_Rejoined_For_Conn(s_stub.conn_id);
}

void Agora_Rtc_Stub_Emit_Rejoined_For_Conn(connection_id_t conn_id)
{
    if (s_handler.on_rejoin_channel_success != NULL)
    {
        s_handler.on_rejoin_channel_success(conn_id, s_stub.last_uid, 456);
    }
}

void Agora_Rtc_Stub_Emit_User_Joined(uint32_t uid)
{
    Agora_Rtc_Stub_Emit_User_Joined_For_Conn(s_stub.conn_id, uid);
}

void Agora_Rtc_Stub_Emit_User_Joined_For_Conn(connection_id_t conn_id, uint32_t uid)
{
    if (s_handler.on_user_joined != NULL)
    {
        s_handler.on_user_joined(conn_id, uid, 0);
    }
}

void Agora_Rtc_Stub_Emit_User_Offline(uint32_t uid)
{
    Agora_Rtc_Stub_Emit_User_Offline_For_Conn(s_stub.conn_id, uid);
}

void Agora_Rtc_Stub_Emit_User_Offline_For_Conn(connection_id_t conn_id, uint32_t uid)
{
    if (s_handler.on_user_offline != NULL)
    {
        s_handler.on_user_offline(conn_id, uid, 0);
    }
}

void Agora_Rtc_Stub_Emit_Token_Will_Expire(void)
{
    Agora_Rtc_Stub_Emit_Token_Will_Expire_For_Conn(s_stub.conn_id);
}

void Agora_Rtc_Stub_Emit_Token_Will_Expire_For_Conn(connection_id_t conn_id)
{
    if (s_handler.on_token_privilege_will_expire != NULL)
    {
        s_handler.on_token_privilege_will_expire(conn_id, s_stub.last_token);
    }
}

void Agora_Rtc_Stub_Emit_Audio(const void *data, size_t len, audio_data_type_e type)
{
    Agora_Rtc_Stub_Emit_Audio_For_Conn(s_stub.conn_id, data, len, type);
}

void Agora_Rtc_Stub_Emit_Audio_For_Conn(connection_id_t conn_id,
                                        const void *data,
                                        size_t len,
                                        audio_data_type_e type)
{
    audio_frame_info_t info = {
        .data_type = type,
    };
    if (s_handler.on_audio_data != NULL)
    {
        s_handler.on_audio_data(conn_id, 77u, 321u, data, len, &info);
    }
}

void Agora_Rtc_Stub_Emit_Video(const void *data, size_t len, video_data_type_e type, video_frame_type_e frame_type)
{
    Agora_Rtc_Stub_Emit_Video_For_Conn(s_stub.conn_id, data, len, type, frame_type);
}

void Agora_Rtc_Stub_Emit_Video_For_Conn(connection_id_t conn_id,
                                        const void *data,
                                        size_t len,
                                        video_data_type_e type,
                                        video_frame_type_e frame_type)
{
    video_frame_info_t info = {
        .data_type = type,
        .stream_type = VIDEO_STREAM_HIGH,
        .frame_type = frame_type,
        .frame_rate = 0,
        .rotation = VIDEO_ORIENTATION_0,
    };
    if (s_handler.on_video_data != NULL)
    {
        s_handler.on_video_data(conn_id, 77u, 321u, data, len, &info);
    }
}

void Agora_Rtc_Stub_Emit_Target_Bitrate(uint32_t target_bitrate)
{
    if (s_handler.on_target_bitrate_changed != NULL)
    {
        s_stub.target_bitrate_callbacks++;
        s_stub.last_target_bitrate = target_bitrate;
        s_handler.on_target_bitrate_changed(s_stub.conn_id, target_bitrate);
    }
}

void Agora_Rtc_Stub_Emit_Key_Frame_Request(uint32_t uid)
{
    if (s_handler.on_key_frame_gen_req != NULL)
    {
        s_stub.key_frame_callbacks++;
        s_handler.on_key_frame_gen_req(s_stub.conn_id, uid, VIDEO_STREAM_HIGH);
    }
}

void Agora_Rtc_Stub_Emit_Stream_Message(int stream_id, uint32_t uid, const char *data, size_t len, uint64_t sent_ts)
{
    Agora_Rtc_Stub_Emit_Stream_Message_For_Conn(s_stub.conn_id, stream_id, uid, data, len, sent_ts);
}

void Agora_Rtc_Stub_Emit_Stream_Message_For_Conn(connection_id_t conn_id,
                                                 int stream_id,
                                                 uint32_t uid,
                                                 const char *data,
                                                 size_t len,
                                                 uint64_t sent_ts)
{
    if (s_handler.on_stream_message != NULL)
    {
        s_handler.on_stream_message(conn_id, uid, stream_id, data, len, sent_ts);
    }
}

void Agora_Rtc_Stub_Emit_Rtm_Login_Ok(void)
{
    if (s_rtm_handler.on_rtm_event != NULL)
    {
        s_rtm_handler.on_rtm_event(s_stub.last_rtm_uid, RTM_EVENT_TYPE_LOGIN, ERR_RTM_OK);
    }
}

void Agora_Rtc_Stub_Emit_Rtm_Data(const char *rtm_uid, const void *msg, size_t msg_len)
{
    if (s_rtm_handler.on_rtm_data != NULL)
    {
        s_rtm_handler.on_rtm_data(rtm_uid, msg, msg_len, NULL);
    }
}
