#include "ai_rtc_facade_backend.h"
#include "agora/ai_rtc_agora_service.h"

static int agora_backend_start(const Ai_Rtc_Facade_Backend_Start_Config_t *config)
{
    return Ai_Rtc_Agora_Service_Start(config);
}

static int agora_backend_stop(void)
{
    return Ai_Rtc_Agora_Service_Stop();
}

static int agora_backend_wait_joined(uint32_t timeout_ms)
{
    return Ai_Rtc_Agora_Service_Wait_Joined(timeout_ms);
}

static int agora_backend_send_audio(const Ai_Rtc_Facade_Audio_Frame_t *frame)
{
    return Ai_Rtc_Agora_Service_Send_Audio(frame);
}

static int agora_backend_send_datastream(const uint8_t *data, size_t len)
{
    return Ai_Rtc_Agora_Service_Send_Datastream(data, len);
}

static int agora_backend_send_video(const Ai_Rtc_Facade_Video_Frame_t *frame)
{
    return Ai_Rtc_Agora_Service_Send_Video(frame);
}

static int agora_backend_renew_token(const char *token)
{
    return Ai_Rtc_Agora_Service_Renew_Token(token);
}

static int agora_backend_shutdown(void)
{
    return Ai_Rtc_Agora_Service_Shutdown();
}

static const Ai_Rtc_Facade_Backend_t s_backend = {
    .start = agora_backend_start,
    .stop = agora_backend_stop,
    .wait_joined = agora_backend_wait_joined,
    .send_audio = agora_backend_send_audio,
    .send_datastream = agora_backend_send_datastream,
    .send_video = agora_backend_send_video,
    .renew_token = agora_backend_renew_token,
    .shutdown = agora_backend_shutdown,
};

const Ai_Rtc_Facade_Backend_t *Ai_Rtc_Facade_Get_Backend(void)
{
    return &s_backend;
}
