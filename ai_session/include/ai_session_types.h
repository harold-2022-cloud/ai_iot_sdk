#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AI_SESSION_API_VERSION_MAJOR 1U
#define AI_SESSION_API_VERSION_MINOR 1U
#define AI_SESSION_API_VERSION \
    ((AI_SESSION_API_VERSION_MAJOR << 16) | AI_SESSION_API_VERSION_MINOR)

typedef enum {
    AI_SESSION_OK = 0,
    AI_SESSION_ERR_ARG = -1,
    AI_SESSION_ERR_STATE = -2,
    AI_SESSION_ERR_BUSY = -3,
    AI_SESSION_ERR_TIMEOUT = -4,
    AI_SESSION_ERR_CLOUD = -5,
    AI_SESSION_ERR_RTC = -6,
    AI_SESSION_ERR_AUDIO = -7,
    AI_SESSION_ERR_CANCELLED = -8,
    AI_SESSION_ERR_NO_MEMORY = -9,
    AI_SESSION_ERR_INTERNAL = -99,
} ai_session_status_t;

typedef enum {
    AI_SESSION_STATE_IDLE = 0,
    AI_SESSION_STATE_TOKEN_PENDING,
    AI_SESSION_STATE_STARTING,
    AI_SESSION_STATE_RUNNING,
    AI_SESSION_STATE_STOPPING,
} ai_session_state_t;

typedef enum {
    AI_SESSION_DIALOG_UNKNOWN = 0,
    AI_SESSION_DIALOG_LISTENING,
    AI_SESSION_DIALOG_THINKING,
    AI_SESSION_DIALOG_SPEAKING,
    AI_SESSION_DIALOG_SILENT,
} ai_session_dialog_state_t;

typedef enum {
    AI_SESSION_TRIGGER_UNKNOWN = 0,
    AI_SESSION_TRIGGER_KEY,
    AI_SESSION_TRIGGER_ASR_WAKEUP,
    AI_SESSION_TRIGGER_CLOUD,
    AI_SESSION_TRIGGER_AUTO,
} ai_session_trigger_t;

typedef enum {
    AI_SESSION_STOP_USER = 0,
    AI_SESSION_STOP_IDLE_TIMEOUT,
    AI_SESSION_STOP_MAX_DURATION,
    AI_SESSION_STOP_TOKEN_FAIL,
    AI_SESSION_STOP_RTC_LOST,
    AI_SESSION_STOP_AGENT_OFFLINE,
    AI_SESSION_STOP_ERROR,
} ai_session_stop_reason_t;

const char *Ai_Session_Stop_Reason_To_String(ai_session_stop_reason_t reason);

typedef enum {
    AI_SESSION_EVT_TOKEN_REQUESTED = 1,
    AI_SESSION_EVT_TOKEN_RECEIVED,
    AI_SESSION_EVT_TOKEN_FAILED,
    AI_SESSION_EVT_STARTING,
    AI_SESSION_EVT_STARTED,
    AI_SESSION_EVT_START_FAILED,
    AI_SESSION_EVT_STOPPING,
    AI_SESSION_EVT_STOPPED,
    AI_SESSION_EVT_CANCELLED,
    AI_SESSION_EVT_RTC_REJOINED,
    AI_SESSION_EVT_RTC_LOST,
    AI_SESSION_EVT_AGENT_JOINED,
    AI_SESSION_EVT_AGENT_OFFLINE,
    AI_SESSION_EVT_AGENT_REMOVED,
    AI_SESSION_EVT_TOKEN_EXPIRED,
    AI_SESSION_EVT_DIALOG_STATE,
    AI_SESSION_EVT_ASR_ACTIVITY,
    AI_SESSION_EVT_FEEDBACK_WAKEUP,
    AI_SESSION_EVT_FEEDBACK_STANDBY,
} ai_session_event_t;

typedef struct {
    const char *app_id;
    const char *channel_name;
    const char *rtc_token;
    uint32_t uid;
} ai_session_rtc_creds_t;

typedef struct {
    uint8_t max_join_attempts;
    uint32_t join_wait_timeout_ms;
    uint32_t join_retry_backoff_ms;
} ai_session_join_policy_t;

typedef struct ai_session_rtc_ops {
    void *ctx;
    ai_session_status_t (*configure)(void *ctx,
                                     const ai_session_rtc_creds_t *creds,
                                     uint32_t session_id,
                                     uint32_t generation);
    ai_session_status_t (*start)(void *ctx,
                                 uint32_t session_id,
                                 uint32_t generation);
    ai_session_status_t (*stop)(void *ctx,
                                uint32_t session_id,
                                uint32_t generation,
                                ai_session_stop_reason_t reason);
    ai_session_status_t (*start_with_policy)(
        void *ctx,
        uint32_t session_id,
        uint32_t generation,
        const ai_session_join_policy_t *join_policy);
} ai_session_rtc_ops_t;

typedef struct {
    ai_session_trigger_t trigger;
    uint32_t session_id;
    const char *persona_id;
    const char *language;
    uint32_t token_timeout_ms;
    uint8_t max_token_retries;
} ai_session_token_request_t;

typedef struct {
    int result;
    const char *rtc_token;
    const char *channel_name;
    const char *app_id;
    int uid;
} ai_session_token_result_t;

typedef struct {
    uint32_t idle_timeout_ms;
    uint32_t max_duration_ms;
    bool idle_pause_while_speaking;
} ai_session_timer_policy_t;

typedef struct {
    uint32_t sample_rate_hz;
    uint8_t channels;
    uint16_t bits_per_sample;
    uint16_t frame_duration_ms;
    uint32_t frame_bytes;
} ai_session_pcm_format_t;

typedef struct {
    ai_session_state_t state;
    ai_session_dialog_state_t dialog_state;
    bool rtc_connected;
    bool agent_online;
    bool token_pending;
    uint32_t session_id;
    uint32_t generation;
    uint16_t lastmile_delay_ms;
    uint16_t rx_audio_kbps;
    uint32_t audio_rx_drop_count;
    uint32_t audio_rx_pool_exhausted;
    int64_t token_request_sent_us;
    int64_t token_received_us;
    int64_t engine_start_us;
    int64_t join_success_us;
    uint32_t token_observe_count;
    uint32_t token_observe_unexpected_count;
    uint32_t last_token_uid;
    int last_token_result;
    uint16_t last_token_len;
    uint16_t last_channel_len;
    uint16_t last_app_id_len;
} ai_session_diag_t;

#ifdef __cplusplus
}
#endif
