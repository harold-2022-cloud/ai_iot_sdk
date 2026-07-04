#include "ai_rtc_agora_datastream.h"
#include "base_64.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; } } while (0)

typedef struct
{
    int calls;
    Ai_Rtc_Agora_Datastream_Event_t last;
} Test_State_t;

static void on_event(const Ai_Rtc_Agora_Datastream_Event_t *event, void *user)
{
    Test_State_t *state = (Test_State_t *)user;

    state->calls++;
    state->last = *event;
}

static int make_packet(char *out, size_t out_size, const char *msg_id, unsigned cur, unsigned total, const char *json)
{
    unsigned char encoded[512];
    int encoded_len = 0;

    if (!Base64_Encode((const unsigned char *)json, (int)strlen(json), &encoded_len, encoded))
    {
        return -1;
    }
    while (encoded_len > 0 && (encoded[encoded_len - 1] == '\n' || encoded[encoded_len - 1] == '\0'))
    {
        encoded[--encoded_len] = '\0';
    }
    return snprintf(out, out_size, "%s|%u|%u|%s", msg_id, cur, total, encoded);
}

static int test_state_messages(void)
{
    const struct
    {
        const char *state;
        Ai_Rtc_Agora_Datastream_State_t expected;
    } cases[] = {
        { "listening", AI_RTC_AGORA_DS_STATE_LISTENING },
        { "thinking", AI_RTC_AGORA_DS_STATE_THINKING },
        { "speaking", AI_RTC_AGORA_DS_STATE_SPEAKING },
        { "silent", AI_RTC_AGORA_DS_STATE_SILENT },
        { "unknown-value", AI_RTC_AGORA_DS_STATE_UNKNOWN },
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
    {
        char json[128];
        char packet[256];
        Test_State_t state;
        int packet_len;

        memset(&state, 0, sizeof(state));
        snprintf(json, sizeof(json), "{\"object\":\"message.state\",\"state\":\"%s\"}", cases[i].state);
        packet_len = make_packet(packet, sizeof(packet), "state-msg", 1u, 1u, json);
        CHECK(packet_len > 0);
        CHECK(Ai_Rtc_Agora_Datastream_On_Message((const uint8_t *)packet,
                                                 (size_t)packet_len,
                                                 on_event,
                                                 &state) == 1);
        CHECK(state.calls == 1);
        CHECK(state.last.object == AI_RTC_AGORA_DS_OBJECT_MESSAGE_STATE);
        CHECK(state.last.state == cases[i].expected);
        CHECK(state.last.raw_json_len == strlen(json));
    }
    return 0;
}

static int test_user_message(void)
{
    const char *json = "{\"object\":\"message.user\",\"text\":\"hello\",\"final\":true}";
    char packet[256];
    Test_State_t state;
    int packet_len;

    memset(&state, 0, sizeof(state));
    packet_len = make_packet(packet, sizeof(packet), "user-msg", 1u, 1u, json);
    CHECK(packet_len > 0);
    CHECK(Ai_Rtc_Agora_Datastream_On_Message((const uint8_t *)packet,
                                             (size_t)packet_len,
                                             on_event,
                                             &state) == 1);
    CHECK(state.calls == 1);
    CHECK(state.last.object == AI_RTC_AGORA_DS_OBJECT_MESSAGE_USER);
    CHECK(state.last.raw_json_len == strlen(json));
    return 0;
}

static int test_two_segment_message(void)
{
    const char *json = "{\"object\":\"message.state\",\"state\":\"speaking\"}";
    unsigned char encoded[256];
    char packet1[256];
    char packet2[256];
    Test_State_t state;
    int encoded_len = 0;
    size_t half;

    memset(&state, 0, sizeof(state));
    CHECK(Base64_Encode((const unsigned char *)json, (int)strlen(json), &encoded_len, encoded));
    while (encoded_len > 0 && (encoded[encoded_len - 1] == '\n' || encoded[encoded_len - 1] == '\0'))
    {
        encoded[--encoded_len] = '\0';
    }
    half = (size_t)encoded_len / 2u;
    snprintf(packet1, sizeof(packet1), "split-msg|1|2|%.*s", (int)half, encoded);
    snprintf(packet2, sizeof(packet2), "split-msg|2|2|%s", encoded + half);

    CHECK(Ai_Rtc_Agora_Datastream_On_Message((const uint8_t *)packet1,
                                             strlen(packet1),
                                             on_event,
                                             &state) == 0);
    CHECK(state.calls == 0);
    CHECK(Ai_Rtc_Agora_Datastream_On_Message((const uint8_t *)packet2,
                                             strlen(packet2),
                                             on_event,
                                             &state) == 1);
    CHECK(state.calls == 1);
    CHECK(state.last.object == AI_RTC_AGORA_DS_OBJECT_MESSAGE_STATE);
    CHECK(state.last.state == AI_RTC_AGORA_DS_STATE_SPEAKING);
    return 0;
}

static int test_rejects_too_many_segments(void)
{
    const char *packet = "msg|1|5|AAAA";
    Test_State_t state;

    memset(&state, 0, sizeof(state));
    CHECK(Ai_Rtc_Agora_Datastream_On_Message((const uint8_t *)packet,
                                             strlen(packet),
                                             on_event,
                                             &state) < 0);
    CHECK(state.calls == 0);
    return 0;
}

int main(void)
{
    CHECK(test_state_messages() == 0);
    CHECK(test_user_message() == 0);
    CHECK(test_two_segment_message() == 0);
    CHECK(test_rejects_too_many_segments() == 0);
    printf("PASS: ai rtc agora datastream\n");
    return 0;
}
