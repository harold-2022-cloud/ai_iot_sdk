/**
 * @file   ota_ops_bk7258.c
 * @brief  BK7258 OTA backend ops — platform_ota_backend_ops_t over Bsp_Ota_Flash_*.
 *
 * State-machine mapping (plan D2):
 *
 *   begin(ctx)         → Bsp_Ota_Flash_Init()
 *   write(ctx,d,n,tot) → Bsp_Ota_Flash_Process_Data(buf, len16, total)  [chunked if len>UINT16_MAX]
 *   commit(ctx)        → Bsp_Ota_Flash_Check_Crc(expected_crc) + Bsp_Ota_Flash_Complete()
 *   abort(ctx)         → Bsp_Ota_Flash_Deinit()
 *
 * Return-value normalisation:
 *   Bsp_Ota_Flash_* return BK_OK (0) on success and BK_FAIL (non-zero, typically -1)
 *   on error.  platform_ota_backend_ops_t convention is 0 == success, <0 == error.
 *   BK_OK == 0, so the return values are already compatible — no translation needed.
 *   BK_FAIL is -1 which also satisfies the < 0 error convention.
 *
 * CRC context:
 *   platform_ota_backend_ops_t.commit does NOT accept a CRC argument.
 *   The expected CRC is carried in bk_ota_ctx_t.expected_crc (backend_ctx).
 *   If ctx is NULL the CRC defaults to 0 (smoke-test path — will fail on a real image).
 *
 * Build note:
 *   At Phase 8 the BK7258 SDK header path must be on the include search path so
 *   that "bsp_ota_flash.h" resolves.  Expected location:
 *       chip_bk7258/bsp_ota_flash.h  (or via bk_common component).
 *   Adjust the CMakeLists.txt INCLUDE_DIRS accordingly if the path changes.
 */

#include <stddef.h>
#include <stdint.h>
#include <limits.h>   /* UINT16_MAX */

#include "platform_ota.h"
#include "ota_ops_bk7258.h"

/*
 * BK OTA Flash driver declarations.
 *
 * 不 #include rino 的 bsp_ota_flash.h —— 它會拉 mcu.h / bsp_include.h（rino 專屬橋接頭）
 * 與 Ota_Info_t / bk_logic_partition_t / Bsp_Crc32_Context_t 等 rino 型別，污染中立邊界。
 * 改用自含 extern 宣告（簽名只用標準型別）。實作由真正啟用 OTA 的產品於連結期提供
 * （Phase 8R re-host：rino bsp_ota_flash.c 或 entity 自帶實作）。冒煙不呼叫 OTA →
 * Bk_Ota_Ops_Get 未被引用 → 此 TU 連結期被 GC，不會有 undefined Bsp_Ota_Flash_* 符號。
 */
extern int  Bsp_Ota_Flash_Init(void);
extern int  Bsp_Ota_Flash_Process_Data(unsigned char *buf, uint16_t len, uint32_t total);
extern int  Bsp_Ota_Flash_Check_Crc(uint32_t in_crc);
extern void Bsp_Ota_Flash_Complete(void);
extern int  Bsp_Ota_Flash_Deinit(void);


/* -------------------------------------------------------------------------- */
/* begin                                                                       */
/* -------------------------------------------------------------------------- */

/**
 * @name   be_begin
 * @brief  Initialise BK OTA flash state and partition.
 * @param  ctx  Unused (ctx carried on backend_ctx, not used here).
 * @retval 0        Success (BK_OK).
 * @retval non-zero Failure (BK_FAIL = -1).
 */
static int be_begin(void *ctx)
{
    (void)ctx;
    return Bsp_Ota_Flash_Init();
}


/* -------------------------------------------------------------------------- */
/* write                                                                       */
/* -------------------------------------------------------------------------- */

/**
 * @name   be_write
 * @brief  Feed a data chunk to BK OTA flash.
 *
 *  Bsp_Ota_Flash_Process_Data accepts len as uint16_t (max 65535 bytes).
 *  The platform ops contract passes len as size_t with no upper-bound guarantee.
 *  If a single call delivers more than UINT16_MAX bytes we loop in sub-chunks of
 *  at most UINT16_MAX bytes, passing the same `total` (constant image size) each
 *  time.  On sub-chunk failure the error is returned immediately.
 *
 * @param  ctx    Unused.
 * @param  data   Source data buffer.
 * @param  len    Byte count for this chunk (size_t, may exceed UINT16_MAX).
 * @param  total  Total image size in bytes (constant across all write calls).
 * @retval 0        Success (BK_OK).
 * @retval non-zero Failure (BK_FAIL = -1).
 */
static int be_write(void *ctx, const void *data, size_t len, uint32_t total)
{
    (void)ctx;

    const unsigned char *p     = (const unsigned char *)data;
    size_t               remaining = len;

    while (remaining > 0U)
    {
        /* BK Process_Data len parameter is uint16_t — cap each sub-chunk. */
        uint16_t chunk = (remaining > (size_t)UINT16_MAX)
                         ? (uint16_t)UINT16_MAX
                         : (uint16_t)remaining;

        int rc = Bsp_Ota_Flash_Process_Data((unsigned char *)p, chunk, total);
        if (rc != 0)
        {
            return rc;
        }

        p         += chunk;
        remaining -= (size_t)chunk;
    }

    return 0; /* BK_OK */
}


/* -------------------------------------------------------------------------- */
/* commit                                                                      */
/* -------------------------------------------------------------------------- */

/**
 * @name   be_commit
 * @brief  Verify CRC and mark OTA complete.
 *
 *  Sequence: Bsp_Ota_Flash_Check_Crc(expected_crc) → Bsp_Ota_Flash_Complete().
 *  Bsp_Ota_Flash_Complete() is void; the commit result is the Check_Crc return value.
 *
 *  The expected CRC is read from bk_ota_ctx_t.expected_crc via the backend_ctx
 *  pointer.  If ctx is NULL the CRC defaults to 0.
 *
 *  TODO (P7 non-goal, spec §6):
 *    The P7 smoke/control-plane run does NOT exercise real OTA and does NOT
 *    populate bk_ota_ctx_t.expected_crc.  As a result Check_Crc(0) will reject
 *    a real image.  For production OTA the protocol layer MUST set
 *    bk_ota_ctx_t.expected_crc (via platform_ota_config_t.backend_ctx) before
 *    calling platform_ota_commit().  This file keeps the real Check_Crc call so
 *    the state machine and host tests (Task 7.3) observe the correct call order.
 *    Do NOT silently substitute 0 success — the CRC guard must fire for real images.
 *
 * @param  ctx  Pointer to bk_ota_ctx_t (may be NULL for smoke tests).
 * @retval 0        CRC matched (BK_OK).
 * @retval non-zero CRC mismatch or driver error (BK_FAIL = -1).
 */
static int be_commit(void *ctx)
{
    uint32_t expected_crc = (ctx != NULL)
                            ? ((bk_ota_ctx_t *)ctx)->expected_crc
                            : 0u;

    int rc = Bsp_Ota_Flash_Check_Crc(expected_crc);
    Bsp_Ota_Flash_Complete(); /* void — does not affect return value */
    return rc;
}


/* -------------------------------------------------------------------------- */
/* abort                                                                       */
/* -------------------------------------------------------------------------- */

/**
 * @name   be_abort
 * @brief  Release BK OTA flash resources (deinit).
 * @param  ctx  Unused.
 * @retval 0        Success (BK_OK).
 * @retval non-zero Failure (BK_FAIL = -1; e.g. called before begin).
 */
static int be_abort(void *ctx)
{
    (void)ctx;
    return Bsp_Ota_Flash_Deinit();
}


/* -------------------------------------------------------------------------- */
/* ops table + getter                                                           */
/* -------------------------------------------------------------------------- */

static const platform_ota_backend_ops_t s_bk7258_ops =
{
    .begin  = be_begin,
    .write  = be_write,
    .commit = be_commit,
    .abort  = be_abort,
};

/**
 * @name   Bk_Ota_Ops_Get
 * @brief  Return the static BK7258 OTA backend ops table.
 * @retval Pointer to read-only platform_ota_backend_ops_t.
 */
const platform_ota_backend_ops_t *Bk_Ota_Ops_Get(void)
{
    return &s_bk7258_ops;
}
