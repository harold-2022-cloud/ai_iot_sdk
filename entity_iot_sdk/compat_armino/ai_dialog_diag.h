#ifndef AI_DIALOG_DIAG_H
#define AI_DIALOG_DIAG_H

/* entity_iot_sdk core 的中立诊断 facade 默认实现（armino/非-IDF build）。
 *
 * 架构（ports-and-adapters）：core 拥有它所需的诊断接口（本 header 的宏/符号），
 * 真实诊断实现由产品/平台层提供。带真实 AI-flow 诊断的 esp32 路径经 platform_adapter
 * 取真实 ai_dialog_diag.h；不提供真实实现的平台（如 BK7258 port）回退到此 no-op 默认。
 *
 * 关键铁则：core 绝不为取此 header 反向 REQUIRES 任何 chip 后端（chip_bk7258/
 * chip_esp32s3）或 platform_adapter（见 docs/porting/entity_porting_contract.md §Forbidden
 * Dependencies）。故此默认 header 内置于 core 自身（compat_armino/，仅 armino 分支
 * 加入 INCLUDE_DIRS，不污染 esp32 分支），而非借自 chip 后端。
 *
 * no-op 仅镜像 entity_mqtt 实际引用的符号，其余 YAGNI 不写。被引用符号（grep 确认）：
 *   - AI_DIALOG_DIAG_LOGI / LOGW / LOGE（entity_mqtt_event_report.c, entity_mqtt_event_respone_parse.c）
 *   - AI_HOTPATH_VERBOSE_DO（entity_mqtt_event_report.c:47, 1005, 1043, 1058, 1064）
 *   - Ai_Dialog_Timing_Mark_Publish（entity_mqtt_event_report.c:1062）
 *
 * 关键：no-op 必须「消费」宏参数，否则仅为诊断而声明的局部变量（如 mqtt_conn）会被
 * -Werror=unused-variable 杀掉。故 LOGx 把参数传进一个空操作 variadic inline（参数被求值+传递+丢弃），
 * VERBOSE_DO 则镜像 ESP 真实语义 `if (0) { ... }`（编译体内语句→变量「被使用」+死代码消除）。 */

/* 空操作 variadic sink：求值并丢弃所有参数（含可变参），使其计为「已使用」。 */
static inline void _ai_dialog_diag_noop(const char *tag, const char *stage, const char *fmt, ...)
{
    (void)tag;
    (void)stage;
    (void)fmt;
}

#define AI_DIALOG_DIAG_LOGI(tag, stage, fmt, ...)  _ai_dialog_diag_noop((tag), (stage), (fmt), ##__VA_ARGS__)
#define AI_DIALOG_DIAG_LOGW(tag, stage, fmt, ...)  _ai_dialog_diag_noop((tag), (stage), (fmt), ##__VA_ARGS__)
#define AI_DIALOG_DIAG_LOGE(tag, stage, fmt, ...)  _ai_dialog_diag_noop((tag), (stage), (fmt), ##__VA_ARGS__)

/* 镜像 ESP 真实 AI_HOTPATH_VERBOSE_DO（默认 AI_HOTPATH_VERBOSE_LOGS=0）的语义：
   body 在 if(0) 内编译（内部变量计为已使用，避免 -Werror=unused-variable），但被死代码消除。
   body 内的 ENTITY_LOG* 为 entity core 自有宏，armino 下已定义。 */
#ifndef AI_HOTPATH_VERBOSE_DO
#define AI_HOTPATH_VERBOSE_DO(...)  do { if (0) { __VA_ARGS__ } } while (0)
#endif

static inline void Ai_Dialog_Timing_Mark_Publish(void) {}

#endif /* AI_DIALOG_DIAG_H */
