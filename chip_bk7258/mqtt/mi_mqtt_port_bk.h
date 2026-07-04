// mi_mqtt_port_bk.h — BK7258 platform seam for the vendored coreMQTT engine
//
// 目的：替换 mi_mqtt.c 中的 #include "rino_hal.h"，将引擎所需的平台原语
//       映射到 entity SDK 的 Bsp_* 接口（bsp_system.h）。
//       只有 mi_mqtt.c 包含本文件；mi_mqtt_serializer.c / mi_mqtt_state.c
//       不需要（它们不使用 hal_* 符号，日志宏由 mi_mqtt_config_defaults.h 定义）。
//
// 日志宏处理决定：
//   mi_mqtt_config_defaults.h 用 #ifndef 守卫定义了 LogError/LogWarn/LogInfo/LogDebug
//   均为空操作（默认关闭）。mi_mqtt_custom_config.h 未覆盖这些宏。
//   因此本文件不重新定义日志宏，避免宏重定义冲突。
//   如需启用日志，在 mi_mqtt_custom_config.h 中覆盖即可。
//
// 互斥量桥接说明：
//   引擎将 mutex 成员声明为 void*（MQTTContext_t.mutex），调用时传 &pContext->mutex
//   即 void**。Bsp_Mutex_t 也是 void*，Bsp_Mutex_Lock 接受 Bsp_Mutex_t*（即 void**）
//   + timeout_ms，故可直接透传，无需类型转换中间变量。
//
// 内存分配说明：
//   hal_malloc/hal_free 映射到 Bsp_Mem_Malloc/Bsp_Mem_Free（Task 1.1 实现）。
//   引擎内部通过同一对函数配置和释放缓冲区，配对自洽。
//
// 发现的额外 seam 符号：
//   引擎实际只使用 hal_mutex_lock / hal_mutex_unlock（含第二参数 timeout）。
//   未发现 hal_malloc / hal_free / hal_mutex_init / hal_mutex_deinit 的调用。
//   本文件仍提供 hal_malloc / hal_free 以防 Task 5.1b 的 mi_mqtt_client_bk7258.c
//   包含本头文件时需要（声明为 static inline 不会产生未使用符号警告）。

#pragma once

#include <stdint.h>
#include "bsp_system.h"   // Bsp_Mem_Malloc, Bsp_Mem_Free, Bsp_Mutex_t, Bsp_Mutex_Lock/Unlock

// --------------------------------------------------------------------------
// 内存分配（engine grep: 未在 mi_mqtt.c 直接调用，但按规范提供）
// --------------------------------------------------------------------------

static inline void *hal_malloc(unsigned int n)
{
    return Bsp_Mem_Malloc(n);
}

static inline void hal_free(void *p)
{
    Bsp_Mem_Free(p);
}

// --------------------------------------------------------------------------
// 互斥量（engine 调用: hal_mutex_lock(&ctx->mutex, 0xFFFFFFFF)）
//   ctx->mutex 类型为 void*，&ctx->mutex 即 void** == Bsp_Mutex_t*
//   Bsp_Mutex_Lock(Bsp_Mutex_t *mutex, uint32_t timeout_ms) 签名匹配
// --------------------------------------------------------------------------

static inline uint32_t hal_mutex_lock(void *m, uint32_t timeout_ms)
{
    return Bsp_Mutex_Lock((Bsp_Mutex_t *)m, timeout_ms);
}

static inline uint32_t hal_mutex_unlock(void *m)
{
    return Bsp_Mutex_Unlock((Bsp_Mutex_t *)m);
}

// --------------------------------------------------------------------------
// 日志宏：不在此处定义。
//   mi_mqtt_config_defaults.h 已通过 #ifndef 守卫将四个宏定义为空操作。
//   mi_mqtt_custom_config.h 未覆盖，故保持空操作。
//   在此重定义会触发 "macro redefined" 警告/错误，故省略。
// --------------------------------------------------------------------------
