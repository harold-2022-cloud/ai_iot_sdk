#ifndef PLATFORM_STORAGE_EXEC_ESP32_H
#define PLATFORM_STORAGE_EXEC_ESP32_H

#include "platform_storage_proxy.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ESP32 execution policy: an Internal-SRAM-stack worker thread draining a
 * {fn,arg} queue. queue_len MUST be >= the proxy slot_count. stack_bytes is in
 * bytes (ESP-IDF). Returns an exec with ops==NULL on failure. */
platform_storage_exec_t platform_storage_exec_esp32_worker(unsigned int queue_len,
                                                           unsigned int stack_bytes,
                                                           unsigned int prio);

#ifdef __cplusplus
}
#endif
#endif /* PLATFORM_STORAGE_EXEC_ESP32_H */
