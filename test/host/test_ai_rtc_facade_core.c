#include "ai_rtc_facade.h"
#include "ai_rtc_facade_fake_backend.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; } } while (0)

typedef struct
{
    int state_events;
    int joined_events;
    int failed_events;
    int audio_rx_calls;
    int video_rx_calls;
    int datastream_rx_calls;
    Ai_Rtc_Facade_State_t last_state;
    Ai_Rtc_Facade_Video_Frame_t last_video;
    Ai_Rtc_Facade_Datastream_Message_t last_message;
} Test_State_t;

static void on_state(Ai_Rtc_Facade_Event_t event,
                     Ai_Rtc_Facade_State_t state,
                     int detail,
                     void *user)
{
    Test_State_t *test = (Test_State_t *)user;
    (void)detail;
    test->state_events++;
    test->last_state = state;
    if (event == AI_RTC_FACADE_EVENT_JOINED)
    {
        test->joined_events++;
    }
    if (event == AI_RTC_FACADE_EVENT_FAILED)
    {
        test->failed_events++;
    }
}

static int on_audio_rx(const Ai_Rtc_Facade_Audio_Frame_t *frame, void *user)
{
    Test_State_t *test = (Test_State_t *)user;
    CHECK(frame != NULL);
    test->audio_rx_calls++;
    return AI_RTC_FACADE_OK;
}

static int on_video_rx(const Ai_Rtc_Facade_Video_Frame_t *frame, void *user)
{
    Test_State_t *test = (Test_State_t *)user;
    CHECK(frame != NULL);
    test->video_rx_calls++;
    test->last_video = *frame;
    return AI_RTC_FACADE_OK;
}

static int on_datastream_rx(const Ai_Rtc_Facade_Datastream_Message_t *message, void *user)
{
    Test_State_t *test = (Test_State_t *)user;
    CHECK(message != NULL);
    test->datastream_rx_calls++;
    test->last_message = *message;
    return AI_RTC_FACADE_OK;
}

static Ai_Rtc_Facade_Token_Result_t valid_token(void)
{
    Ai_Rtc_Facade_Token_Result_t token = {
        .result = 0,
        .rtc_token = "rtc-token",
        .channel_name = "chan",
        .app_id = "app",
        .uid = 42,
    };
    return token;
}

static int init_facade(Test_State_t *test)
{
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
        .user = test,
    };

    memset(test, 0, sizeof(*test));
    Ai_Rtc_Facade_Fake_Backend_Reset();
    CHECK(Ai_Rtc_Facade_Init(&config, &callbacks) == AI_RTC_FACADE_OK);
    return 0;
}

static int test_token_auto_join_and_send(void)
{
    Test_State_t test;
    uint8_t audio_data[16] = {0};
    uint8_t video_data[5] = {1, 2, 3, 4, 5};
    uint8_t message[] = "hello";
    const Ai_Rtc_Facade_Audio_Frame_t audio = {
        .data = audio_data,
        .len = sizeof(audio_data),
        .format = AI_RTC_FACADE_AUDIO_FORMAT_G722,
        .sample_rate_hz = 16000,
        .channels = 1,
        .duration_ms = 20,
        .timestamp_ms = 9,
    };
    const Ai_Rtc_Facade_Video_Frame_t video = {
        .data = video_data,
        .len = sizeof(video_data),
        .format = AI_RTC_FACADE_VIDEO_FORMAT_H264,
        .width = 640,
        .height = 360,
        .frame_rate_hz = 15,
        .timestamp_ms = 10,
        .flags = AI_RTC_FACADE_VIDEO_FLAG_KEYFRAME,
    };
    Ai_Rtc_Facade_Token_Result_t token = valid_token();

    CHECK(init_facade(&test) == 0);
    Ai_Rtc_Facade_Fake_Backend_State()->auto_join = 1;

    CHECK(Ai_Rtc_Facade_On_Token_Result(&token) == AI_RTC_FACADE_OK);
    CHECK(Ai_Rtc_Facade_Is_Joined());
    CHECK(Ai_Rtc_Facade_Get_State() == AI_RTC_FACADE_STATE_JOINED);
    CHECK(test.joined_events == 1);
    CHECK(Ai_Rtc_Facade_Fake_Backend_State()->start_calls == 1);
    CHECK(strcmp(Ai_Rtc_Facade_Fake_Backend_State()->last_start.app_id, "app") == 0);
    CHECK(strcmp(Ai_Rtc_Facade_Fake_Backend_State()->last_start.channel_name, "chan") == 0);
    CHECK(Ai_Rtc_Facade_Fake_Backend_State()->last_start.user_account == NULL);
    CHECK(Ai_Rtc_Facade_Fake_Backend_State()->last_start.control_peer_id == NULL);
    CHECK(Ai_Rtc_Facade_Fake_Backend_State()->last_start.control_token == NULL);
    CHECK(!Ai_Rtc_Facade_Fake_Backend_State()->last_start.enable_audio_ai_qos);

    CHECK(Ai_Rtc_Facade_Send_Audio(&audio) == AI_RTC_FACADE_OK);
    CHECK(Ai_Rtc_Facade_Fake_Backend_State()->send_audio_calls == 1);
    CHECK(Ai_Rtc_Facade_Fake_Backend_State()->last_audio.len == sizeof(audio_data));

    CHECK(Ai_Rtc_Facade_Send_Datastream(message, sizeof(message)) == AI_RTC_FACADE_OK);
    CHECK(Ai_Rtc_Facade_Fake_Backend_State()->send_datastream_calls == 1);
    CHECK(Ai_Rtc_Facade_Fake_Backend_State()->last_datastream_data == message);
    CHECK(Ai_Rtc_Facade_Fake_Backend_State()->last_datastream_len == sizeof(message));

    CHECK(Ai_Rtc_Facade_Send_Video(&video) == AI_RTC_FACADE_OK);
    CHECK(Ai_Rtc_Facade_Fake_Backend_State()->send_video_calls == 1);
    CHECK(Ai_Rtc_Facade_Fake_Backend_State()->last_video.data == video_data);
    CHECK(Ai_Rtc_Facade_Fake_Backend_State()->last_video.len == sizeof(video_data));
    CHECK(Ai_Rtc_Facade_Fake_Backend_State()->last_video.format == AI_RTC_FACADE_VIDEO_FORMAT_H264);
    CHECK(Ai_Rtc_Facade_Fake_Backend_State()->last_video.frame_rate_hz == 15);
    CHECK(Ai_Rtc_Facade_Fake_Backend_State()->last_video.flags == AI_RTC_FACADE_VIDEO_FLAG_KEYFRAME);
    Ai_Rtc_Facade_Deinit();
    return 0;
}

static int test_token_optional_control_fields_are_passed_to_backend(void)
{
    Test_State_t test;
    Ai_Rtc_Facade_Token_Result_t token = valid_token();

    token.user_account = "local-user";
    token.control_peer_id = "agent-user";
    token.control_token = "control-token";

    CHECK(init_facade(&test) == 0);
    CHECK(Ai_Rtc_Facade_On_Token_Result(&token) == AI_RTC_FACADE_OK);
    CHECK(Ai_Rtc_Facade_Fake_Backend_State()->start_calls == 1);
    CHECK(strcmp(Ai_Rtc_Facade_Fake_Backend_State()->last_start.user_account, "local-user") == 0);
    CHECK(strcmp(Ai_Rtc_Facade_Fake_Backend_State()->last_start.control_peer_id, "agent-user") == 0);
    CHECK(strcmp(Ai_Rtc_Facade_Fake_Backend_State()->last_start.control_token, "control-token") == 0);

    Ai_Rtc_Facade_Deinit();
    return 0;
}

static int test_token_ai_qos_option_is_passed_to_backend(void)
{
    Test_State_t test;
    Ai_Rtc_Facade_Token_Result_t token = valid_token();

    token.enable_audio_ai_qos = true;

    CHECK(init_facade(&test) == 0);
    CHECK(Ai_Rtc_Facade_On_Token_Result(&token) == AI_RTC_FACADE_OK);
    CHECK(Ai_Rtc_Facade_Fake_Backend_State()->start_calls == 1);
    CHECK(Ai_Rtc_Facade_Fake_Backend_State()->last_start.enable_audio_ai_qos);

    Ai_Rtc_Facade_Deinit();
    return 0;
}

static int test_pending_join_gates_tx_and_callbacks(void)
{
    Test_State_t test;
    uint8_t data[4] = {0};
    Ai_Rtc_Facade_Audio_Frame_t audio = {
        .data = data,
        .len = sizeof(data),
        .format = AI_RTC_FACADE_AUDIO_FORMAT_PCM16,
    };
    Ai_Rtc_Facade_Video_Frame_t video = {
        .data = data,
        .len = sizeof(data),
        .format = AI_RTC_FACADE_VIDEO_FORMAT_H264,
        .frame_rate_hz = 15,
        .flags = AI_RTC_FACADE_VIDEO_FLAG_KEYFRAME,
    };
    Ai_Rtc_Facade_Datastream_Message_t message = {
        .stream_id = 7,
        .sender_uid = 88,
        .data = data,
        .len = sizeof(data),
        .sent_ts = 123,
    };

    Ai_Rtc_Facade_Token_Result_t token = valid_token();

    CHECK(init_facade(&test) == 0);
    CHECK(Ai_Rtc_Facade_On_Token_Result(&token) == AI_RTC_FACADE_OK);
    CHECK(!Ai_Rtc_Facade_Is_Joined());
    CHECK(Ai_Rtc_Facade_Send_Audio(&audio) == AI_RTC_FACADE_ERR_NOT_READY);
    CHECK(Ai_Rtc_Facade_Send_Datastream(data, sizeof(data)) == AI_RTC_FACADE_ERR_NOT_READY);
    CHECK(Ai_Rtc_Facade_Send_Video(&video) == AI_RTC_FACADE_ERR_NOT_READY);

    Ai_Rtc_Facade_Fake_Backend_Emit_Audio(&audio);
    Ai_Rtc_Facade_Fake_Backend_Emit_Video(&video);
    Ai_Rtc_Facade_Fake_Backend_Emit_Datastream(&message);
    CHECK(test.audio_rx_calls == 0);
    CHECK(test.video_rx_calls == 0);
    CHECK(test.datastream_rx_calls == 0);

    Ai_Rtc_Facade_Fake_Backend_Emit_Joined();
    CHECK(Ai_Rtc_Facade_Is_Joined());
    Ai_Rtc_Facade_Fake_Backend_Emit_Audio(&audio);
    Ai_Rtc_Facade_Fake_Backend_Emit_Video(&video);
    Ai_Rtc_Facade_Fake_Backend_Emit_Datastream(&message);
    CHECK(test.audio_rx_calls == 1);
    CHECK(test.video_rx_calls == 1);
    CHECK(test.datastream_rx_calls == 1);
    CHECK(test.last_video.data == data);
    CHECK(test.last_video.len == sizeof(data));
    CHECK(test.last_video.frame_rate_hz == 15);
    CHECK(test.last_video.flags == AI_RTC_FACADE_VIDEO_FLAG_KEYFRAME);
    CHECK(test.last_message.stream_id == 7);
    CHECK(test.last_message.sender_uid == 88);

    Ai_Rtc_Facade_Deinit();
    return 0;
}

static int test_failures_and_stop(void)
{
    Test_State_t test;
    Ai_Rtc_Facade_Token_Result_t token = valid_token();

    CHECK(init_facade(&test) == 0);
    token.result = -99;
    CHECK(Ai_Rtc_Facade_On_Token_Result(&token) == AI_RTC_FACADE_ERR_INTERNAL);
    CHECK(test.failed_events == 1);
    CHECK(Ai_Rtc_Facade_Fake_Backend_State()->start_calls == 0);
    CHECK(Ai_Rtc_Facade_Stop() == AI_RTC_FACADE_OK);

    Ai_Rtc_Facade_Deinit();
    CHECK(init_facade(&test) == 0);
    Ai_Rtc_Facade_Fake_Backend_State()->start_result = AI_RTC_FACADE_ERR_INTERNAL;
    token = valid_token();
    CHECK(Ai_Rtc_Facade_On_Token_Result(&token) == AI_RTC_FACADE_ERR_INTERNAL);
    CHECK(test.failed_events == 1);
    CHECK(Ai_Rtc_Facade_Get_State() == AI_RTC_FACADE_STATE_FAILED);
    CHECK(Ai_Rtc_Facade_Stop() == AI_RTC_FACADE_OK);
    CHECK(Ai_Rtc_Facade_Fake_Backend_State()->stop_calls == 1);

    Ai_Rtc_Facade_Deinit();
    return 0;
}

int main(void)
{
    CHECK(test_token_auto_join_and_send() == 0);
    CHECK(test_token_optional_control_fields_are_passed_to_backend() == 0);
    CHECK(test_token_ai_qos_option_is_passed_to_backend() == 0);
    CHECK(test_pending_join_gates_tx_and_callbacks() == 0);
    CHECK(test_failures_and_stop() == 0);
    printf("PASS: ai rtc facade core\n");
    return 0;
}
