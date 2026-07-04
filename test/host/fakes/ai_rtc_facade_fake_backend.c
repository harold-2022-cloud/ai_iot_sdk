#include "ai_rtc_facade_fake_backend.h"

#include <string.h>

static Ai_Rtc_Facade_Fake_Backend_State_t s_fake;

static int fake_start(const Ai_Rtc_Facade_Backend_Start_Config_t *config)
{
    s_fake.start_calls++;
    if (config != NULL)
    {
        s_fake.last_start = *config;
    }
    if (s_fake.start_result == AI_RTC_FACADE_OK && s_fake.auto_join)
    {
        Ai_Rtc_Facade_Backend_Notify_Joined(0);
    }
    return s_fake.start_result;
}

static int fake_stop(void)
{
    s_fake.stop_calls++;
    return s_fake.stop_result;
}

static int fake_wait_joined(uint32_t timeout_ms)
{
    (void)timeout_ms;
    s_fake.wait_calls++;
    return s_fake.wait_result;
}

static int fake_send_audio(const Ai_Rtc_Facade_Audio_Frame_t *frame)
{
    s_fake.send_audio_calls++;
    if (frame != NULL)
    {
        s_fake.last_audio = *frame;
    }
    return s_fake.send_audio_result;
}

static int fake_send_datastream(const uint8_t *data, size_t len)
{
    s_fake.send_datastream_calls++;
    s_fake.last_datastream_data = data;
    s_fake.last_datastream_len = len;
    return s_fake.send_datastream_result;
}

static int fake_send_video(const Ai_Rtc_Facade_Video_Frame_t *frame)
{
    s_fake.send_video_calls++;
    if (frame != NULL)
    {
        s_fake.last_video = *frame;
    }
    return s_fake.send_video_result;
}

static int fake_renew_token(const char *token)
{
    (void)token;
    return AI_RTC_FACADE_OK;
}

static const Ai_Rtc_Facade_Backend_t s_backend = {
    .start = fake_start,
    .stop = fake_stop,
    .wait_joined = fake_wait_joined,
    .send_audio = fake_send_audio,
    .send_datastream = fake_send_datastream,
    .send_video = fake_send_video,
    .renew_token = fake_renew_token,
};

const Ai_Rtc_Facade_Backend_t *Ai_Rtc_Facade_Get_Backend(void)
{
    return &s_backend;
}

void Ai_Rtc_Facade_Fake_Backend_Reset(void)
{
    memset(&s_fake, 0, sizeof(s_fake));
    s_fake.start_result = AI_RTC_FACADE_OK;
    s_fake.stop_result = AI_RTC_FACADE_OK;
    s_fake.wait_result = AI_RTC_FACADE_ERR_NOT_READY;
    s_fake.send_audio_result = AI_RTC_FACADE_OK;
    s_fake.send_datastream_result = AI_RTC_FACADE_OK;
    s_fake.send_video_result = AI_RTC_FACADE_OK;
}

Ai_Rtc_Facade_Fake_Backend_State_t *Ai_Rtc_Facade_Fake_Backend_State(void)
{
    return &s_fake;
}

void Ai_Rtc_Facade_Fake_Backend_Emit_Joined(void)
{
    Ai_Rtc_Facade_Backend_Notify_Joined(0);
}

void Ai_Rtc_Facade_Fake_Backend_Emit_Audio(const Ai_Rtc_Facade_Audio_Frame_t *frame)
{
    Ai_Rtc_Facade_Backend_Notify_Audio_Rx(frame);
}

void Ai_Rtc_Facade_Fake_Backend_Emit_Video(const Ai_Rtc_Facade_Video_Frame_t *frame)
{
    Ai_Rtc_Facade_Backend_Notify_Video_Rx(frame);
}

void Ai_Rtc_Facade_Fake_Backend_Emit_Datastream(const Ai_Rtc_Facade_Datastream_Message_t *message)
{
    Ai_Rtc_Facade_Backend_Notify_Datastream_Rx(message);
}
