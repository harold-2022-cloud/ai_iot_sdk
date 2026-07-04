#pragma once

#include "ai_audio_if.h"
#include "ai_session_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ai_session_ctx ai_session_handle_t;

#define AI_SESSION_JOIN_MAX_ATTEMPTS_DEFAULT       2U
#define AI_SESSION_JOIN_WAIT_TIMEOUT_MS_DEFAULT    15000U
#define AI_SESSION_JOIN_RETRY_BACKOFF_MS_DEFAULT   1000U
#define AI_SESSION_JOIN_MAX_ATTEMPTS_MIN           1U
#define AI_SESSION_JOIN_MAX_ATTEMPTS_MAX           5U
#define AI_SESSION_JOIN_WAIT_TIMEOUT_MS_MIN        1000U
#define AI_SESSION_JOIN_WAIT_TIMEOUT_MS_MAX        60000U
#define AI_SESSION_JOIN_RETRY_BACKOFF_MS_MAX       10000U
#define AI_SESSION_JOIN_TOTAL_BUDGET_MS_MAX         120000U

typedef struct {
    bool (*request_token)(const ai_session_token_request_t *req, void *user);
    bool (*is_cloud_healthy)(void *user);
    bool (*force_reconnect)(const char *reason, void *user);
    void *user;
} ai_session_cloud_hooks_t;

typedef struct {
    void (*on_event)(ai_session_handle_t *h,
                     ai_session_event_t evt,
                     uint32_t param,
                     void *user);
    bool (*on_start_gate)(ai_session_handle_t *h,
                          ai_session_trigger_t trigger,
                          void *user);
    ai_session_status_t (*on_before_engine_start)(ai_session_handle_t *h,
                                                  const ai_session_rtc_creds_t *creds,
                                                  void *user);
    ai_session_cloud_hooks_t cloud;
    void *user;
} ai_session_hooks_t;

typedef struct {
    uint32_t api_version;
    const ai_session_hooks_t *hooks;
    const ai_audio_io_t *audio;
    const ai_session_rtc_ops_t *rtc;
    ai_session_pcm_format_t pcm;
    ai_session_join_policy_t join;
    ai_session_timer_policy_t timer;
    bool auto_start_timers_on_running;
} ai_session_config_t;

ai_session_status_t Ai_Session_Module_Init(void);
void Ai_Session_Module_Deinit(void);

ai_session_handle_t *Ai_Session_Create(const ai_session_config_t *cfg);
void Ai_Session_Destroy(ai_session_handle_t *h);

ai_session_status_t Ai_Session_Start(ai_session_handle_t *h,
                                     const ai_session_token_request_t *req);
ai_session_status_t Ai_Session_Begin_External_Token_Request(
    ai_session_handle_t *h,
    const ai_session_token_request_t *req);
ai_session_status_t Ai_Session_Start_With_Creds(ai_session_handle_t *h,
                                                const ai_session_rtc_creds_t *creds,
                                                ai_session_trigger_t trigger);
ai_session_status_t Ai_Session_Stop(ai_session_handle_t *h,
                                    ai_session_stop_reason_t reason);
ai_session_status_t Ai_Session_Cancel_Pending(ai_session_handle_t *h);
ai_session_status_t Ai_Session_Request_Rtc_Stop(ai_session_handle_t *h,
                                                ai_session_stop_reason_t reason);

ai_session_status_t Ai_Session_On_Token_Received(ai_session_handle_t *h,
                                                 const ai_session_rtc_creds_t *creds);
ai_session_status_t Ai_Session_On_Token_Failed(ai_session_handle_t *h,
                                               int cloud_result_code);
ai_session_status_t Ai_Session_Accept_Rtc_Start(ai_session_handle_t *h,
                                                uint32_t session_id,
                                                uint32_t generation);
ai_session_status_t Ai_Session_Accept_Rtc_Stop(ai_session_handle_t *h,
                                               uint32_t session_id,
                                               uint32_t generation);
ai_session_status_t Ai_Session_Mark_Rtc_Running(ai_session_handle_t *h,
                                                uint32_t session_id,
                                                uint32_t generation);
ai_session_status_t Ai_Session_Mark_Rtc_Stopped(ai_session_handle_t *h,
                                                uint32_t session_id,
                                                uint32_t generation,
                                                ai_session_stop_reason_t reason);

ai_session_status_t Ai_Session_Observe_Token_Result(
    ai_session_handle_t *h,
    const ai_session_token_result_t *result);
ai_session_status_t Ai_Session_Module_Observe_Token_Result(
    const ai_session_token_result_t *result);

ai_session_state_t Ai_Session_Get_State(const ai_session_handle_t *h);
bool Ai_Session_Is_Active(const ai_session_handle_t *h);
ai_session_dialog_state_t Ai_Session_Get_Dialog_State(const ai_session_handle_t *h);
bool Ai_Session_Get_Diag(const ai_session_handle_t *h,
                         ai_session_diag_t *out);
ai_session_status_t Ai_Session_Wait_Joined(ai_session_handle_t *h,
                                            uint32_t timeout_ms);

void Ai_Session_Set_Rx_Playback_Enabled(ai_session_handle_t *h, bool enabled);
void Ai_Session_Set_Mic_Uplink_Enabled(ai_session_handle_t *h, bool enabled);
void Ai_Session_Set_Weak_Network(ai_session_handle_t *h, bool weak);
ai_session_status_t Ai_Session_Reload_Idle_Timer(ai_session_handle_t *h,
                                                 const char *reason);

void Ai_Session_Config_Default(ai_session_config_t *cfg);
void Ai_Session_Join_Policy_Default(ai_session_join_policy_t *policy);
bool Ai_Session_Join_Policy_Is_Valid(const ai_session_join_policy_t *policy);

#ifdef __cplusplus
}
#endif
