#include "ai_rtc_facade.h"
#include "agora_rtc_api.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; } } while (0)

typedef struct
{
    int datastream_rx_calls;
    Ai_Rtc_Facade_Datastream_Message_t last_message;
} Test_State_t;

void Ai_Rtc_Agora_Port_Stub_Reset(void);

static int on_datastream_rx(const Ai_Rtc_Facade_Datastream_Message_t *message, void *user)
{
    Test_State_t *test = (Test_State_t *)user;
    CHECK(message != NULL);
    test->datastream_rx_calls++;
    test->last_message = *message;
    return AI_RTC_FACADE_OK;
}

static int test_rtm_private_transport_flow(void)
{
    Test_State_t test;
    uint8_t tx_msg[] = "hello";
    uint8_t rx_msg[] = "rx-json";
    const Ai_Rtc_Facade_Config_t config = {
        .join_timeout_ms = 3000,
        .enable_audio = true,
        .enable_video = false,
    };
    const Ai_Rtc_Facade_Callbacks_t callbacks = {
        .on_datastream_rx = on_datastream_rx,
        .user = &test,
    };
    const Ai_Rtc_Facade_Token_Result_t token = {
        .result = 0,
        .rtc_token = "rtc-token",
        .channel_name = "channel",
        .app_id = "appid",
        .uid = 9,
        .user_account = "local-user",
        .control_peer_id = "agent-user",
        .control_token = "rtm-token",
    };

    memset(&test, 0, sizeof(test));
    Agora_Rtc_Stub_Reset();
    Ai_Rtc_Agora_Port_Stub_Reset();

    CHECK(Ai_Rtc_Facade_Init(&config, &callbacks) == AI_RTC_FACADE_OK);
    CHECK(Ai_Rtc_Facade_On_Token_Result(&token) == AI_RTC_FACADE_OK);
    CHECK(Agora_Rtc_Stub_State()->login_rtm_calls == 1);
    CHECK(strcmp(Agora_Rtc_Stub_State()->last_rtm_uid, "local-user") == 0);
    CHECK(strcmp(Agora_Rtc_Stub_State()->last_rtm_token, "rtm-token") == 0);
    CHECK(Agora_Rtc_Stub_State()->join_channel_with_user_account_calls == 1);
    CHECK(Agora_Rtc_Stub_State()->join_channel_calls == 0);
    CHECK(strcmp(Agora_Rtc_Stub_State()->last_user_account, "local-user") == 0);
    CHECK(strcmp(Agora_Rtc_Stub_State()->last_channel_name, "channel") == 0);
    CHECK(strcmp(Agora_Rtc_Stub_State()->last_token, "rtc-token") == 0);
    CHECK(Agora_Rtc_Stub_State()->create_data_stream_calls == 0);

    Agora_Rtc_Stub_Emit_Joined();
    CHECK(Ai_Rtc_Facade_Is_Joined());
    CHECK(Ai_Rtc_Facade_Send_Datastream(tx_msg, sizeof(tx_msg)) == AI_RTC_FACADE_ERR_NOT_READY);
    CHECK(Agora_Rtc_Stub_State()->send_rtm_calls == 0);

    Agora_Rtc_Stub_Emit_Rtm_Login_Ok();
    CHECK(Ai_Rtc_Facade_Send_Datastream(tx_msg, sizeof(tx_msg)) == AI_RTC_FACADE_OK);
    CHECK(Agora_Rtc_Stub_State()->send_rtm_calls == 1);
    CHECK(strcmp(Agora_Rtc_Stub_State()->last_rtm_peer_uid, "agent-user") == 0);
    CHECK(Agora_Rtc_Stub_State()->last_rtm_data == tx_msg);
    CHECK(Agora_Rtc_Stub_State()->last_rtm_len == sizeof(tx_msg));
    CHECK(Agora_Rtc_Stub_State()->last_rtm_msg_id == 1u);

    Agora_Rtc_Stub_Emit_Rtm_Data("agent-user", rx_msg, sizeof(rx_msg));
    CHECK(test.datastream_rx_calls == 1);
    CHECK(test.last_message.stream_id == -1);
    CHECK(test.last_message.sender_uid == 0u);
    CHECK(test.last_message.data == rx_msg);
    CHECK(test.last_message.len == sizeof(rx_msg));
    CHECK(test.last_message.sent_ts == 0u);

    CHECK(Ai_Rtc_Facade_Stop() == AI_RTC_FACADE_OK);
    CHECK(Agora_Rtc_Stub_State()->logout_rtm_calls == 1);
    Ai_Rtc_Facade_Deinit();
    return 0;
}

static int test_rtm_private_transport_requires_user_and_peer(void)
{
    Test_State_t test;
    const Ai_Rtc_Facade_Config_t config = {
        .join_timeout_ms = 3000,
        .enable_audio = true,
        .enable_video = false,
    };
    const Ai_Rtc_Facade_Callbacks_t callbacks = {
        .on_datastream_rx = on_datastream_rx,
        .user = &test,
    };
    Ai_Rtc_Facade_Token_Result_t token = {
        .result = 0,
        .rtc_token = "rtc-token",
        .channel_name = "channel",
        .app_id = "appid",
        .uid = 9,
        .user_account = "local-user",
        .control_peer_id = "agent-user",
    };

    memset(&test, 0, sizeof(test));
    Agora_Rtc_Stub_Reset();
    Ai_Rtc_Agora_Port_Stub_Reset();
    CHECK(Ai_Rtc_Facade_Init(&config, &callbacks) == AI_RTC_FACADE_OK);

    token.user_account = NULL;
    CHECK(Ai_Rtc_Facade_On_Token_Result(&token) == AI_RTC_FACADE_ERR_INVALID_ARG);
    CHECK(Agora_Rtc_Stub_State()->login_rtm_calls == 0);

    Ai_Rtc_Facade_Deinit();
    CHECK(Ai_Rtc_Facade_Init(&config, &callbacks) == AI_RTC_FACADE_OK);
    token.user_account = "local-user";
    token.control_peer_id = NULL;
    CHECK(Ai_Rtc_Facade_On_Token_Result(&token) == AI_RTC_FACADE_ERR_INVALID_ARG);
    CHECK(Agora_Rtc_Stub_State()->login_rtm_calls == 0);

    Ai_Rtc_Facade_Deinit();
    return 0;
}

int main(void)
{
    CHECK(test_rtm_private_transport_flow() == 0);
    CHECK(test_rtm_private_transport_requires_user_and_peer() == 0);
    printf("PASS: ai rtc facade agora backend rtm\n");
    return 0;
}
