/*
 * storage_ops_bk7258.c — BK7258 platform_storage_ops_t over EasyFlash
 *
 * ── Design notes ─────────────────────────────────────────────────────────────
 *
 * FLAT KV / ctx unused:
 *   EasyFlash is a global flat key-value store; there are no namespaces.
 *   Unlike the esp32 NVS backend (which scopes keys under a per-handle
 *   namespace), all keys here are process-global.  The `ctx` pointer passed
 *   through platform_storage_config_t is therefore unused in every op.
 *   Products must guarantee key-string uniqueness across all components to
 *   prevent cross-component collisions.
 *
 * init no-op rationale:
 *   In rino's hal_storage_bk7258.c the BK7258 reference implementation does
 *   NOT call easyflash_init() — EasyFlash is initialised by the BK system
 *   at boot (armino startup sequence).  Calling easyflash_init() a second time
 *   is unsafe; we therefore make _st_init a documented no-op.
 *
 * Return-value normalisation:
 *   bk_get_env_enhance() — returns the number of bytes read (> 0) on success,
 *     or <= 0 on failure (rino convention: ret > 0 → success, set *out_len).
 *     Normalised: ret > 0 → PLATFORM_STORAGE_OK (0); else → -1
 *     (PLATFORM_STORAGE_ERR_BACKEND is -4; the brief specifies "-1 otherwise",
 *     so we return plain -1 for binary pass/fail; callers test != 0).
 *
 *   bk_set_env_enhance() — rino passes its return through directly; the rino
 *     hal_kv_set returns the raw value without normalisation, implying the BK
 *     SDK returns 0 on success (EF_NO_ERR) and non-zero on error.
 *     Normalised: 0 → 0; non-zero → -1.
 *
 *   ef_del_env() — returns EfErrCode; EF_NO_ERR == 0.
 *     Normalised: 0 → 0; non-zero → -1.
 *     (The ops contract treats any non-zero return as error; we normalise to
 *     -1 rather than leaking EfErrCode values into the portable layer.)
 *
 * Phase-8 concern — EasyFlash header path:
 *   bk_ef.h and easyflash.h are included via bk7258_sdk.h (armino aggregator).
 *   At Phase 8 the armino build must have easy_flash in PRIV_REQUIRES
 *   (already listed in chip_bk7258/CMakeLists.txt).  If the BK SDK ships
 *   bk_get_env_enhance / ef_del_env under a different header, adjust the
 *   include here and verify signatures against the armino SDK version in use.
 * ─────────────────────────────────────────────────────────────────────────────
 */

#include "platform_storage.h"
#include "storage_ops_bk7258.h"

#include <string.h>

/* BK7258 EasyFlash API — bk_get_env_enhance, bk_set_env_enhance, ef_del_env.
 * bk_ef.h and easyflash.h are pulled in via the armino SDK aggregator header.
 * Phase-8: verify the exact header path against the armino SDK version used. */
#include "bk_ef.h"
#include "easyflash.h"

/* ── Key validation ──────────────────────────────────────────────────────── */

/**
 * @name    _bk_storage_key_valid
 * @brief   返回 key 是否合法（非 NULL、非空字符串）。
 * @param   key  待检查的键名字符串。
 * @retval  1 合法，0 非法。
 */
static int _bk_storage_key_valid(const char *key)
{
    return (key != NULL) && (key[0] != '\0');
}

/* ── ops implementations ─────────────────────────────────────────────────── */

/**
 * @name    _st_init
 * @brief   Storage 初始化（no-op）。EasyFlash 由 BK 系统在启动时初始化，
 *          此处无需也不应重复调用 easyflash_init()。
 * @param   ctx  未使用（EasyFlash 全局平坦 KV，无命名空间概念）。
 * @retval  0 始终成功。
 */
static int _st_init(void *ctx)
{
    (void)ctx;
    /* EasyFlash initialised by armino boot sequence; explicit call not needed. */
    return 0;
}

/**
 * @name    _st_read
 * @brief   从 EasyFlash 读取 key 对应的 blob 数据。
 * @param   ctx      未使用。
 * @param   key      键名，不得为 NULL 或空字符串。
 * @param   buf      接收缓冲区，不得为 NULL。
 * @param   buf_len  缓冲区字节数。
 * @param   out_len  成功时写入实际读取字节数，可为 NULL。
 * @retval  0 成功；-1 失败（key 无效、buf 为 NULL 或 EasyFlash 返回错误）。
 *
 * Return-value convention:
 *   bk_get_env_enhance() returns the number of bytes read (> 0) on success.
 *   We map ret > 0 → 0 (PLATFORM_STORAGE_OK), all other cases → -1.
 */
static int _st_read(void *ctx, const char *key, void *buf, size_t buf_len, size_t *out_len)
{
    (void)ctx;

    if (!_bk_storage_key_valid(key) || (buf == NULL))
    {
        return PLATFORM_STORAGE_ERR_INVALID_ARG;
    }

    int ret = bk_get_env_enhance(key, buf, (int)buf_len);
    if (ret > 0)
    {
        if (out_len)
        {
            *out_len = (size_t)ret;
        }
        return 0;
    }
    return -1;
}

/**
 * @name    _st_write
 * @brief   向 EasyFlash 写入 key-value blob。
 * @param   ctx   未使用。
 * @param   key   键名，不得为 NULL 或空字符串。
 * @param   data  数据指针，当 len > 0 时不得为 NULL。
 * @param   len   数据字节数。
 * @retval  0 成功；-1 失败。
 *
 * Return-value convention:
 *   bk_set_env_enhance() returns 0 (EF_NO_ERR) on success, non-zero on error.
 *   Normalised: 0 → 0; non-zero → -1.
 */
static int _st_write(void *ctx, const char *key, const void *data, size_t len)
{
    (void)ctx;

    if (!_bk_storage_key_valid(key) || ((len > 0U) && (data == NULL)))
    {
        return PLATFORM_STORAGE_ERR_INVALID_ARG;
    }

    int ret = bk_set_env_enhance(key, (void *)data, (int)len);
    return (ret == 0) ? 0 : -1;
}

/**
 * @name    _st_erase
 * @brief   从 EasyFlash 删除指定 key。
 * @param   ctx  未使用。
 * @param   key  键名，不得为 NULL 或空字符串。
 * @retval  0 成功；-1 失败（含 key 不存在）。
 *
 * Return-value convention:
 *   ef_del_env() returns EfErrCode; EF_NO_ERR == 0.
 *   Normalised: 0 → 0; non-zero → -1 (EfErrCode not leaked into portable layer).
 */
static int _st_erase(void *ctx, const char *key)
{
    (void)ctx;

    if (!_bk_storage_key_valid(key))
    {
        return PLATFORM_STORAGE_ERR_INVALID_ARG;
    }

    int ret = (int)ef_del_env(key);
    return (ret == 0) ? 0 : -1;
}

/**
 * @name    _st_deinit
 * @brief   Storage 反初始化（no-op）。EasyFlash 无需显式 deinit。
 * @param   ctx  未使用。
 */
static void _st_deinit(void *ctx)
{
    (void)ctx;
}

/* ── Ops table + getter ──────────────────────────────────────────────────── */

static const platform_storage_ops_t s_bk7258_storage_ops =
{
    .init   = _st_init,
    .read   = _st_read,
    .write  = _st_write,
    .erase  = _st_erase,
    .deinit = _st_deinit,
};

/**
 * @name    Bk_Storage_Ops_Get
 * @brief   返回 BK7258 EasyFlash storage ops 单例指针。
 * @retval  指向 static const platform_storage_ops_t 的指针。
 */
const platform_storage_ops_t *Bk_Storage_Ops_Get(void)
{
    return &s_bk7258_storage_ops;
}
