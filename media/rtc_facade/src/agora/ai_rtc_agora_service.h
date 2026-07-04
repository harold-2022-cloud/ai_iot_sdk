#ifndef AI_RTC_AGORA_SERVICE_H
#define AI_RTC_AGORA_SERVICE_H

#include "ai_rtc_facade_backend.h"

#ifdef __cplusplus
extern "C" {
#endif

int Ai_Rtc_Agora_Service_Start(const Ai_Rtc_Facade_Backend_Start_Config_t *config);
int Ai_Rtc_Agora_Service_Stop(void);
int Ai_Rtc_Agora_Service_Wait_Joined(uint32_t timeout_ms);
int Ai_Rtc_Agora_Service_Send_Audio(const Ai_Rtc_Facade_Audio_Frame_t *frame);
int Ai_Rtc_Agora_Service_Send_Video(const Ai_Rtc_Facade_Video_Frame_t *frame);
int Ai_Rtc_Agora_Service_Send_Datastream(const uint8_t *data, size_t len);
int Ai_Rtc_Agora_Service_Renew_Token(const char *token);
int Ai_Rtc_Agora_Service_Shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* AI_RTC_AGORA_SERVICE_H */
