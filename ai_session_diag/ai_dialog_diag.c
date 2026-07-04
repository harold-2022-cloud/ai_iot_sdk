#include "ai_dialog_diag.h"

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static volatile uint32_t s_ai_dialog_diag_sid = 0;
static volatile uint32_t s_ai_dialog_diag_start_ms = 0;
static portMUX_TYPE s_ai_dialog_diag_mux = portMUX_INITIALIZER_UNLOCKED;

typedef struct
{
    uint32_t sid;
    uint32_t publish_count;
    int64_t  key_us;
    int64_t  first_publish_us;
    int64_t  publish_us;
    int64_t  token_rx_us;
    int64_t  rtc_engine_start_us;
    int64_t  rtc_start_us;
    int64_t  join_success_us;
    char     trigger[24];
} ai_dialog_timing_t;

static ai_dialog_timing_t s_ai_dialog_timing = {0};

static int64_t Ai_Dialog_Timing_Delta_Ms(int64_t newer_us, int64_t older_us)
{
    if ((newer_us <= 0) || (older_us <= 0) || (newer_us < older_us))
    {
        return -1;
    }

    return (newer_us - older_us) / 1000LL;
}

static void Ai_Dialog_Timing_Copy_Trigger(char *dst, size_t dst_len, const char *trigger)
{
    if ((dst == NULL) || (dst_len == 0U))
    {
        return;
    }

    const char *safe_trigger = (trigger != NULL) ? trigger : "unknown";
    (void)strncpy(dst, safe_trigger, dst_len - 1U);
    dst[dst_len - 1U] = '\0';
}

void Ai_Dialog_Diag_Start(uint32_t sid, const char *trigger)
{
    int64_t now_us = esp_timer_get_time();
    uint32_t now_ms = (uint32_t)(now_us / 1000LL);

    taskENTER_CRITICAL(&s_ai_dialog_diag_mux);
    s_ai_dialog_diag_start_ms = now_ms;
    memset(&s_ai_dialog_timing, 0, sizeof(s_ai_dialog_timing));
    s_ai_dialog_timing.sid = sid;
    s_ai_dialog_timing.key_us = now_us;
    Ai_Dialog_Timing_Copy_Trigger(s_ai_dialog_timing.trigger,
                                  sizeof(s_ai_dialog_timing.trigger),
                                  trigger);
    s_ai_dialog_diag_sid = sid;
    taskEXIT_CRITICAL(&s_ai_dialog_diag_mux);
}

void Ai_Dialog_Diag_End(const char *reason)
{
    (void)reason;
    /* Keep the last sid/start timestamp until the next session starts.
     * Late RTC/MQTT callbacks can then still be correlated to the session. */
}

uint32_t Ai_Dialog_Diag_Get_Sid(void)
{
    return s_ai_dialog_diag_sid;
}

ai_dialog_diag_snapshot_t Ai_Dialog_Diag_Snapshot(void)
{
    ai_dialog_diag_snapshot_t snapshot = {0};
    uint32_t start_ms;
    uint32_t now_ms;

    snapshot.now_us = esp_timer_get_time();

    taskENTER_CRITICAL(&s_ai_dialog_diag_mux);
    snapshot.sid = s_ai_dialog_diag_sid;
    start_ms = s_ai_dialog_diag_start_ms;
    taskEXIT_CRITICAL(&s_ai_dialog_diag_mux);

    now_ms = (uint32_t)(snapshot.now_us / 1000LL);
    snapshot.start_us = (int64_t)start_ms * 1000LL;
    snapshot.delta_ms = (start_ms > 0U) ? (int32_t)(now_ms - start_ms) : -1;
    snapshot.core_id = xPortGetCoreID();
    snapshot.priority = (uint32_t)uxTaskPriorityGet(NULL);

    return snapshot;
}

bool Ai_Dialog_Diag_Info_Enabled(const char *stage)
{
    if (AI_HOTPATH_VERBOSE_LOGS)
    {
        return true;
    }
    if (stage == NULL)
    {
        return false;
    }

    /* 白名單保留跨任務狀態轉換；payload/每包/音訊熱路徑 INFO 預設不輸出。 */
    return (strcmp(stage, "session_start") == 0) ||
           (strcmp(stage, "token_publish_queued") == 0) ||
           (strcmp(stage, "token_callback_enter") == 0) ||
           (strcmp(stage, "rtc_start_event_posted") == 0) ||
           (strcmp(stage, "rtc_engine_start_enter") == 0) ||
           (strcmp(stage, "rtc_start_begin") == 0) ||
           (strcmp(stage, "rtc_join_success_event") == 0) ||
           (strcmp(stage, "agent_joined_event_enter") == 0) ||
           (strcmp(stage, "agent_joined_event_done") == 0) ||
           (strcmp(stage, "turn_mark") == 0) ||
           (strcmp(stage, "turn_stall") == 0) ||
           (strcmp(stage, "audio_tx_first") == 0) ||
           (strcmp(stage, "audio_tx_active_first") == 0) ||
           (strcmp(stage, "audio_rx_play_enter_first") == 0) ||
           (strcmp(stage, "audio_rx_decoder_write_first") == 0) ||
           (strcmp(stage, "session_stop_event_done") == 0);
}

void Ai_Dialog_Timing_Mark_Publish(void)
{
    int64_t now_us = esp_timer_get_time();

    taskENTER_CRITICAL(&s_ai_dialog_diag_mux);
    if (s_ai_dialog_timing.first_publish_us <= 0)
    {
        s_ai_dialog_timing.first_publish_us = now_us;
    }
    s_ai_dialog_timing.publish_us = now_us;
    s_ai_dialog_timing.publish_count++;
    taskEXIT_CRITICAL(&s_ai_dialog_diag_mux);
}

void Ai_Dialog_Timing_Mark_Token_Rx(void)
{
    int64_t now_us = esp_timer_get_time();

    taskENTER_CRITICAL(&s_ai_dialog_diag_mux);
    s_ai_dialog_timing.token_rx_us = now_us;
    taskEXIT_CRITICAL(&s_ai_dialog_diag_mux);
}

void Ai_Dialog_Timing_Mark_Rtc_Engine_Start(void)
{
    int64_t now_us = esp_timer_get_time();

    taskENTER_CRITICAL(&s_ai_dialog_diag_mux);
    s_ai_dialog_timing.rtc_engine_start_us = now_us;
    taskEXIT_CRITICAL(&s_ai_dialog_diag_mux);
}

void Ai_Dialog_Timing_Mark_Rtc_Start(void)
{
    int64_t now_us = esp_timer_get_time();

    taskENTER_CRITICAL(&s_ai_dialog_diag_mux);
    s_ai_dialog_timing.rtc_start_us = now_us;
    taskEXIT_CRITICAL(&s_ai_dialog_diag_mux);
}

void Ai_Dialog_Timing_Log_Join_Success(void)
{
    int64_t now_us = esp_timer_get_time();

    taskENTER_CRITICAL(&s_ai_dialog_diag_mux);
    s_ai_dialog_timing.join_success_us = now_us;
    taskEXIT_CRITICAL(&s_ai_dialog_diag_mux);

    Ai_Dialog_Timing_Log_Summary("join_success");
}

void Ai_Dialog_Timing_Log_Summary(const char *reason)
{
    ai_dialog_timing_t timing;
    const char *safe_reason = (reason != NULL) ? reason : "unknown";

    taskENTER_CRITICAL(&s_ai_dialog_diag_mux);
    timing = s_ai_dialog_timing;
    taskEXIT_CRITICAL(&s_ai_dialog_diag_mux);

    ESP_LOGI("AI_TIMING",
             "[AI_TIMING_SUMMARY] sid=%lu reason=%s trigger=%s publish_count=%lu "
             "key_to_first_publish_ms=%lld last_publish_to_token_rx_ms=%lld "
             "token_rx_to_engine_start_ms=%lld engine_start_to_rtc_start_ms=%lld "
             "rtc_start_to_join_success_ms=%lld token_rx_to_join_success_ms=%lld "
             "key_to_join_success_ms=%lld",
             (unsigned long)timing.sid,
             safe_reason,
             timing.trigger[0] ? timing.trigger : "unknown",
             (unsigned long)timing.publish_count,
             (long long)Ai_Dialog_Timing_Delta_Ms(timing.first_publish_us, timing.key_us),
             (long long)Ai_Dialog_Timing_Delta_Ms(timing.token_rx_us, timing.publish_us),
             (long long)Ai_Dialog_Timing_Delta_Ms(timing.rtc_engine_start_us, timing.token_rx_us),
             (long long)Ai_Dialog_Timing_Delta_Ms(timing.rtc_start_us, timing.rtc_engine_start_us),
             (long long)Ai_Dialog_Timing_Delta_Ms(timing.join_success_us, timing.rtc_start_us),
             (long long)Ai_Dialog_Timing_Delta_Ms(timing.join_success_us, timing.token_rx_us),
             (long long)Ai_Dialog_Timing_Delta_Ms(timing.join_success_us, timing.key_us));
}
