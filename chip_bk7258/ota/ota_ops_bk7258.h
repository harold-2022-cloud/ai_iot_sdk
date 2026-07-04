/**
 * @file   ota_ops_bk7258.h
 * @brief  BK7258 OTA backend ops — platform_ota_backend_ops_t over Bsp_Ota_Flash_*.
 *
 *  Include-guarded, extern-C clean.  Consumers add bk_common + chip_bk7258 to
 *  their REQUIRES and call Bk_Ota_Ops_Get() to retrieve the static ops table.
 *
 *  Context type bk_ota_ctx_t carries the expected CRC that commit() needs.
 *  Pass a pointer to a caller-owned bk_ota_ctx_t as platform_ota_config_t.backend_ctx.
 *  For P7 smoke/control-plane verification the field is zero-initialised;
 *  real OTA must populate expected_crc before platform_ota_commit() is called.
 */

#ifndef OTA_OPS_BK7258_H
#define OTA_OPS_BK7258_H

#include "platform_ota.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  BK7258 OTA backend context.
 *
 *  The platform_ota_backend_ops_t.commit callback receives this as `void *ctx`.
 *  Set expected_crc to the image CRC32 before calling platform_ota_commit().
 *
 *  NOTE (P7 non-goal): the P7 smoke test does not exercise real OTA and does
 *  not populate expected_crc.  Bsp_Ota_Flash_Check_Crc(0) will fail on a real
 *  image; the protocol layer MUST set this field for production use.  See the
 *  TODO comment in ota_ops_bk7258.c::be_commit().
 */
typedef struct
{
    uint32_t expected_crc; /**< Expected CRC32 of the complete OTA image. */
} bk_ota_ctx_t;

/**
 * @brief  Return the static BK7258 OTA backend ops table.
 *
 *  Assign to platform_ota_config_t.backend_ops.
 *  Pass a pointer to a bk_ota_ctx_t as platform_ota_config_t.backend_ctx.
 *
 * @return  Pointer to read-only platform_ota_backend_ops_t.
 */
const platform_ota_backend_ops_t *Bk_Ota_Ops_Get(void);

#ifdef __cplusplus
}
#endif

#endif /* OTA_OPS_BK7258_H */
