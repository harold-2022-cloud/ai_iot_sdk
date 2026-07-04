/*
 * @name    storage_ops_bk7258.h
 * @brief   BK7258 EasyFlash storage ops — declaration of Bk_Storage_Ops_Get().
 *
 * Consumers (e.g. Phase-8R product init) call Bk_Storage_Ops_Get() to obtain
 * the filled platform_storage_ops_t and pass it to platform_storage_create().
 *
 * FLAT KV NOTE: EasyFlash is a global flat key-value store (no namespaces).
 * The ctx pointer passed through platform_storage_config_t is ignored.
 * Products must ensure key strings are unique across all components to avoid
 * collisions — unlike the esp32 NVS backend which scopes keys under a namespace.
 */

#pragma once

#include "platform_storage.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   Return the singleton BK7258 EasyFlash storage ops.
 * @retval  Pointer to a static const platform_storage_ops_t.
 */
const platform_storage_ops_t *Bk_Storage_Ops_Get(void);

#ifdef __cplusplus
}
#endif
