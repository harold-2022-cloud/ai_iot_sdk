#ifndef AI_RTC_AGORA_CONTROL_TRANSPORT_H
#define AI_RTC_AGORA_CONTROL_TRANSPORT_H

#include "agora_rtc_api.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AI_RTC_AGORA_CONTROL_TRANSPORT_DATASTREAM 1
#define AI_RTC_AGORA_CONTROL_TRANSPORT_RTM 2

#ifndef AI_RTC_AGORA_CONTROL_TRANSPORT
#define AI_RTC_AGORA_CONTROL_TRANSPORT AI_RTC_AGORA_CONTROL_TRANSPORT_DATASTREAM
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    connection_id_t conn_id;
    const char *rtc_token;
    const char *user_account;
    const char *control_peer_id;
    const char *control_token;
} Ai_Rtc_Agora_Control_Config_t;

typedef struct
{
    int stream_id;
    bool rtm_logged_in;
    bool rtm_login_started;
    uint32_t next_rtm_msg_id;
    const char *control_peer_id;
} Ai_Rtc_Agora_Control_State_t;

void Ai_Rtc_Agora_Control_Reset(Ai_Rtc_Agora_Control_State_t *state);
int Ai_Rtc_Agora_Control_Start(Ai_Rtc_Agora_Control_State_t *state,
                               const Ai_Rtc_Agora_Control_Config_t *config);
int Ai_Rtc_Agora_Control_On_Rtc_Joined(Ai_Rtc_Agora_Control_State_t *state,
                                       connection_id_t conn_id);
int Ai_Rtc_Agora_Control_Stop(Ai_Rtc_Agora_Control_State_t *state);
bool Ai_Rtc_Agora_Control_Is_Ready(const Ai_Rtc_Agora_Control_State_t *state);
int Ai_Rtc_Agora_Control_Send(Ai_Rtc_Agora_Control_State_t *state,
                              connection_id_t conn_id,
                              const uint8_t *data,
                              size_t len);

#ifdef __cplusplus
}
#endif

#endif /* AI_RTC_AGORA_CONTROL_TRANSPORT_H */
