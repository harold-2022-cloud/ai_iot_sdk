#ifndef AI_RTC_FACADE_H
#define AI_RTC_FACADE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AI_RTC_FACADE_PUBLIC_API_VERSION "2026-07-01-w1"
#define AI_RTC_FACADE_VIDEO_FLAG_KEYFRAME (1u << 0)

typedef enum
{
    AI_RTC_FACADE_OK = 0,
    AI_RTC_FACADE_ERR_INVALID_ARG = -1,
    AI_RTC_FACADE_ERR_NOT_READY = -2,
    AI_RTC_FACADE_ERR_BUSY = -3,
    AI_RTC_FACADE_ERR_UNSUPPORTED = -4,
    AI_RTC_FACADE_ERR_INTERNAL = -5,
} Ai_Rtc_Facade_Result_t;

typedef enum
{
    AI_RTC_FACADE_STATE_IDLE = 0,
    AI_RTC_FACADE_STATE_TOKEN_READY,
    AI_RTC_FACADE_STATE_STARTING,
    AI_RTC_FACADE_STATE_JOINING,
    AI_RTC_FACADE_STATE_JOINED,
    AI_RTC_FACADE_STATE_RECONNECTING,
    AI_RTC_FACADE_STATE_STOPPING,
    AI_RTC_FACADE_STATE_FAILED,
} Ai_Rtc_Facade_State_t;

typedef enum
{
    AI_RTC_FACADE_EVENT_STATE_CHANGED = 0,
    AI_RTC_FACADE_EVENT_JOINED,
    AI_RTC_FACADE_EVENT_RECONNECTING,
    AI_RTC_FACADE_EVENT_REJOINED,
    AI_RTC_FACADE_EVENT_REMOTE_USER_JOINED,
    AI_RTC_FACADE_EVENT_REMOTE_USER_OFFLINE,
    AI_RTC_FACADE_EVENT_TOKEN_WILL_EXPIRE,
    AI_RTC_FACADE_EVENT_TOKEN_EXPIRED,
    AI_RTC_FACADE_EVENT_STOPPED,
    AI_RTC_FACADE_EVENT_FAILED,
} Ai_Rtc_Facade_Event_t;

typedef enum
{
    AI_RTC_FACADE_AUDIO_FORMAT_PCM16 = 0,
    AI_RTC_FACADE_AUDIO_FORMAT_G722,
    AI_RTC_FACADE_AUDIO_FORMAT_OPUS,
} Ai_Rtc_Facade_Audio_Format_t;

typedef enum
{
    AI_RTC_FACADE_VIDEO_FORMAT_H264 = 0,
    AI_RTC_FACADE_VIDEO_FORMAT_JPEG,
} Ai_Rtc_Facade_Video_Format_t;

typedef struct
{
    int result;
    const char *rtc_token;
    const char *channel_name;
    const char *app_id;
    int uid;
    const char *user_account;
    const char *control_peer_id;
    const char *control_token;
    bool enable_audio_ai_qos;
} Ai_Rtc_Facade_Token_Result_t;

typedef struct
{
    const uint8_t *data;
    size_t len;
    Ai_Rtc_Facade_Audio_Format_t format;
    uint32_t sample_rate_hz;
    uint8_t channels;
    uint32_t duration_ms;
    uint64_t timestamp_ms;
} Ai_Rtc_Facade_Audio_Frame_t;

typedef struct
{
    const uint8_t *data;
    size_t len;
    Ai_Rtc_Facade_Video_Format_t format;
    uint32_t width;
    uint32_t height;
    uint32_t frame_rate_hz;
    uint64_t timestamp_ms;
    uint32_t flags;
} Ai_Rtc_Facade_Video_Frame_t;

typedef struct
{
    int stream_id;
    uint32_t sender_uid;
    const uint8_t *data;
    size_t len;
    uint64_t sent_ts;
} Ai_Rtc_Facade_Datastream_Message_t;

typedef void (*Ai_Rtc_Facade_State_Cb)(Ai_Rtc_Facade_Event_t event,
                                       Ai_Rtc_Facade_State_t state,
                                       int detail,
                                       void *user);
typedef int (*Ai_Rtc_Facade_Audio_Rx_Cb)(const Ai_Rtc_Facade_Audio_Frame_t *frame,
                                         void *user);
typedef int (*Ai_Rtc_Facade_Video_Rx_Cb)(const Ai_Rtc_Facade_Video_Frame_t *frame,
                                         void *user);
typedef int (*Ai_Rtc_Facade_Datastream_Rx_Cb)(const Ai_Rtc_Facade_Datastream_Message_t *message,
                                              void *user);

typedef struct
{
    Ai_Rtc_Facade_State_Cb on_state;
    Ai_Rtc_Facade_Audio_Rx_Cb on_audio_rx;
    Ai_Rtc_Facade_Video_Rx_Cb on_video_rx;
    Ai_Rtc_Facade_Datastream_Rx_Cb on_datastream_rx;
    void *user;
} Ai_Rtc_Facade_Callbacks_t;

typedef struct
{
    uint32_t join_timeout_ms;
    bool enable_audio;
    bool enable_video;
} Ai_Rtc_Facade_Config_t;

/* Caller owns config/callback storage after this call returns; implementation copies required values. */
int Ai_Rtc_Facade_Init(const Ai_Rtc_Facade_Config_t *config,
                       const Ai_Rtc_Facade_Callbacks_t *callbacks);

void Ai_Rtc_Facade_Deinit(void);

/* Successful token result triggers the facade-owned token-to-join lifecycle. */
int Ai_Rtc_Facade_On_Token_Result(const Ai_Rtc_Facade_Token_Result_t *result);

/* Stop is idempotent: stopping while idle returns AI_RTC_FACADE_OK. */
int Ai_Rtc_Facade_Stop(void);

Ai_Rtc_Facade_State_t Ai_Rtc_Facade_Get_State(void);
bool Ai_Rtc_Facade_Is_Joined(void);
int Ai_Rtc_Facade_Wait_Joined(uint32_t timeout_ms);

/* TX frame/message data remains caller-owned and only needs to stay valid for the call duration. */
int Ai_Rtc_Facade_Send_Audio(const Ai_Rtc_Facade_Audio_Frame_t *frame);
int Ai_Rtc_Facade_Send_Datastream(const uint8_t *data, size_t len);
int Ai_Rtc_Facade_Send_Video(const Ai_Rtc_Facade_Video_Frame_t *frame);

/* RX callback frame/message data is valid only during the callback. Copy it before returning if it is needed later. */

#ifdef __cplusplus
}
#endif

#endif /* AI_RTC_FACADE_H */
