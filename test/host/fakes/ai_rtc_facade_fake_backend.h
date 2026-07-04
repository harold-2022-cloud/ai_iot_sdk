#ifndef AI_RTC_FACADE_FAKE_BACKEND_H
#define AI_RTC_FACADE_FAKE_BACKEND_H

#include "ai_rtc_facade_backend.h"

typedef struct
{
    int auto_join;
    int start_result;
    int stop_result;
    int wait_result;
    int send_audio_result;
    int send_datastream_result;
    int send_video_result;
    int start_calls;
    int stop_calls;
    int wait_calls;
    int send_audio_calls;
    int send_datastream_calls;
    int send_video_calls;
    Ai_Rtc_Facade_Backend_Start_Config_t last_start;
    Ai_Rtc_Facade_Audio_Frame_t last_audio;
    Ai_Rtc_Facade_Video_Frame_t last_video;
    const uint8_t *last_datastream_data;
    size_t last_datastream_len;
} Ai_Rtc_Facade_Fake_Backend_State_t;

void Ai_Rtc_Facade_Fake_Backend_Reset(void);
Ai_Rtc_Facade_Fake_Backend_State_t *Ai_Rtc_Facade_Fake_Backend_State(void);
void Ai_Rtc_Facade_Fake_Backend_Emit_Joined(void);
void Ai_Rtc_Facade_Fake_Backend_Emit_Audio(const Ai_Rtc_Facade_Audio_Frame_t *frame);
void Ai_Rtc_Facade_Fake_Backend_Emit_Video(const Ai_Rtc_Facade_Video_Frame_t *frame);
void Ai_Rtc_Facade_Fake_Backend_Emit_Datastream(const Ai_Rtc_Facade_Datastream_Message_t *message);

#endif /* AI_RTC_FACADE_FAKE_BACKEND_H */
