#ifndef AI_RTC_AGORA_DATASTREAM_H
#define AI_RTC_AGORA_DATASTREAM_H

#include <stddef.h>
#include <stdint.h>

typedef enum
{
    AI_RTC_AGORA_DS_OBJECT_UNKNOWN = 0,
    AI_RTC_AGORA_DS_OBJECT_MESSAGE_USER,
    AI_RTC_AGORA_DS_OBJECT_MESSAGE_STATE,
} Ai_Rtc_Agora_Datastream_Object_t;

typedef enum
{
    AI_RTC_AGORA_DS_STATE_UNKNOWN = 0,
    AI_RTC_AGORA_DS_STATE_LISTENING,
    AI_RTC_AGORA_DS_STATE_THINKING,
    AI_RTC_AGORA_DS_STATE_SPEAKING,
    AI_RTC_AGORA_DS_STATE_SILENT,
} Ai_Rtc_Agora_Datastream_State_t;

typedef struct
{
    Ai_Rtc_Agora_Datastream_Object_t object;
    Ai_Rtc_Agora_Datastream_State_t state;
    const char *raw_json;
    size_t raw_json_len;
} Ai_Rtc_Agora_Datastream_Event_t;

typedef void (*Ai_Rtc_Agora_Datastream_Event_Cb)(const Ai_Rtc_Agora_Datastream_Event_t *event,
                                                 void *user);

void Ai_Rtc_Agora_Datastream_Reset(void);
int Ai_Rtc_Agora_Datastream_On_Message(const uint8_t *data,
                                       size_t len,
                                       Ai_Rtc_Agora_Datastream_Event_Cb cb,
                                       void *user);

#endif /* AI_RTC_AGORA_DATASTREAM_H */
