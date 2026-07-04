#ifndef OTA_BACKEND_ESP32_H
#define OTA_BACKEND_ESP32_H

#include "platform_ota.h"

#ifdef __cplusplus
extern "C" {
#endif

const platform_ota_backend_ops_t *ota_backend_esp32_ops(void);
void *ota_backend_esp32_ctx(void);

#ifdef __cplusplus
}
#endif
#endif /* OTA_BACKEND_ESP32_H */
