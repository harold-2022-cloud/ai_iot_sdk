#include "ai_session.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdlib.h>
#include <string.h>

#define AI_SESSION_TAG "AI_SESSION"
#define AI_SESSION_TOKEN_OBSERVE_TOKEN_MAX 2048U
#define AI_SESSION_TOKEN_OBSERVE_FIELD_MAX 256U

typedef struct {
    uint32_t observe_count;
    uint32_t last_uid;
    int last_result;
    uint16_t last_token_len;
    uint16_t last_channel_len;
    uint16_t last_app_id_len;
} ai_session_token_observe_stats_t;

struct ai_session_ctx {
    portMUX_TYPE lock;
    ai_session_config_t cfg;
    ai_session_state_t state;
    ai_session_dialog_state_t dialog_state;
    ai_session_diag_t diag;
    bool weak_network;
    bool cancel_requested;
    bool rtc_start_requested;
    bool rtc_stop_requested;
};

static portMUX_TYPE s_module_lock = portMUX_INITIALIZER_UNLOCKED;
static bool s_module_initialized;
static ai_session_token_observe_stats_t s_module_token_stats;


static uint16_t ai_session_strnlen_u16(const char *s, size_t max_len)
{
    size_t len = 0U;

    if (s == NULL)
    {
        return 0U;
    }

    while ((len < max_len) && (s[len] != '\0'))
    {
        len++;
    }

    return (uint16_t)len;
}

static bool ai_session_state_expects_token(ai_session_state_t state)
{
    return (state == AI_SESSION_STATE_TOKEN_PENDING) ||
           (state == AI_SESSION_STATE_STARTING);
}

static const char *ai_session_state_str(ai_session_state_t state)
{
    switch (state)
    {
        case AI_SESSION_STATE_IDLE: return "IDLE";
        case AI_SESSION_STATE_TOKEN_PENDING: return "TOKEN_PENDING";
        case AI_SESSION_STATE_STARTING: return "STARTING";
        case AI_SESSION_STATE_RUNNING: return "RUNNING";
        case AI_SESSION_STATE_STOPPING: return "STOPPING";
        default: return "UNKNOWN";
    }
}

const char *Ai_Session_Stop_Reason_To_String(ai_session_stop_reason_t reason)
{
    switch (reason)
    {
        case AI_SESSION_STOP_USER: return "user";
        case AI_SESSION_STOP_IDLE_TIMEOUT: return "idle_timeout";
        case AI_SESSION_STOP_MAX_DURATION: return "max_duration";
        case AI_SESSION_STOP_TOKEN_FAIL: return "token_fail";
        case AI_SESSION_STOP_RTC_LOST: return "rtc_lost";
        case AI_SESSION_STOP_AGENT_OFFLINE: return "agent_offline";
        case AI_SESSION_STOP_ERROR: return "error";
        default: return "unknown";
    }
}

static void ai_session_log_token_observe(const char *scope,
                                         const ai_session_token_result_t *result,
                                         ai_session_state_t state,
                                         bool expected,
                                         uint16_t token_len,
                                         uint16_t channel_len,
                                         uint16_t app_id_len)
{
    if (result->result == 0)
    {
        ESP_LOGI(AI_SESSION_TAG,
                 "[AI_SESSION][TOKEN_OBSERVE] scope=%s mode=observe_only result=%d uid=%d token_len=%u channel_len=%u app_id_len=%u state=%d expected=%d",
                 scope,
                 result->result,
                 result->uid,
                 (unsigned int)token_len,
                 (unsigned int)channel_len,
                 (unsigned int)app_id_len,
                 (int)state,
                 expected ? 1 : 0);
    }
    else
    {
        ESP_LOGW(AI_SESSION_TAG,
                 "[AI_SESSION][TOKEN_OBSERVE_FAIL] scope=%s mode=observe_only result=%d uid=%d state=%d expected=%d",
                 scope,
                 result->result,
                 result->uid,
                 (int)state,
                 expected ? 1 : 0);
    }
}

static bool ai_session_version_supported(uint32_t version)
{
    uint32_t major = version >> 16;
    return (major == AI_SESSION_API_VERSION_MAJOR);
}

static bool ai_session_version_supports_join_policy(uint32_t version)
{
    uint32_t major = version >> 16;
    uint32_t minor = version & 0xffffU;
    return (major == AI_SESSION_API_VERSION_MAJOR) && (minor >= 1U);
}

static void ai_session_emit(ai_session_handle_t *h, ai_session_event_t evt, uint32_t param)
{
    const ai_session_hooks_t *hooks = h->cfg.hooks;

    if ((hooks != NULL) && (hooks->on_event != NULL))
    {
        hooks->on_event(h, evt, param, hooks->user);
    }
}

static void ai_session_lock(ai_session_handle_t *h)
{
    taskENTER_CRITICAL(&h->lock);
}

static void ai_session_unlock(ai_session_handle_t *h)
{
    taskEXIT_CRITICAL(&h->lock);
}

const ai_audio_io_t *Ai_Audio_Io_Get_Default(void)
{
    return NULL;
}

ai_session_status_t Ai_Session_Module_Init(void)
{
    taskENTER_CRITICAL(&s_module_lock);
    s_module_initialized = true;
    taskEXIT_CRITICAL(&s_module_lock);
    return AI_SESSION_OK;
}

void Ai_Session_Module_Deinit(void)
{
    taskENTER_CRITICAL(&s_module_lock);
    s_module_initialized = false;
    taskEXIT_CRITICAL(&s_module_lock);
}

void Ai_Session_Join_Policy_Default(ai_session_join_policy_t *policy)
{
    if (policy == NULL)
    {
        return;
    }

    policy->max_join_attempts = AI_SESSION_JOIN_MAX_ATTEMPTS_DEFAULT;
    policy->join_wait_timeout_ms = AI_SESSION_JOIN_WAIT_TIMEOUT_MS_DEFAULT;
    policy->join_retry_backoff_ms = AI_SESSION_JOIN_RETRY_BACKOFF_MS_DEFAULT;
}

bool Ai_Session_Join_Policy_Is_Valid(const ai_session_join_policy_t *policy)
{
    uint64_t total_budget_ms;

    if (policy == NULL)
    {
        return false;
    }

    if ((policy->max_join_attempts < AI_SESSION_JOIN_MAX_ATTEMPTS_MIN) ||
        (policy->max_join_attempts > AI_SESSION_JOIN_MAX_ATTEMPTS_MAX) ||
        (policy->join_wait_timeout_ms < AI_SESSION_JOIN_WAIT_TIMEOUT_MS_MIN) ||
        (policy->join_wait_timeout_ms > AI_SESSION_JOIN_WAIT_TIMEOUT_MS_MAX) ||
        (policy->join_retry_backoff_ms > AI_SESSION_JOIN_RETRY_BACKOFF_MS_MAX))
    {
        return false;
    }

    total_budget_ms =
        ((uint64_t)policy->max_join_attempts * policy->join_wait_timeout_ms) +
        ((uint64_t)(policy->max_join_attempts - 1U) * policy->join_retry_backoff_ms);
    return total_budget_ms <= AI_SESSION_JOIN_TOTAL_BUDGET_MS_MAX;
}

void Ai_Session_Config_Default(ai_session_config_t *cfg)
{
    if (cfg == NULL)
    {
        return;
    }

    memset(cfg, 0, sizeof(*cfg));
    cfg->api_version = AI_SESSION_API_VERSION;
    cfg->audio = Ai_Audio_Io_Get_Default();
    /* 中立默认 PCM(16k/mono/16-bit/20ms = 640B);板相关覆盖由调用方填 cfg->pcm */
    cfg->pcm.sample_rate_hz = 16000U;
    cfg->pcm.channels = 1U;
    cfg->pcm.bits_per_sample = 16U;
    cfg->pcm.frame_duration_ms = 20U;
    cfg->pcm.frame_bytes = 640U;
    Ai_Session_Join_Policy_Default(&cfg->join);
    cfg->timer.idle_timeout_ms = 120000U;
    cfg->timer.max_duration_ms = 1800000U;
    cfg->timer.idle_pause_while_speaking = true;
    cfg->auto_start_timers_on_running = true;
}

ai_session_handle_t *Ai_Session_Create(const ai_session_config_t *cfg)
{
    bool initialized;

    if ((cfg == NULL) || !ai_session_version_supported(cfg->api_version) ||
        (cfg->hooks == NULL))
    {
        return NULL;
    }

    taskENTER_CRITICAL(&s_module_lock);
    initialized = s_module_initialized;
    taskEXIT_CRITICAL(&s_module_lock);
    if (!initialized)
    {
        return NULL;
    }

    ai_session_handle_t *h = (ai_session_handle_t *)calloc(1, sizeof(*h));
    if (h == NULL)
    {
        return NULL;
    }

    portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
    h->lock = lock;
    h->cfg = *cfg;
    if (!Ai_Session_Join_Policy_Is_Valid(&h->cfg.join))
    {
        ESP_LOGW(AI_SESSION_TAG,
                 "[AI_SESSION][JOIN_POLICY_FALLBACK] attempts=%u timeout_ms=%lu backoff_ms=%lu "
                 "fallback_attempts=%u fallback_timeout_ms=%u fallback_backoff_ms=%u",
                 (unsigned int)h->cfg.join.max_join_attempts,
                 (unsigned long)h->cfg.join.join_wait_timeout_ms,
                 (unsigned long)h->cfg.join.join_retry_backoff_ms,
                 (unsigned int)AI_SESSION_JOIN_MAX_ATTEMPTS_DEFAULT,
                 (unsigned int)AI_SESSION_JOIN_WAIT_TIMEOUT_MS_DEFAULT,
                 (unsigned int)AI_SESSION_JOIN_RETRY_BACKOFF_MS_DEFAULT);
        Ai_Session_Join_Policy_Default(&h->cfg.join);
    }
    h->state = AI_SESSION_STATE_IDLE;
    h->dialog_state = AI_SESSION_DIALOG_UNKNOWN;
    h->diag.state = h->state;
    h->diag.dialog_state = h->dialog_state;
    return h;
}

void Ai_Session_Destroy(ai_session_handle_t *h)
{
    if (h == NULL)
    {
        return;
    }
    free(h);
}

ai_session_status_t Ai_Session_Start(ai_session_handle_t *h,
                                     const ai_session_token_request_t *req)
{
    ai_session_token_request_t safe_req;
    uint32_t generation;
    bool queued;
    bool rollback;

    if ((h == NULL) || (req == NULL) ||
        (h->cfg.hooks == NULL) || (h->cfg.hooks->cloud.request_token == NULL))
    {
        return AI_SESSION_ERR_ARG;
    }

    if ((h->cfg.hooks->on_start_gate != NULL) &&
        !h->cfg.hooks->on_start_gate(h, req->trigger, h->cfg.hooks->user))
    {
        return AI_SESSION_ERR_STATE;
    }

    ai_session_lock(h);
    if (h->state != AI_SESSION_STATE_IDLE)
    {
        ai_session_unlock(h);
        return AI_SESSION_ERR_BUSY;
    }
    if (h->cancel_requested && (h->diag.session_id == req->session_id))
    {
        ai_session_unlock(h);
        ESP_LOGW(AI_SESSION_TAG,
                 "[AI_SESSION][EXTERNAL_TOKEN_BEGIN_DROP] sid=%lu reason=cancelled_same_generation",
                 (unsigned long)req->session_id);
        return AI_SESSION_ERR_CANCELLED;
    }
    h->state = AI_SESSION_STATE_TOKEN_PENDING;
    h->diag.state = h->state;
    h->diag.token_pending = true;
    h->diag.session_id = req->session_id;
    h->diag.generation++;
    generation = h->diag.generation;
    h->cancel_requested = false;
    h->rtc_start_requested = false;
    h->rtc_stop_requested = false;
    safe_req = *req;
    ai_session_unlock(h);

    ai_session_emit(h, AI_SESSION_EVT_TOKEN_REQUESTED, safe_req.session_id);
    queued = h->cfg.hooks->cloud.request_token(&safe_req, h->cfg.hooks->cloud.user);
    if (!queued)
    {
        ai_session_lock(h);
        rollback = (h->state == AI_SESSION_STATE_TOKEN_PENDING) &&
                   (h->diag.session_id == safe_req.session_id) &&
                   (h->diag.generation == generation);
        if (rollback)
        {
            h->state = AI_SESSION_STATE_IDLE;
            h->diag.state = h->state;
            h->diag.token_pending = false;
            h->cancel_requested = false;
            h->rtc_start_requested = false;
            h->rtc_stop_requested = false;
        }
        ai_session_unlock(h);
        if (rollback)
        {
            ai_session_emit(h, AI_SESSION_EVT_TOKEN_FAILED, 0U);
        }
        return AI_SESSION_ERR_CLOUD;
    }

    return AI_SESSION_OK;
}

ai_session_status_t Ai_Session_Begin_External_Token_Request(
    ai_session_handle_t *h,
    const ai_session_token_request_t *req)
{
    ai_session_token_request_t safe_req;
    uint32_t generation;

    if ((h == NULL) || (req == NULL))
    {
        return AI_SESSION_ERR_ARG;
    }

    if ((h->cfg.hooks != NULL) && (h->cfg.hooks->on_start_gate != NULL) &&
        !h->cfg.hooks->on_start_gate(h, req->trigger, h->cfg.hooks->user))
    {
        return AI_SESSION_ERR_STATE;
    }

    ai_session_lock(h);
    if (h->state != AI_SESSION_STATE_IDLE)
    {
        ai_session_unlock(h);
        return AI_SESSION_ERR_BUSY;
    }
    if (h->cancel_requested && (h->diag.session_id == req->session_id))
    {
        ai_session_unlock(h);
        ESP_LOGW(AI_SESSION_TAG,
                 "[AI_SESSION][EXTERNAL_TOKEN_BEGIN_DROP] sid=%lu reason=cancelled_same_generation",
                 (unsigned long)req->session_id);
        return AI_SESSION_ERR_CANCELLED;
    }
    h->state = AI_SESSION_STATE_TOKEN_PENDING;
    h->diag.state = h->state;
    h->diag.token_pending = true;
    h->diag.session_id = req->session_id;
    h->diag.generation++;
    generation = h->diag.generation;
    h->cancel_requested = false;
    h->rtc_start_requested = false;
    h->rtc_stop_requested = false;
    safe_req = *req;
    ai_session_unlock(h);

    ESP_LOGI(AI_SESSION_TAG,
             "[AI_SESSION][EXTERNAL_TOKEN_PENDING] sid=%lu gen=%lu trigger=%d",
             (unsigned long)safe_req.session_id,
             (unsigned long)generation,
             (int)safe_req.trigger);
    ai_session_emit(h, AI_SESSION_EVT_TOKEN_REQUESTED, safe_req.session_id);
    return AI_SESSION_OK;
}

ai_session_status_t Ai_Session_Start_With_Creds(ai_session_handle_t *h,
                                                const ai_session_rtc_creds_t *creds,
                                                ai_session_trigger_t trigger)
{
    ai_session_status_t gate_status;
    ai_session_state_t prev_state;
    uint32_t session_id;
    uint32_t generation;
    const ai_session_rtc_ops_t *rtc;
    bool rtc_policy_api;
    bool rollback;

    if ((h == NULL) || (creds == NULL) ||
        (creds->channel_name == NULL) || (creds->channel_name[0] == '\0'))
    {
        return AI_SESSION_ERR_ARG;
    }

    (void)trigger;

    ai_session_lock(h);
    prev_state = h->state;
    if (prev_state != AI_SESSION_STATE_TOKEN_PENDING)
    {
        ai_session_unlock(h);
        return (prev_state == AI_SESSION_STATE_IDLE) ? AI_SESSION_ERR_STATE : AI_SESSION_ERR_BUSY;
    }
    h->state = AI_SESSION_STATE_STARTING;
    h->diag.state = h->state;
    h->diag.token_pending = false;
    h->cancel_requested = false;
    h->rtc_start_requested = false;
    h->rtc_stop_requested = false;
    session_id = h->diag.session_id;
    generation = h->diag.generation;
    rtc = h->cfg.rtc;
    rtc_policy_api = ai_session_version_supports_join_policy(h->cfg.api_version);
    ai_session_unlock(h);

    ai_session_emit(h, AI_SESSION_EVT_TOKEN_RECEIVED, session_id);
    ai_session_emit(h, AI_SESSION_EVT_STARTING, session_id);

    gate_status = AI_SESSION_OK;
    if ((h->cfg.hooks != NULL) && (h->cfg.hooks->on_before_engine_start != NULL))
    {
        gate_status = h->cfg.hooks->on_before_engine_start(h, creds, h->cfg.hooks->user);
    }
    if (gate_status == AI_SESSION_OK)
    {
        if ((rtc == NULL) || (rtc->configure == NULL) ||
            (((!rtc_policy_api) || (rtc->start_with_policy == NULL)) &&
             (rtc->start == NULL)))
        {
            gate_status = AI_SESSION_ERR_RTC;
        }
        else
        {
            gate_status = Ai_Session_Accept_Rtc_Start(h, session_id, generation);
            if (gate_status == AI_SESSION_OK)
            {
                gate_status = rtc->configure(rtc->ctx, creds, session_id, generation);
            }
            if (gate_status == AI_SESSION_OK)
            {
                gate_status = Ai_Session_Accept_Rtc_Start(h, session_id, generation);
            }
            if (gate_status == AI_SESSION_OK)
            {
                ESP_LOGI(AI_SESSION_TAG,
                         "[AI_SESSION][JOIN_POLICY] sid=%lu gen=%lu attempts=%u timeout_ms=%lu backoff_ms=%lu",
                         (unsigned long)session_id,
                         (unsigned long)generation,
                         (unsigned int)h->cfg.join.max_join_attempts,
                         (unsigned long)h->cfg.join.join_wait_timeout_ms,
                         (unsigned long)h->cfg.join.join_retry_backoff_ms);
                if (rtc_policy_api && (rtc->start_with_policy != NULL))
                {
                    gate_status = rtc->start_with_policy(rtc->ctx,
                                                         session_id,
                                                         generation,
                                                         &h->cfg.join);
                }
                else
                {
                    ESP_LOGW(AI_SESSION_TAG,
                             "[AI_SESSION][JOIN_POLICY_BACKEND_LEGACY] sid=%lu gen=%lu action=start_without_policy",
                             (unsigned long)session_id,
                             (unsigned long)generation);
                    gate_status = rtc->start(rtc->ctx, session_id, generation);
                }
            }
        }
    }
    if (gate_status == AI_SESSION_OK)
    {
        return AI_SESSION_OK;
    }

    ai_session_lock(h);
    rollback = (h->state == AI_SESSION_STATE_STARTING) &&
               (h->diag.session_id == session_id) &&
               (h->diag.generation == generation);
    if (rollback)
    {
        h->state = AI_SESSION_STATE_IDLE;
        h->diag.state = h->state;
        h->diag.rtc_connected = false;
        h->diag.token_pending = false;
        h->cancel_requested = false;
        h->rtc_start_requested = false;
        h->rtc_stop_requested = false;
    }
    ai_session_unlock(h);
    if (rollback)
    {
        ai_session_emit(h, AI_SESSION_EVT_START_FAILED, (uint32_t)(-gate_status));
    }
    return gate_status;
}

ai_session_status_t Ai_Session_Stop(ai_session_handle_t *h,
                                    ai_session_stop_reason_t reason)
{
    if (h == NULL)
    {
        return AI_SESSION_ERR_ARG;
    }

    ai_session_lock(h);
    if (h->state == AI_SESSION_STATE_IDLE)
    {
        ai_session_unlock(h);
        return AI_SESSION_OK;
    }
    h->state = AI_SESSION_STATE_STOPPING;
    h->diag.state = h->state;
    h->diag.token_pending = false;
    h->cancel_requested = true;
    ai_session_unlock(h);

    ai_session_emit(h, AI_SESSION_EVT_STOPPING, (uint32_t)reason);

    ai_session_lock(h);
    h->state = AI_SESSION_STATE_IDLE;
    h->dialog_state = AI_SESSION_DIALOG_UNKNOWN;
    h->diag.state = h->state;
    h->diag.dialog_state = h->dialog_state;
    h->diag.rtc_connected = false;
    h->diag.agent_online = false;
    h->cancel_requested = false;
    h->rtc_start_requested = false;
    h->rtc_stop_requested = false;
    ai_session_unlock(h);

    ai_session_emit(h, AI_SESSION_EVT_STOPPED, (uint32_t)reason);
    return AI_SESSION_OK;
}

ai_session_status_t Ai_Session_Cancel_Pending(ai_session_handle_t *h)
{
    uint32_t session_id;

    if (h == NULL)
    {
        return AI_SESSION_ERR_ARG;
    }

    ai_session_lock(h);
    if ((h->state != AI_SESSION_STATE_TOKEN_PENDING) &&
        (h->state != AI_SESSION_STATE_STARTING))
    {
        ai_session_unlock(h);
        return AI_SESSION_ERR_STATE;
    }
    h->state = AI_SESSION_STATE_IDLE;
    h->diag.state = h->state;
    h->diag.token_pending = false;
    h->cancel_requested = true;
    h->rtc_start_requested = false;
    h->rtc_stop_requested = false;
    session_id = h->diag.session_id;
    ai_session_unlock(h);

    ai_session_emit(h, AI_SESSION_EVT_CANCELLED, session_id);
    return AI_SESSION_OK;
}

ai_session_status_t Ai_Session_Request_Rtc_Stop(ai_session_handle_t *h,
                                                ai_session_stop_reason_t reason)
{
    ai_session_state_t prev_state;
    uint32_t session_id;
    uint32_t generation;
    bool cancel_requested;
    bool accept;
    bool late_cancel_stop;
    const ai_session_rtc_ops_t *rtc;
    ai_session_status_t status;

    if (h == NULL)
    {
        return AI_SESSION_ERR_ARG;
    }

    ai_session_lock(h);
    prev_state = h->state;
    session_id = h->diag.session_id;
    generation = h->diag.generation;
    cancel_requested = h->cancel_requested;
    late_cancel_stop = (prev_state == AI_SESSION_STATE_IDLE) &&
                       cancel_requested &&
                       (session_id != 0U);
    accept = (((prev_state == AI_SESSION_STATE_STARTING) ||
               (prev_state == AI_SESSION_STATE_RUNNING)) ||
              late_cancel_stop) &&
             !h->rtc_stop_requested;
    if (!accept)
    {
        bool already_requested = h->rtc_stop_requested;
        ai_session_unlock(h);
        ESP_LOGW(AI_SESSION_TAG,
                 "[AI_SESSION][RTC_STOP_BLOCK] sid=%lu gen=%lu state=%s cancel=%d already_requested=%d action=request_stop_block",
                 (unsigned long)session_id,
                 (unsigned long)generation,
                 ai_session_state_str(prev_state),
                 cancel_requested ? 1 : 0,
                 already_requested ? 1 : 0);
        return already_requested ? AI_SESSION_ERR_BUSY : AI_SESSION_ERR_STATE;
    }

    h->rtc_stop_requested = true;
    h->cancel_requested = true;
    h->state = AI_SESSION_STATE_STOPPING;
    h->diag.state = h->state;
    h->diag.token_pending = false;
    rtc = h->cfg.rtc;
    ai_session_unlock(h);

    ESP_LOGI(AI_SESSION_TAG,
             "[AI_SESSION][RTC_STOP_ACCEPT] sid=%lu gen=%lu from_state=%s late_cancel=%d reason=%d reason_str=%s",
             (unsigned long)session_id,
             (unsigned long)generation,
             ai_session_state_str(prev_state),
             late_cancel_stop ? 1 : 0,
             (int)reason,
             Ai_Session_Stop_Reason_To_String(reason));
    ai_session_emit(h, AI_SESSION_EVT_STOPPING, (uint32_t)reason);

    if ((rtc == NULL) || (rtc->stop == NULL))
    {
        status = AI_SESSION_ERR_RTC;
    }
    else
    {
        status = rtc->stop(rtc->ctx, session_id, generation, reason);
    }

    if (status == AI_SESSION_OK)
    {
        return AI_SESSION_OK;
    }

    ai_session_lock(h);
    if ((h->diag.session_id == session_id) &&
        (h->diag.generation == generation) &&
        (h->state == AI_SESSION_STATE_STOPPING))
    {
        h->state = prev_state;
        h->diag.state = h->state;
        h->diag.token_pending = (prev_state == AI_SESSION_STATE_TOKEN_PENDING);
        h->rtc_stop_requested = false;
        h->cancel_requested = cancel_requested;
    }
    ai_session_unlock(h);

    ESP_LOGW(AI_SESSION_TAG,
             "[AI_SESSION][RTC_STOP_ROLLBACK] sid=%lu gen=%lu status=%d restore_state=%s",
             (unsigned long)session_id,
             (unsigned long)generation,
             (int)status,
             ai_session_state_str(prev_state));
    return status;
}

ai_session_status_t Ai_Session_On_Token_Received(ai_session_handle_t *h,
                                                 const ai_session_rtc_creds_t *creds)
{
    return Ai_Session_Start_With_Creds(h, creds, AI_SESSION_TRIGGER_UNKNOWN);
}

ai_session_status_t Ai_Session_On_Token_Failed(ai_session_handle_t *h,
                                               int cloud_result_code)
{
    if (h == NULL)
    {
        return AI_SESSION_ERR_ARG;
    }

    ai_session_lock(h);
    if ((h->state != AI_SESSION_STATE_TOKEN_PENDING) &&
        (h->state != AI_SESSION_STATE_STARTING))
    {
        ai_session_unlock(h);
        return AI_SESSION_ERR_STATE;
    }
    h->state = AI_SESSION_STATE_IDLE;
    h->diag.state = h->state;
    h->diag.token_pending = false;
    h->cancel_requested = false;
    h->rtc_start_requested = false;
    h->rtc_stop_requested = false;
    ai_session_unlock(h);

    ai_session_emit(h, AI_SESSION_EVT_TOKEN_FAILED, (uint32_t)cloud_result_code);
    return AI_SESSION_OK;
}

ai_session_status_t Ai_Session_Accept_Rtc_Start(ai_session_handle_t *h,
                                                uint32_t session_id,
                                                uint32_t generation)
{
    ai_session_state_t state;
    uint32_t current_sid;
    uint32_t current_generation;
    bool cancel_requested;
    bool stop_requested;
    bool accept;

    if (h == NULL)
    {
        return AI_SESSION_ERR_ARG;
    }

    ai_session_lock(h);
    state = h->state;
    current_sid = h->diag.session_id;
    current_generation = h->diag.generation;
    cancel_requested = h->cancel_requested;
    stop_requested = h->rtc_stop_requested;
    accept = (state == AI_SESSION_STATE_STARTING) &&
             (current_sid == session_id) &&
             (current_generation == generation) &&
             !cancel_requested &&
             !stop_requested;
    if (accept)
    {
        h->rtc_start_requested = true;
    }
    ai_session_unlock(h);

    if (accept)
    {
        ESP_LOGI(AI_SESSION_TAG,
                 "[AI_SESSION][RTC_START_ACCEPT] sid=%lu gen=%lu state=%s",
                 (unsigned long)session_id,
                 (unsigned long)generation,
                 ai_session_state_str(state));
        return AI_SESSION_OK;
    }

    ESP_LOGW(AI_SESSION_TAG,
             "[AI_SESSION][RTC_START_BLOCK] sid=%lu gen=%lu current_sid=%lu current_gen=%lu state=%s cancel=%d stop_requested=%d action=block",
             (unsigned long)session_id,
             (unsigned long)generation,
             (unsigned long)current_sid,
             (unsigned long)current_generation,
             ai_session_state_str(state),
             cancel_requested ? 1 : 0,
             stop_requested ? 1 : 0);
    return cancel_requested ? AI_SESSION_ERR_CANCELLED : AI_SESSION_ERR_STATE;
}

ai_session_status_t Ai_Session_Accept_Rtc_Stop(ai_session_handle_t *h,
                                               uint32_t session_id,
                                               uint32_t generation)
{
    ai_session_state_t state;
    uint32_t current_sid;
    uint32_t current_generation;
    bool already_requested;
    bool accept;

    if (h == NULL)
    {
        return AI_SESSION_ERR_ARG;
    }

    ai_session_lock(h);
    state = h->state;
    current_sid = h->diag.session_id;
    current_generation = h->diag.generation;
    already_requested = h->rtc_stop_requested;
    accept = (state != AI_SESSION_STATE_IDLE) &&
             (current_sid == session_id) &&
             (current_generation == generation) &&
             !already_requested;
    if (accept)
    {
        h->rtc_stop_requested = true;
        h->cancel_requested = true;
        h->state = AI_SESSION_STATE_STOPPING;
        h->diag.state = h->state;
        h->diag.token_pending = false;
    }
    ai_session_unlock(h);

    if (accept)
    {
        ESP_LOGI(AI_SESSION_TAG,
                 "[AI_SESSION][RTC_STOP_ACCEPT] sid=%lu gen=%lu from_state=%s",
                 (unsigned long)session_id,
                 (unsigned long)generation,
                 ai_session_state_str(state));
        return AI_SESSION_OK;
    }

    ESP_LOGW(AI_SESSION_TAG,
             "[AI_SESSION][RTC_STOP_BLOCK] sid=%lu gen=%lu current_sid=%lu current_gen=%lu state=%s already_requested=%d action=block",
             (unsigned long)session_id,
             (unsigned long)generation,
             (unsigned long)current_sid,
             (unsigned long)current_generation,
             ai_session_state_str(state),
             already_requested ? 1 : 0);
    return already_requested ? AI_SESSION_ERR_BUSY : AI_SESSION_ERR_STATE;
}

ai_session_status_t Ai_Session_Mark_Rtc_Running(ai_session_handle_t *h,
                                                uint32_t session_id,
                                                uint32_t generation)
{
    ai_session_state_t state;
    uint32_t current_sid;
    uint32_t current_generation;
    bool cancel_requested;
    bool accept;
    bool changed;

    if (h == NULL)
    {
        return AI_SESSION_ERR_ARG;
    }

    ai_session_lock(h);
    state = h->state;
    current_sid = h->diag.session_id;
    current_generation = h->diag.generation;
    cancel_requested = h->cancel_requested;
    accept = ((state == AI_SESSION_STATE_STARTING) ||
              (state == AI_SESSION_STATE_RUNNING)) &&
             (current_sid == session_id) &&
             (current_generation == generation) &&
             !cancel_requested;
    changed = accept && (state != AI_SESSION_STATE_RUNNING);
    if (accept)
    {
        h->state = AI_SESSION_STATE_RUNNING;
        h->diag.state = h->state;
        h->diag.rtc_connected = true;
        h->diag.token_pending = false;
        h->cancel_requested = false;
        h->rtc_start_requested = true;
    }
    ai_session_unlock(h);

    if (!accept)
    {
        ESP_LOGW(AI_SESSION_TAG,
                 "[AI_SESSION][RTC_RUNNING_SKIP] sid=%lu gen=%lu current_sid=%lu current_gen=%lu state=%s cancel=%d action=drop_stale_feedback",
                 (unsigned long)session_id,
                 (unsigned long)generation,
                 (unsigned long)current_sid,
                 (unsigned long)current_generation,
                 ai_session_state_str(state),
                 cancel_requested ? 1 : 0);
        return cancel_requested ? AI_SESSION_ERR_CANCELLED : AI_SESSION_ERR_STATE;
    }

    ESP_LOGI(AI_SESSION_TAG,
             "[AI_SESSION][RTC_RUNNING] sid=%lu gen=%lu prev_state=%s changed=%d",
             (unsigned long)session_id,
             (unsigned long)generation,
             ai_session_state_str(state),
             changed ? 1 : 0);
    if (changed)
    {
        ai_session_emit(h, AI_SESSION_EVT_STARTED, session_id);
    }
    return AI_SESSION_OK;
}

ai_session_status_t Ai_Session_Mark_Rtc_Stopped(ai_session_handle_t *h,
                                                uint32_t session_id,
                                                uint32_t generation,
                                                ai_session_stop_reason_t reason)
{
    ai_session_state_t state;
    uint32_t current_sid;
    uint32_t current_generation;
    bool sid_matches;
    bool changed;

    if (h == NULL)
    {
        return AI_SESSION_ERR_ARG;
    }

    ai_session_lock(h);
    state = h->state;
    current_sid = h->diag.session_id;
    current_generation = h->diag.generation;
    sid_matches = (current_sid == session_id) && (current_generation == generation);
    changed = sid_matches && (state != AI_SESSION_STATE_IDLE);
    if (sid_matches)
    {
        h->state = AI_SESSION_STATE_IDLE;
        h->dialog_state = AI_SESSION_DIALOG_UNKNOWN;
        h->diag.state = h->state;
        h->diag.dialog_state = h->dialog_state;
        h->diag.rtc_connected = false;
        h->diag.agent_online = false;
        h->diag.token_pending = false;
        h->cancel_requested = false;
        h->rtc_start_requested = false;
        h->rtc_stop_requested = false;
    }
    ai_session_unlock(h);

    if (!sid_matches)
    {
        ESP_LOGW(AI_SESSION_TAG,
                 "[AI_SESSION][RTC_STOPPED_SKIP] sid=%lu gen=%lu current_sid=%lu current_gen=%lu state=%s reason=%d reason_str=%s action=drop_stale_feedback",
                 (unsigned long)session_id,
                 (unsigned long)generation,
                 (unsigned long)current_sid,
                 (unsigned long)current_generation,
                 ai_session_state_str(state),
                 (int)reason,
                 Ai_Session_Stop_Reason_To_String(reason));
        return AI_SESSION_ERR_STATE;
    }

    ESP_LOGI(AI_SESSION_TAG,
             "[AI_SESSION][RTC_STOPPED] sid=%lu gen=%lu prev_state=%s changed=%d reason=%d reason_str=%s",
             (unsigned long)session_id,
             (unsigned long)generation,
             ai_session_state_str(state),
             changed ? 1 : 0,
             (int)reason,
             Ai_Session_Stop_Reason_To_String(reason));
    if (changed)
    {
        ai_session_emit(h, AI_SESSION_EVT_STOPPED, (uint32_t)reason);
    }
    return AI_SESSION_OK;
}

ai_session_status_t Ai_Session_Observe_Token_Result(
    ai_session_handle_t *h,
    const ai_session_token_result_t *result)
{
    uint16_t token_len;
    uint16_t channel_len;
    uint16_t app_id_len;
    ai_session_state_t state;
    bool expected;

    if ((h == NULL) || (result == NULL))
    {
        return AI_SESSION_ERR_ARG;
    }

    token_len = ai_session_strnlen_u16(result->rtc_token, AI_SESSION_TOKEN_OBSERVE_TOKEN_MAX);
    channel_len = ai_session_strnlen_u16(result->channel_name, AI_SESSION_TOKEN_OBSERVE_FIELD_MAX);
    app_id_len = ai_session_strnlen_u16(result->app_id, AI_SESSION_TOKEN_OBSERVE_FIELD_MAX);

    ai_session_lock(h);
    state = h->state;
    expected = ai_session_state_expects_token(state);
    h->diag.token_observe_count++;
    if (!expected)
    {
        h->diag.token_observe_unexpected_count++;
    }
    h->diag.last_token_result = result->result;
    h->diag.last_token_uid = (uint32_t)result->uid;
    h->diag.last_token_len = token_len;
    h->diag.last_channel_len = channel_len;
    h->diag.last_app_id_len = app_id_len;
    ai_session_unlock(h);

    ai_session_log_token_observe("handle",
                                 result,
                                 state,
                                 expected,
                                 token_len,
                                 channel_len,
                                 app_id_len);
    return AI_SESSION_OK;
}

ai_session_status_t Ai_Session_Module_Observe_Token_Result(
    const ai_session_token_result_t *result)
{
    uint16_t token_len;
    uint16_t channel_len;
    uint16_t app_id_len;

    if (result == NULL)
    {
        return AI_SESSION_ERR_ARG;
    }

    token_len = ai_session_strnlen_u16(result->rtc_token, AI_SESSION_TOKEN_OBSERVE_TOKEN_MAX);
    channel_len = ai_session_strnlen_u16(result->channel_name, AI_SESSION_TOKEN_OBSERVE_FIELD_MAX);
    app_id_len = ai_session_strnlen_u16(result->app_id, AI_SESSION_TOKEN_OBSERVE_FIELD_MAX);

    taskENTER_CRITICAL(&s_module_lock);
    s_module_token_stats.observe_count++;
    s_module_token_stats.last_result = result->result;
    s_module_token_stats.last_uid = (uint32_t)result->uid;
    s_module_token_stats.last_token_len = token_len;
    s_module_token_stats.last_channel_len = channel_len;
    s_module_token_stats.last_app_id_len = app_id_len;
    taskEXIT_CRITICAL(&s_module_lock);

    ai_session_log_token_observe("module",
                                 result,
                                 AI_SESSION_STATE_IDLE,
                                 true,
                                 token_len,
                                 channel_len,
                                 app_id_len);
    return AI_SESSION_OK;
}

ai_session_state_t Ai_Session_Get_State(const ai_session_handle_t *h)
{
    ai_session_state_t state;

    if (h == NULL)
    {
        return AI_SESSION_STATE_IDLE;
    }

    taskENTER_CRITICAL((portMUX_TYPE *)&h->lock);
    state = h->state;
    taskEXIT_CRITICAL((portMUX_TYPE *)&h->lock);
    return state;
}

bool Ai_Session_Is_Active(const ai_session_handle_t *h)
{
    ai_session_state_t state = Ai_Session_Get_State(h);
    return (state == AI_SESSION_STATE_TOKEN_PENDING) ||
           (state == AI_SESSION_STATE_STARTING) ||
           (state == AI_SESSION_STATE_RUNNING) ||
           (state == AI_SESSION_STATE_STOPPING);
}

ai_session_dialog_state_t Ai_Session_Get_Dialog_State(const ai_session_handle_t *h)
{
    ai_session_dialog_state_t state;

    if (h == NULL)
    {
        return AI_SESSION_DIALOG_UNKNOWN;
    }

    taskENTER_CRITICAL((portMUX_TYPE *)&h->lock);
    state = h->dialog_state;
    taskEXIT_CRITICAL((portMUX_TYPE *)&h->lock);
    return state;
}

bool Ai_Session_Get_Diag(const ai_session_handle_t *h,
                         ai_session_diag_t *out)
{
    if ((h == NULL) || (out == NULL))
    {
        return false;
    }

    taskENTER_CRITICAL((portMUX_TYPE *)&h->lock);
    *out = h->diag;
    taskEXIT_CRITICAL((portMUX_TYPE *)&h->lock);
    return true;
}

ai_session_status_t Ai_Session_Wait_Joined(ai_session_handle_t *h,
                                            uint32_t timeout_ms)
{
    (void)timeout_ms;
    if (h == NULL)
    {
        return AI_SESSION_ERR_ARG;
    }
    return (Ai_Session_Get_State(h) == AI_SESSION_STATE_RUNNING) ?
           AI_SESSION_OK : AI_SESSION_ERR_TIMEOUT;
}

void Ai_Session_Set_Rx_Playback_Enabled(ai_session_handle_t *h, bool enabled)
{
    const ai_audio_io_t *audio;

    if (h == NULL)
    {
        return;
    }
    ai_session_lock(h);
    audio = h->cfg.audio;
    ai_session_unlock(h);
    if ((audio != NULL) && (audio->set_rx_playback_enabled != NULL))
    {
        audio->set_rx_playback_enabled(enabled);
    }
}

void Ai_Session_Set_Mic_Uplink_Enabled(ai_session_handle_t *h, bool enabled)
{
    const ai_audio_io_t *audio;

    if (h == NULL)
    {
        return;
    }
    ai_session_lock(h);
    audio = h->cfg.audio;
    ai_session_unlock(h);
    if ((audio != NULL) && (audio->set_mic_uplink_enabled != NULL))
    {
        audio->set_mic_uplink_enabled(enabled);
    }
}

void Ai_Session_Set_Weak_Network(ai_session_handle_t *h, bool weak)
{
    if (h == NULL)
    {
        return;
    }
    ai_session_lock(h);
    h->weak_network = weak;
    ai_session_unlock(h);
}

ai_session_status_t Ai_Session_Reload_Idle_Timer(ai_session_handle_t *h,
                                                 const char *reason)
{
    (void)reason;
    if (h == NULL)
    {
        return AI_SESSION_ERR_ARG;
    }
    return Ai_Session_Is_Active(h) ? AI_SESSION_OK : AI_SESSION_ERR_STATE;
}
