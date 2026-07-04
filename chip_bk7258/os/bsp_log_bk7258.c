// bsp_log_bk7258.c — BK7258 chip backend：日志輸出原語。
//
// 範圍（Task 2.3）：
//   - Bsp_Vprint(format, param_list)：va_list 形態日志，供 Log Cbs .Print 綁定（Task 6.3）
//   - Bsp_Printf(level, format, ...)：variadic 日志，層級映射說明見下
//
// 設計決策：BK log API
//   使用 bk_vprintf_ext(BK_LOG_INFO, (char*)"", format, va_list)，
//   形態取自 rino common_components/chip_bk7258/hal_log_bk7258.c:15（已驗）。
//   bk_vprintf_ext 聲明出自 BK SDK log 頭（典型路徑 "components/log/log.h" 或
//   "driver/log.h"，Phase 8 以 armino 工程實際 include 路徑為準）。
//
// level 映射：
//   bsp_log.h 定義 LOG_LEVEL_ERROR(0)/WARN(1)/INFO(2)/DEBUG(3)。
//   BK SDK 定義 BK_LOG_ERROR / BK_LOG_WARN / BK_LOG_INFO / BK_LOG_DEBUG 等。
//   若兩側順序/值域一致，可直接強轉；若不確定（SDK 版本差異），統一傳 BK_LOG_INFO：
//   此為安全預設，確保控制面日志不被靜默，犧牲的僅是客戶端 level 過濾精度。
//   Phase 8 實測 armino log.h 可用 BK level 後，可啟用 _bsp_to_bk_level 精確映射。
//
// Bsp_Atomic_* 說明（Task 2.3 範圍外）：
//   platform_os/bsp_atomic.h 為 header-only GCC __atomic_* 內聯，天生中立，
//   BK7258 不需額外 .c 實作。chip_bk7258 INCLUDE_DIRS 含 platform_os 路徑即可使用。
//
// 簽名以 platform_os/bsp_log.h 為唯一真源，逐一對齊。

#include "bsp_log.h"   /* Bsp_Vprint / Bsp_Printf 原型 + LOG_LEVEL_* 常數 */
#include <stdarg.h>

/* BK log API：bk_vprintf_ext 宣告於 components/system.h；BK_LOG_INFO 於 components/log.h。 */
#include "components/system.h"   /* bk_vprintf_ext */
#include "components/log.h"      /* BK_LOG_INFO */

// ─────────────────────────────────────────────────────────────────────────────
// （可選精確映射，目前未啟用）
// 若 Phase 8 確認 BK SDK BK_LOG_* 數值可靠，取消注釋以啟用精確 level 映射：
//
// static inline int _bsp_to_bk_level(int level)
// {
//     switch (level) {
//         case LOG_LEVEL_ERROR: return BK_LOG_ERROR;
//         case LOG_LEVEL_WARN:  return BK_LOG_WARN;
//         case LOG_LEVEL_INFO:  return BK_LOG_INFO;
//         case LOG_LEVEL_DEBUG: return BK_LOG_DEBUG;
//         default:              return BK_LOG_INFO;
//     }
// }
// ─────────────────────────────────────────────────────────────────────────────

// ─────────────────────────────────────────────────────────────────────────────
// @name    Bsp_Vprint
// @brief   va_list 形態日志輸出，綁定至 Entity_Log_Info_t.Print（Task 6.3）。
//          形態與 esp32s3 端 Bsp_Vprint 一致（int (*)(const char*, va_list)）。
//          實作鏡像 rino hal_log_bk7258.c:15 的精確形態：
//          bk_vprintf_ext(BK_LOG_INFO, (char*)"", format, param_list)。
// @param   format      printf 格式字串
// @param   param_list  可變參數列表（va_list）
// @retval  0（固定，符合 Print Cb 合約）
// ─────────────────────────────────────────────────────────────────────────────
int Bsp_Vprint(const char *format, va_list param_list)
{
    /* 鏡像 rino hal_log_bk7258.c:15 精確形態：
     * level = BK_LOG_INFO（確保控制面日志不被靜默）
     * tag   = (char*)""（空標籤，格式化前綴由 SDK 自行添加）
     * Phase 8：若需要精確 level，可在呼叫端 wrap 層映射後傳入。 */
    bk_vprintf_ext(BK_LOG_INFO, (char *)"", format, param_list);
    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// @name    Bsp_Printf
// @brief   variadic 日志輸出，展開後委派 bk_vprintf_ext。
//          level 映射：目前固定傳 BK_LOG_INFO（安全預設；見檔頭 level 映射說明）。
//          Phase 8 確認 BK_LOG_* 數值後可啟用上方 _bsp_to_bk_level 精確映射。
// @param   level   日志層級（LOG_LEVEL_ERROR/WARN/INFO/DEBUG，目前僅作 void 用途）
// @param   format  printf 格式字串
// @param   ...     可變參數
// ─────────────────────────────────────────────────────────────────────────────
void Bsp_Printf(int level, const char *format, ...)
{
    /* level 目前固定映射至 BK_LOG_INFO（安全預設，確保不靜默任何控制面日志）。
     * Phase 8 啟用精確映射：將 BK_LOG_INFO 替換為 _bsp_to_bk_level(level)。 */
    (void)level;

    va_list args;
    va_start(args, format);
    bk_vprintf_ext(BK_LOG_INFO, (char *)"", format, args);
    va_end(args);
}
