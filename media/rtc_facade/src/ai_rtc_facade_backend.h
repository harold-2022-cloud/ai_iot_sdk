#ifndef AI_RTC_FACADE_BACKEND_H
#define AI_RTC_FACADE_BACKEND_H

#include "ai_rtc_facade.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    const char *app_id;
    const char *channel_name;
    const char *rtc_token;
    uint32_t uid;
    const char *user_account;
    const char *control_peer_id;
    const char *control_token;
    bool enable_audio_ai_qos;
    bool enable_audio;
    bool enable_video;
    uint32_t join_timeout_ms;
} Ai_Rtc_Facade_Backend_Start_Config_t;

typedef struct
{
    int (*start)(const Ai_Rtc_Facade_Backend_Start_Config_t *config);
    int (*stop)(void);
    int (*wait_joined)(uint32_t timeout_ms);
    int (*send_audio)(const Ai_Rtc_Facade_Audio_Frame_t *frame);
    int (*send_datastream)(const uint8_t *data, size_t len);
    int (*send_video)(const Ai_Rtc_Facade_Video_Frame_t *frame);
    int (*renew_token)(const char *token);
    int (*shutdown)(void);
} Ai_Rtc_Facade_Backend_t;

const Ai_Rtc_Facade_Backend_t *Ai_Rtc_Facade_Get_Backend(void);

void Ai_Rtc_Facade_Backend_Notify_Joined(int detail);
void Ai_Rtc_Facade_Backend_Notify_Reconnecting(int detail);
void Ai_Rtc_Facade_Backend_Notify_Rejoined(int detail);
void Ai_Rtc_Facade_Backend_Notify_Remote_User_Joined(uint32_t uid);
void Ai_Rtc_Facade_Backend_Notify_Remote_User_Offline(uint32_t uid);
void Ai_Rtc_Facade_Backend_Notify_Token_Will_Expire(void);
void Ai_Rtc_Facade_Backend_Notify_Token_Expired(int detail);
void Ai_Rtc_Facade_Backend_Notify_Failed(int detail);
void Ai_Rtc_Facade_Backend_Notify_Audio_Rx(const Ai_Rtc_Facade_Audio_Frame_t *frame);
void Ai_Rtc_Facade_Backend_Notify_Video_Rx(const Ai_Rtc_Facade_Video_Frame_t *frame);
void Ai_Rtc_Facade_Backend_Notify_Datastream_Rx(const Ai_Rtc_Facade_Datastream_Message_t *message);

#ifdef __cplusplus
}
#endif

#endif /* AI_RTC_FACADE_BACKEND_H */
