#ifndef AI_DIALOG_DIAG_H
#define AI_DIALOG_DIAG_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_log.h"

#ifndef AI_HOTPATH_VERBOSE_LOGS
/* Default off: keep the timing summary/state whitelist, suppress payload/per-packet hot-path INFO. */
#ifdef CONFIG_AI_HOTPATH_VERBOSE_LOGS
#define AI_HOTPATH_VERBOSE_LOGS CONFIG_AI_HOTPATH_VERBOSE_LOGS
#else
#define AI_HOTPATH_VERBOSE_LOGS 0
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    uint32_t sid;
    int64_t  start_us;
    int64_t  now_us;
    int32_t  delta_ms;
    int      core_id;
    uint32_t priority;
} ai_dialog_diag_snapshot_t;

void Ai_Dialog_Diag_Start(uint32_t sid, const char *trigger);
void Ai_Dialog_Diag_End(const char *reason);
uint32_t Ai_Dialog_Diag_Get_Sid(void);
ai_dialog_diag_snapshot_t Ai_Dialog_Diag_Snapshot(void);
bool Ai_Dialog_Diag_Info_Enabled(const char *stage);

void Ai_Dialog_Timing_Mark_Publish(void);
void Ai_Dialog_Timing_Mark_Token_Rx(void);
void Ai_Dialog_Timing_Mark_Rtc_Engine_Start(void);
void Ai_Dialog_Timing_Mark_Rtc_Start(void);
void Ai_Dialog_Timing_Log_Join_Success(void);
void Ai_Dialog_Timing_Log_Summary(const char *reason);

#define AI_DIALOG_DIAG_LOGI(tag, stage, fmt, ...)                                      \
    do {                                                                               \
        if (Ai_Dialog_Diag_Info_Enabled((stage))) {                                    \
            ai_dialog_diag_snapshot_t _ai_diag = Ai_Dialog_Diag_Snapshot();            \
            ESP_LOGI((tag), "[AI_FLOW][%s] sid=%lu t=%lldus +%ldms core=%d prio=%lu " fmt, \
                     (stage),                                                          \
                     (unsigned long)_ai_diag.sid,                                      \
                     (long long)_ai_diag.now_us,                                       \
                     (long)_ai_diag.delta_ms,                                          \
                     _ai_diag.core_id,                                                 \
                     (unsigned long)_ai_diag.priority,                                 \
                     ##__VA_ARGS__);                                                   \
        }                                                                              \
    } while (0)

#define AI_HOTPATH_VERBOSE_DO(...)       \
    do {                                 \
        if (AI_HOTPATH_VERBOSE_LOGS) {   \
            __VA_ARGS__;                 \
        }                                \
    } while (0)

#define AI_DIALOG_DIAG_LOGW(tag, stage, fmt, ...)                                      \
    do {                                                                               \
        ai_dialog_diag_snapshot_t _ai_diag = Ai_Dialog_Diag_Snapshot();                \
        ESP_LOGW((tag), "[AI_FLOW][%s] sid=%lu t=%lldus +%ldms core=%d prio=%lu " fmt, \
                 (stage),                                                              \
                 (unsigned long)_ai_diag.sid,                                          \
                 (long long)_ai_diag.now_us,                                           \
                 (long)_ai_diag.delta_ms,                                              \
                 _ai_diag.core_id,                                                     \
                 (unsigned long)_ai_diag.priority,                                     \
                 ##__VA_ARGS__);                                                       \
    } while (0)

#define AI_DIALOG_DIAG_LOGE(tag, stage, fmt, ...)                                      \
    do {                                                                               \
        ai_dialog_diag_snapshot_t _ai_diag = Ai_Dialog_Diag_Snapshot();                \
        ESP_LOGE((tag), "[AI_FLOW][%s] sid=%lu t=%lldus +%ldms core=%d prio=%lu " fmt, \
                 (stage),                                                              \
                 (unsigned long)_ai_diag.sid,                                          \
                 (long long)_ai_diag.now_us,                                           \
                 (long)_ai_diag.delta_ms,                                              \
                 _ai_diag.core_id,                                                     \
                 (unsigned long)_ai_diag.priority,                                     \
                 ##__VA_ARGS__);                                                       \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif /* AI_DIALOG_DIAG_H */
