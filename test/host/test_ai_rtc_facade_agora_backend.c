#include "ai_rtc_facade.h"
#include "ai_rtc_agora_control_transport.h"
#include "agora_rtc_api.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; } } while (0)

typedef struct
{
    int joined_events;
    int reconnecting_events;
    int rejoined_events;
    int remote_joined_events;
    int remote_offline_events;
    int token_will_expire_events;
    int failed_events;
    int audio_rx_calls;
    int video_rx_calls;
    int datastream_rx_calls;
    int last_detail;
    Ai_Rtc_Facade_State_t last_state;
    Ai_Rtc_Facade_Audio_Frame_t last_audio;
    Ai_Rtc_Facade_Video_Frame_t last_video;
    Ai_Rtc_Facade_Datastream_Message_t last_message;
} Test_State_t;

static int s_rtm_event_calls;
static int s_rtm_data_calls;
static const char *s_last_rtm_event_uid;
static rtm_event_type_e s_last_rtm_event_type;
static rtm_err_code_e s_last_rtm_err_code;
static const char *s_last_rtm_data_uid;
static const void *s_last_rtm_data;
static size_t s_last_rtm_data_len;

void Ai_Rtc_Agora_Port_Stub_Reset(void);
void Ai_Rtc_Agora_Port_Stub_Join_After_Sleeps(int count);
int Ai_Rtc_Agora_Port_Stub_Sleep_Calls(void);

static void on_state(Ai_Rtc_Facade_Event_t event,
                     Ai_Rtc_Facade_State_t state,
                     int detail,
                     void *user)
{
    Test_State_t *test = (Test_State_t *)user;
    test->last_state = state;
    test->last_detail = detail;
    if (event == AI_RTC_FACADE_EVENT_JOINED)
    {
        test->joined_events++;
    }
    if (event == AI_RTC_FACADE_EVENT_RECONNECTING)
    {
        test->reconnecting_events++;
    }
    if (event == AI_RTC_FACADE_EVENT_REJOINED)
    {
        test->rejoined_events++;
    }
    if (event == AI_RTC_FACADE_EVENT_REMOTE_USER_JOINED)
    {
        test->remote_joined_events++;
    }
    if (event == AI_RTC_FACADE_EVENT_REMOTE_USER_OFFLINE)
    {
        test->remote_offline_events++;
    }
    if (event == AI_RTC_FACADE_EVENT_TOKEN_WILL_EXPIRE)
    {
        test->token_will_expire_events++;
    }
    if (event == AI_RTC_FACADE_EVENT_FAILED)
    {
        test->failed_events++;
    }
}

static int on_audio_rx(const Ai_Rtc_Facade_Audio_Frame_t *frame, void *user)
{
    Test_State_t *test = (Test_State_t *)user;
    if (frame == NULL)
    {
        return AI_RTC_FACADE_ERR_INVALID_ARG;
    }
    test->audio_rx_calls++;
    test->last_audio = *frame;
    return AI_RTC_FACADE_OK;
}

static int on_video_rx(const Ai_Rtc_Facade_Video_Frame_t *frame, void *user)
{
    Test_State_t *test = (Test_State_t *)user;
    if (frame == NULL)
    {
        return AI_RTC_FACADE_ERR_INVALID_ARG;
    }
    test->video_rx_calls++;
    test->last_video = *frame;
    return AI_RTC_FACADE_OK;
}

static int on_datastream_rx(const Ai_Rtc_Facade_Datastream_Message_t *message, void *user)
{
    Test_State_t *test = (Test_State_t *)user;
    if (message == NULL)
    {
        return AI_RTC_FACADE_ERR_INVALID_ARG;
    }
    test->datastream_rx_calls++;
    test->last_message = *message;
    return AI_RTC_FACADE_OK;
}

static void on_rtm_event(const char *rtm_uid, rtm_event_type_e event_type, rtm_err_code_e err_code)
{
    s_rtm_event_calls++;
    s_last_rtm_event_uid = rtm_uid;
    s_last_rtm_event_type = event_type;
    s_last_rtm_err_code = err_code;
}

static void on_rtm_data(const char *rtm_uid, const void *msg, size_t msg_len, const char *custom_type)
{
    (void)custom_type;
    s_rtm_data_calls++;
    s_last_rtm_data_uid = rtm_uid;
    s_last_rtm_data = msg;
    s_last_rtm_data_len = msg_len;
}

static int test_agora_backend_wait_joined_polls_until_joined(void)
{
    Test_State_t test;
    const Ai_Rtc_Facade_Config_t config = {
        .join_timeout_ms = 3000,
        .enable_audio = true,
        .enable_video = false,
    };
    const Ai_Rtc_Facade_Callbacks_t callbacks = {
        .on_state = on_state,
        .on_audio_rx = on_audio_rx,
        .on_datastream_rx = on_datastream_rx,
        .user = &test,
    };
    const Ai_Rtc_Facade_Token_Result_t token = {
        .result = 0,
        .rtc_token = "token",
        .channel_name = "channel",
        .app_id = "appid",
        .uid = 9,
    };

    memset(&test, 0, sizeof(test));
    Agora_Rtc_Stub_Reset();
    Ai_Rtc_Agora_Port_Stub_Reset();
    CHECK(Ai_Rtc_Facade_Init(&config, &callbacks) == AI_RTC_FACADE_OK);
    CHECK(Ai_Rtc_Facade_On_Token_Result(&token) == AI_RTC_FACADE_OK);
    CHECK(!Ai_Rtc_Facade_Is_Joined());

    Ai_Rtc_Agora_Port_Stub_Join_After_Sleeps(3);
    CHECK(Ai_Rtc_Facade_Wait_Joined(100) == AI_RTC_FACADE_OK);
    CHECK(Ai_Rtc_Facade_Is_Joined());
    CHECK(test.joined_events == 1);
    CHECK(Ai_Rtc_Agora_Port_Stub_Sleep_Calls() == 3);

    CHECK(Ai_Rtc_Facade_Stop() == AI_RTC_FACADE_OK);
    Ai_Rtc_Facade_Deinit();
    return 0;
}

static int test_private_control_transport_header_is_c_usable(void)
{
    Ai_Rtc_Agora_Control_State_t state = {
        .stream_id = -1,
        .rtm_logged_in = false,
        .next_rtm_msg_id = 0,
        .control_peer_id = "agent-user",
    };
    const Ai_Rtc_Agora_Control_Config_t config = {
        .conn_id = 12u,
        .rtc_token = "rtc-token",
        .user_account = "local-user",
        .control_peer_id = "agent-user",
        .control_token = "control-token",
    };

    CHECK(state.stream_id == -1);
    CHECK(!state.rtm_logged_in);
    CHECK(state.next_rtm_msg_id == 0u);
    CHECK(strcmp(state.control_peer_id, "agent-user") == 0);
    CHECK(config.conn_id == 12u);
    CHECK(strcmp(config.rtc_token, "rtc-token") == 0);
    CHECK(strcmp(config.user_account, "local-user") == 0);
    CHECK(strcmp(config.control_peer_id, "agent-user") == 0);
    CHECK(strcmp(config.control_token, "control-token") == 0);
    return 0;
}

static int test_agora_backend_rejects_stop_while_joining(void)
{
    Test_State_t test;
    const Ai_Rtc_Facade_Config_t config = {
        .join_timeout_ms = 3000,
        .enable_audio = true,
        .enable_video = false,
    };
    const Ai_Rtc_Facade_Callbacks_t callbacks = {
        .on_state = on_state,
        .user = &test,
    };
    Ai_Rtc_Facade_Token_Result_t token = {
        .result = 0,
        .rtc_token = "token",
        .channel_name = "channel",
        .app_id = "appid",
        .uid = 9,
    };

    memset(&test, 0, sizeof(test));
    Agora_Rtc_Stub_Reset();
    Ai_Rtc_Agora_Port_Stub_Reset();

    CHECK(Ai_Rtc_Facade_Init(&config, &callbacks) == AI_RTC_FACADE_OK);
    CHECK(Ai_Rtc_Facade_On_Token_Result(&token) == AI_RTC_FACADE_OK);
    CHECK(Ai_Rtc_Facade_Get_State() == AI_RTC_FACADE_STATE_JOINING);
    CHECK(Agora_Rtc_Stub_State()->join_channel_calls == 1);

    token.rtc_token = "token-2";
    CHECK(Ai_Rtc_Facade_On_Token_Result(&token) == AI_RTC_FACADE_ERR_BUSY);
    CHECK(Agora_Rtc_Stub_State()->create_connection_calls == 1);
    CHECK(Agora_Rtc_Stub_State()->join_channel_calls == 1);

    CHECK(Ai_Rtc_Facade_Stop() == AI_RTC_FACADE_ERR_BUSY);
    CHECK(Ai_Rtc_Facade_Get_State() == AI_RTC_FACADE_STATE_JOINING);
    CHECK(Agora_Rtc_Stub_State()->leave_channel_calls == 0);
    CHECK(Agora_Rtc_Stub_State()->destroy_connection_calls == 0);

    Agora_Rtc_Stub_Emit_Joined();
    CHECK(Ai_Rtc_Facade_Get_State() == AI_RTC_FACADE_STATE_JOINED);
    CHECK(Ai_Rtc_Facade_Stop() == AI_RTC_FACADE_OK);
    CHECK(Agora_Rtc_Stub_State()->leave_channel_calls == 1);
    CHECK(Agora_Rtc_Stub_State()->destroy_connection_calls == 1);

    Ai_Rtc_Facade_Deinit();
    return 0;
}

static int test_agora_backend_flow(void)
{
    Test_State_t test;
    uint8_t audio_data[8] = {0};
    uint8_t video_data[6] = {9, 8, 7, 6, 5, 4};
    uint8_t msg[] = "msg";
    char rx_msg[] = "rx";
    const Ai_Rtc_Facade_Config_t config = {
        .join_timeout_ms = 3000,
        .enable_audio = true,
        .enable_video = true,
    };
    const Ai_Rtc_Facade_Callbacks_t callbacks = {
        .on_state = on_state,
        .on_audio_rx = on_audio_rx,
        .on_video_rx = on_video_rx,
        .on_datastream_rx = on_datastream_rx,
        .user = &test,
    };
    Ai_Rtc_Facade_Token_Result_t token = {
        .result = 0,
        .rtc_token = "token",
        .channel_name = "channel",
        .app_id = "appid",
        .uid = 9,
    };
    const Ai_Rtc_Facade_Audio_Frame_t audio = {
        .data = audio_data,
        .len = sizeof(audio_data),
        .format = AI_RTC_FACADE_AUDIO_FORMAT_G722,
        .sample_rate_hz = 16000,
        .channels = 1,
        .duration_ms = 20,
    };
    const Ai_Rtc_Facade_Video_Frame_t video = {
        .data = video_data,
        .len = sizeof(video_data),
        .format = AI_RTC_FACADE_VIDEO_FORMAT_H264,
        .width = 640,
        .height = 360,
        .frame_rate_hz = 15,
        .timestamp_ms = 20,
        .flags = AI_RTC_FACADE_VIDEO_FLAG_KEYFRAME,
    };

    memset(&test, 0, sizeof(test));
    Agora_Rtc_Stub_Reset();
    Ai_Rtc_Agora_Port_Stub_Reset();
    CHECK(Ai_Rtc_Facade_Init(&config, &callbacks) == AI_RTC_FACADE_OK);
    CHECK(Ai_Rtc_Facade_On_Token_Result(&token) == AI_RTC_FACADE_OK);
    CHECK(Agora_Rtc_Stub_State()->init_calls == 1);
    CHECK(Agora_Rtc_Stub_State()->create_connection_calls == 1);
    CHECK(Agora_Rtc_Stub_State()->join_channel_calls == 1);
    CHECK(Agora_Rtc_Stub_State()->join_channel_with_user_account_calls == 0);
    CHECK(strcmp(Agora_Rtc_Stub_State()->last_app_id, "appid") == 0);
    CHECK(strcmp(Agora_Rtc_Stub_State()->last_channel_name, "channel") == 0);
    CHECK(strcmp(Agora_Rtc_Stub_State()->last_token, "token") == 0);
    CHECK(Agora_Rtc_Stub_State()->last_uid == 9u);
    CHECK(Agora_Rtc_Stub_State()->last_auto_subscribe_audio);
    CHECK(Agora_Rtc_Stub_State()->last_auto_subscribe_video);
    CHECK(Agora_Rtc_Stub_State()->last_enable_audio_jitter_buffer);
    CHECK(!Agora_Rtc_Stub_State()->last_enable_audio_downlink_aec);
    CHECK(!Agora_Rtc_Stub_State()->last_enable_audio_ai_qos);
    CHECK(!Agora_Rtc_Stub_State()->last_enable_audio_decode);
    CHECK(Agora_Rtc_Stub_State()->last_audio_codec_type == AUDIO_CODEC_TYPE_G722);
    CHECK(Agora_Rtc_Stub_State()->last_pcm_sample_rate == 16000);
    CHECK(Agora_Rtc_Stub_State()->last_pcm_channel_num == 1);
    CHECK(Agora_Rtc_Stub_State()->last_pcm_duration == 20);
    CHECK(!Ai_Rtc_Facade_Is_Joined());
    CHECK(Ai_Rtc_Facade_Send_Datastream(msg, sizeof(msg)) == AI_RTC_FACADE_ERR_NOT_READY);
    CHECK(Agora_Rtc_Stub_State()->send_stream_calls == 0);

    Agora_Rtc_Stub_Emit_Joined();
    CHECK(Ai_Rtc_Facade_Is_Joined());
    CHECK(test.joined_events == 1);
    CHECK(Agora_Rtc_Stub_State()->create_data_stream_calls == 1);
    CHECK(Agora_Rtc_Stub_State()->last_reliable);
    CHECK(Agora_Rtc_Stub_State()->last_ordered);
    CHECK(Ai_Rtc_Facade_Send_Video(&video) == AI_RTC_FACADE_OK);
    CHECK(Agora_Rtc_Stub_State()->send_video_calls == 1);
    CHECK(Agora_Rtc_Stub_State()->last_video_data == video_data);
    CHECK(Agora_Rtc_Stub_State()->last_video_len == sizeof(video_data));
    CHECK(Agora_Rtc_Stub_State()->last_video_type == VIDEO_DATA_TYPE_H264);
    CHECK(Agora_Rtc_Stub_State()->last_video_frame_type == VIDEO_FRAME_KEY);
    CHECK(Agora_Rtc_Stub_State()->last_video_frame_rate == 15);

    Agora_Rtc_Stub_Emit_Video(video_data, sizeof(video_data), VIDEO_DATA_TYPE_H264, VIDEO_FRAME_KEY);
    CHECK(test.video_rx_calls == 1);
    CHECK(test.last_video.data == video_data);
    CHECK(test.last_video.len == sizeof(video_data));
    CHECK(test.last_video.format == AI_RTC_FACADE_VIDEO_FORMAT_H264);
    CHECK(test.last_video.flags == AI_RTC_FACADE_VIDEO_FLAG_KEYFRAME);
    CHECK(Ai_Rtc_Facade_Send_Audio(&audio) == AI_RTC_FACADE_ERR_NOT_READY);
    CHECK(Agora_Rtc_Stub_State()->send_audio_calls == 0);

    Agora_Rtc_Stub_Emit_User_Joined(66u);
    CHECK(test.remote_joined_events == 1);
    CHECK(test.last_detail == 66);
    CHECK(Ai_Rtc_Facade_Send_Audio(&audio) == AI_RTC_FACADE_OK);
    CHECK(Agora_Rtc_Stub_State()->send_audio_calls == 1);

    Agora_Rtc_Stub_Emit_Reconnecting();
    CHECK(test.reconnecting_events == 1);
    CHECK(!Ai_Rtc_Facade_Is_Joined());
    CHECK(Ai_Rtc_Facade_Send_Datastream(msg, sizeof(msg)) == AI_RTC_FACADE_ERR_NOT_READY);
    CHECK(Ai_Rtc_Facade_Send_Audio(&audio) == AI_RTC_FACADE_ERR_NOT_READY);
    Agora_Rtc_Stub_Emit_Rejoined();
    CHECK(test.rejoined_events == 1);
    CHECK(Ai_Rtc_Facade_Is_Joined());

    CHECK(Ai_Rtc_Facade_Send_Audio(&audio) == AI_RTC_FACADE_ERR_NOT_READY);
    CHECK(Agora_Rtc_Stub_State()->send_audio_calls == 1);
    Agora_Rtc_Stub_Emit_User_Joined(66u);
    CHECK(test.remote_joined_events == 2);
    CHECK(Ai_Rtc_Facade_Send_Audio(&audio) == AI_RTC_FACADE_OK);
    CHECK(Agora_Rtc_Stub_State()->send_audio_calls == 2);
    Agora_Rtc_Stub_Emit_User_Offline(66u);
    CHECK(test.remote_offline_events == 1);
    CHECK(test.last_detail == 66);
    CHECK(Ai_Rtc_Facade_Send_Audio(&audio) == AI_RTC_FACADE_ERR_NOT_READY);
    CHECK(Agora_Rtc_Stub_State()->send_audio_calls == 2);
    CHECK(Agora_Rtc_Stub_State()->last_audio_data == audio_data);
    CHECK(Agora_Rtc_Stub_State()->last_audio_len == sizeof(audio_data));
    CHECK(Agora_Rtc_Stub_State()->last_audio_type == AUDIO_DATA_TYPE_G722);

    CHECK(Ai_Rtc_Facade_Send_Datastream(msg, sizeof(msg)) == AI_RTC_FACADE_OK);
    CHECK(Agora_Rtc_Stub_State()->send_stream_calls == 1);
    CHECK(Agora_Rtc_Stub_State()->stream_id == 55);
    CHECK(Agora_Rtc_Stub_State()->last_stream_data == (const char *)msg);
    CHECK(Agora_Rtc_Stub_State()->last_stream_len == sizeof(msg));

    Agora_Rtc_Stub_Emit_Audio(audio_data, sizeof(audio_data), AUDIO_DATA_TYPE_OPUS);
    CHECK(test.audio_rx_calls == 1);
    CHECK(test.last_audio.data == audio_data);
    CHECK(test.last_audio.len == sizeof(audio_data));
    CHECK(test.last_audio.format == AI_RTC_FACADE_AUDIO_FORMAT_OPUS);

    Agora_Rtc_Stub_Emit_Stream_Message(6, 77u, rx_msg, sizeof(rx_msg), 999u);
    CHECK(test.datastream_rx_calls == 1);
    CHECK(test.last_message.stream_id == 6);
    CHECK(test.last_message.sender_uid == 77u);
    CHECK(test.last_message.data == (const uint8_t *)rx_msg);
    CHECK(test.last_message.len == sizeof(rx_msg));
    CHECK(test.last_message.sent_ts == 999u);

    Agora_Rtc_Stub_Emit_Token_Will_Expire();
    CHECK(test.token_will_expire_events == 1);
    token.rtc_token = "renewed-token";
    CHECK(Ai_Rtc_Facade_On_Token_Result(&token) == AI_RTC_FACADE_OK);
    CHECK(Agora_Rtc_Stub_State()->renew_token_calls == 1);
    CHECK(strcmp(Agora_Rtc_Stub_State()->last_renew_token, "renewed-token") == 0);

    CHECK(Ai_Rtc_Facade_Stop() == AI_RTC_FACADE_OK);
    CHECK(Agora_Rtc_Stub_State()->leave_channel_calls == 1);
    CHECK(Agora_Rtc_Stub_State()->destroy_connection_calls == 1);
    CHECK(Agora_Rtc_Stub_State()->fini_calls == 0);
    CHECK(Ai_Rtc_Facade_Get_State() == AI_RTC_FACADE_STATE_IDLE);

    token.rtc_token = "token-2";
    CHECK(Ai_Rtc_Facade_On_Token_Result(&token) == AI_RTC_FACADE_OK);
    CHECK(Agora_Rtc_Stub_State()->init_calls == 1);
    CHECK(Agora_Rtc_Stub_State()->create_connection_calls == 2);
    CHECK(Agora_Rtc_Stub_State()->join_channel_calls == 2);
    Agora_Rtc_Stub_Emit_Joined();
    CHECK(Agora_Rtc_Stub_State()->create_data_stream_calls == 2);
    CHECK(Ai_Rtc_Facade_Stop() == AI_RTC_FACADE_OK);
    CHECK(Agora_Rtc_Stub_State()->leave_channel_calls == 2);
    CHECK(Agora_Rtc_Stub_State()->destroy_connection_calls == 2);
    CHECK(Agora_Rtc_Stub_State()->fini_calls == 0);

    token.app_id = "appid2";
    token.rtc_token = "token-3";
    CHECK(Ai_Rtc_Facade_On_Token_Result(&token) == AI_RTC_FACADE_OK);
    CHECK(Agora_Rtc_Stub_State()->fini_calls == 1);
    CHECK(Agora_Rtc_Stub_State()->init_calls == 2);
    CHECK(Agora_Rtc_Stub_State()->create_connection_calls == 3);
    CHECK(Agora_Rtc_Stub_State()->join_channel_calls == 3);
    Agora_Rtc_Stub_Emit_Joined();
    CHECK(Agora_Rtc_Stub_State()->create_data_stream_calls == 3);
    CHECK(Ai_Rtc_Facade_Stop() == AI_RTC_FACADE_OK);
    CHECK(Agora_Rtc_Stub_State()->leave_channel_calls == 3);
    CHECK(Agora_Rtc_Stub_State()->destroy_connection_calls == 3);
    CHECK(Agora_Rtc_Stub_State()->fini_calls == 1);

    Ai_Rtc_Facade_Deinit();
    CHECK(Agora_Rtc_Stub_State()->fini_calls == 2);
    return 0;
}

static int test_agora_backend_disables_audio_jitter_when_audio_disabled(void)
{
    Test_State_t test;
    const Ai_Rtc_Facade_Config_t config = {
        .join_timeout_ms = 3000,
        .enable_audio = false,
        .enable_video = false,
    };
    const Ai_Rtc_Facade_Callbacks_t callbacks = {
        .on_state = on_state,
        .user = &test,
    };
    const Ai_Rtc_Facade_Token_Result_t token = {
        .result = 0,
        .rtc_token = "token",
        .channel_name = "channel",
        .app_id = "appid",
        .uid = 9,
    };

    memset(&test, 0, sizeof(test));
    Agora_Rtc_Stub_Reset();
    Ai_Rtc_Agora_Port_Stub_Reset();

    CHECK(Ai_Rtc_Facade_Init(&config, &callbacks) == AI_RTC_FACADE_OK);
    CHECK(Ai_Rtc_Facade_On_Token_Result(&token) == AI_RTC_FACADE_OK);
    CHECK(!Agora_Rtc_Stub_State()->last_auto_subscribe_audio);
    CHECK(!Agora_Rtc_Stub_State()->last_enable_audio_jitter_buffer);

    Agora_Rtc_Stub_Emit_Joined();
    CHECK(Ai_Rtc_Facade_Stop() == AI_RTC_FACADE_OK);
    Ai_Rtc_Facade_Deinit();
    return 0;
}

static int test_agora_backend_maps_per_session_ai_qos_option(void)
{
    Test_State_t test;
    const Ai_Rtc_Facade_Config_t config = {
        .join_timeout_ms = 3000,
        .enable_audio = true,
        .enable_video = false,
    };
    const Ai_Rtc_Facade_Callbacks_t callbacks = {
        .on_state = on_state,
        .user = &test,
    };
    Ai_Rtc_Facade_Token_Result_t token = {
        .result = 0,
        .rtc_token = "token",
        .channel_name = "channel",
        .app_id = "appid",
        .uid = 9,
    };

    memset(&test, 0, sizeof(test));
    Agora_Rtc_Stub_Reset();
    Ai_Rtc_Agora_Port_Stub_Reset();

    CHECK(Ai_Rtc_Facade_Init(&config, &callbacks) == AI_RTC_FACADE_OK);
    CHECK(Ai_Rtc_Facade_On_Token_Result(&token) == AI_RTC_FACADE_OK);
    CHECK(!Agora_Rtc_Stub_State()->last_enable_audio_ai_qos);
    Agora_Rtc_Stub_Emit_Joined();
    CHECK(Ai_Rtc_Facade_Stop() == AI_RTC_FACADE_OK);

    token.rtc_token = "token-aiqos";
    token.enable_audio_ai_qos = true;
    CHECK(Ai_Rtc_Facade_On_Token_Result(&token) == AI_RTC_FACADE_OK);
    CHECK(Agora_Rtc_Stub_State()->last_enable_audio_ai_qos);
    Agora_Rtc_Stub_Emit_Joined();
    CHECK(Ai_Rtc_Facade_Stop() == AI_RTC_FACADE_OK);

    Ai_Rtc_Facade_Deinit();
    return 0;
}

static int test_agora_backend_ignores_stale_callbacks(void)
{
    Test_State_t test;
    uint8_t audio_data[4] = {0};
    char rx_msg[] = "late";
    const Ai_Rtc_Facade_Config_t config = {
        .join_timeout_ms = 3000,
        .enable_audio = true,
        .enable_video = false,
    };
    const Ai_Rtc_Facade_Callbacks_t callbacks = {
        .on_state = on_state,
        .on_audio_rx = on_audio_rx,
        .on_datastream_rx = on_datastream_rx,
        .user = &test,
    };
    Ai_Rtc_Facade_Token_Result_t token = {
        .result = 0,
        .rtc_token = "token",
        .channel_name = "channel",
        .app_id = "appid",
        .uid = 9,
    };
    connection_id_t old_conn;
    connection_id_t new_conn;

    memset(&test, 0, sizeof(test));
    Agora_Rtc_Stub_Reset();
    Ai_Rtc_Agora_Port_Stub_Reset();
    CHECK(Ai_Rtc_Facade_Init(&config, &callbacks) == AI_RTC_FACADE_OK);
    CHECK(Ai_Rtc_Facade_On_Token_Result(&token) == AI_RTC_FACADE_OK);
    old_conn = Agora_Rtc_Stub_State()->conn_id;
    Agora_Rtc_Stub_Emit_Joined();
    CHECK(test.joined_events == 1);
    CHECK(Ai_Rtc_Facade_Stop() == AI_RTC_FACADE_OK);
    CHECK(Agora_Rtc_Stub_State()->last_leave_conn_id == old_conn);
    CHECK(Agora_Rtc_Stub_State()->last_destroy_conn_id == old_conn);

    Agora_Rtc_Stub_Emit_Audio_For_Conn(old_conn, audio_data, sizeof(audio_data), AUDIO_DATA_TYPE_OPUS);
    Agora_Rtc_Stub_Emit_Stream_Message_For_Conn(old_conn, 6, 77u, rx_msg, sizeof(rx_msg), 999u);
    Agora_Rtc_Stub_Emit_Token_Will_Expire_For_Conn(old_conn);
    Agora_Rtc_Stub_Emit_User_Joined_For_Conn(old_conn, 44u);
    CHECK(test.audio_rx_calls == 0);
    CHECK(test.datastream_rx_calls == 0);
    CHECK(test.token_will_expire_events == 0);
    CHECK(test.remote_joined_events == 0);
    CHECK(test.failed_events == 0);
    CHECK(Ai_Rtc_Facade_Get_State() == AI_RTC_FACADE_STATE_IDLE);

    token.rtc_token = "token-2";
    CHECK(Ai_Rtc_Facade_On_Token_Result(&token) == AI_RTC_FACADE_OK);
    new_conn = Agora_Rtc_Stub_State()->conn_id;
    CHECK(new_conn != old_conn);
    Agora_Rtc_Stub_Emit_Audio_For_Conn(old_conn, audio_data, sizeof(audio_data), AUDIO_DATA_TYPE_OPUS);
    Agora_Rtc_Stub_Emit_Stream_Message_For_Conn(old_conn, 7, 88u, rx_msg, sizeof(rx_msg), 111u);
    CHECK(test.audio_rx_calls == 0);
    CHECK(test.datastream_rx_calls == 0);

    Agora_Rtc_Stub_Emit_Joined();
    CHECK(test.joined_events == 2);
    Agora_Rtc_Stub_Emit_Audio_For_Conn(new_conn, audio_data, sizeof(audio_data), AUDIO_DATA_TYPE_OPUS);
    CHECK(test.audio_rx_calls == 1);

    CHECK(Ai_Rtc_Facade_Stop() == AI_RTC_FACADE_OK);
    Ai_Rtc_Facade_Deinit();
    return 0;
}

static int test_agora_host_stub_records_rtm_calls_and_emits_callbacks(void)
{
    const uint8_t payload[] = "rtm-json";
    const agora_rtm_handler_t handler = {
        .on_rtm_event = on_rtm_event,
        .on_rtm_data = on_rtm_data,
        .on_rtm_send_data_result = NULL,
    };

    Agora_Rtc_Stub_Reset();
    s_rtm_event_calls = 0;
    s_rtm_data_calls = 0;
    s_last_rtm_event_uid = NULL;
    s_last_rtm_event_type = RTM_EVENT_TYPE_EXIT;
    s_last_rtm_err_code = ERR_RTM_FAILED;
    s_last_rtm_data_uid = NULL;
    s_last_rtm_data = NULL;
    s_last_rtm_data_len = 0;

    CHECK(agora_rtc_login_rtm("local-user", "rtm-token", &handler) == 0);
    CHECK(Agora_Rtc_Stub_State()->login_rtm_calls == 1);
    CHECK(strcmp(Agora_Rtc_Stub_State()->last_rtm_uid, "local-user") == 0);
    CHECK(strcmp(Agora_Rtc_Stub_State()->last_rtm_token, "rtm-token") == 0);

    CHECK(agora_rtc_send_rtm_data("agent-user", payload, sizeof(payload), 7u, NULL) == 0);
    CHECK(Agora_Rtc_Stub_State()->send_rtm_calls == 1);
    CHECK(strcmp(Agora_Rtc_Stub_State()->last_rtm_peer_uid, "agent-user") == 0);
    CHECK(Agora_Rtc_Stub_State()->last_rtm_data == payload);
    CHECK(Agora_Rtc_Stub_State()->last_rtm_len == sizeof(payload));
    CHECK(Agora_Rtc_Stub_State()->last_rtm_msg_id == 7u);

    Agora_Rtc_Stub_Emit_Rtm_Login_Ok();
    CHECK(s_rtm_event_calls == 1);
    CHECK(strcmp(s_last_rtm_event_uid, "local-user") == 0);
    CHECK(s_last_rtm_event_type == RTM_EVENT_TYPE_LOGIN);
    CHECK(s_last_rtm_err_code == ERR_RTM_OK);

    Agora_Rtc_Stub_Emit_Rtm_Data("agent-user", payload, sizeof(payload));
    CHECK(s_rtm_data_calls == 1);
    CHECK(strcmp(s_last_rtm_data_uid, "agent-user") == 0);
    CHECK(s_last_rtm_data == payload);
    CHECK(s_last_rtm_data_len == sizeof(payload));

    CHECK(agora_rtc_logout_rtm() == 0);
    CHECK(Agora_Rtc_Stub_State()->logout_rtm_calls == 1);
    return 0;
}

int main(void)
{
    CHECK(test_agora_backend_wait_joined_polls_until_joined() == 0);
    CHECK(test_private_control_transport_header_is_c_usable() == 0);
    CHECK(test_agora_backend_rejects_stop_while_joining() == 0);
    CHECK(test_agora_backend_flow() == 0);
    CHECK(test_agora_backend_disables_audio_jitter_when_audio_disabled() == 0);
    CHECK(test_agora_backend_maps_per_session_ai_qos_option() == 0);
    CHECK(test_agora_backend_ignores_stale_callbacks() == 0);
    CHECK(test_agora_host_stub_records_rtm_calls_and_emits_callbacks() == 0);
    printf("PASS: ai rtc facade agora backend\n");
    return 0;
}
