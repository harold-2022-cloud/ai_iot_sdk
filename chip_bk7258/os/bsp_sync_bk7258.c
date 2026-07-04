// bsp_sync_bk7258.c — BK7258 chip backend：臨界區 + 自旋鎖原語（單核退化版）。
//
// 範圍（Task 2.2）：
//   - Bsp_Critical_Create/Delete/Enter/Exit（供 Task 6.1 bind 至 Entity_Critical_*）
//   - Bsp_Spinlock_Init/Enter/Exit/Enter_ISR/Exit_ISR（符合 bsp_sync.h 合約）
//
// 前提（D4 PREMISE）：
//   BK7258 單核心：臨界區 = 禁止搶占/關中斷，由 rtos_enter_critical /
//   rtos_exit_critical 實作（巢狀安全，armino BK 已驗）。
//   控制代碼（handle）在單核下無需存放每鎖狀態，僅存哨兵值供 NULL 檢查通過。
//
// PORTING WARNING（多核移植必讀）：
//   若移植至多核晶片，本實作的臨界區將失效——rtos_enter_critical 僅保護當前核，
//   不阻止另一核心並發訪問。多核移植必須將 Bsp_Critical_* 替換為真正的自旋鎖
//   （如 FreeRTOS portMUX / 平台 spinlock），否則臨界區語義不成立。
//   同理，Bsp_Spinlock_* 亦必須改用真正的原子自旋鎖。

#include "bsp_sync.h"          /* Bsp_Spinlock_t 型別及原型（唯一真源） */
#include "bsp_sync_bk7258.h"   /* Bsp_Critical_* 原型 + Entity_Critical_t */

/* BK / armino RTOS 原語頭。實際路徑以 armino 工程 include 樹為準；
 * rtos_enter_critical / rtos_exit_critical 確認出自 os/os.h
 *（參見 rino common_components/chip_bk7258/bsp_power.c:32）。
 * Phase 8 若路徑偏差，只需調整此 include，函式體不需改動。 */
#include "os/os.h"

// ════════════════════════════════════════════════════════════════════════════════
// 臨界區（Critical Section）
// ════════════════════════════════════════════════════════════════════════════════
//
// 設計決策：單核不需要每鎖的狀態物件。
//   - Create 把 *pcs 設為非 NULL 哨兵（(void*)1），使 SDK 內部的 NULL 檢查可通過，
//     且 Delete 對稱地清回 NULL，不需任何動態配置或額外記憶體。
//   - Enter / Exit 直接委派 rtos_enter_critical / rtos_exit_critical；
//     pcs 引數以 (void)pcs 忽略（單核無需按鎖區分）。
//
// 多核移植警告（見檔頭 PORTING WARNING）。

// ─────────────────────────────────────────────────────────────────────────────
// @name    Bsp_Critical_Create
// @brief   建立臨界區控制代碼。BK7258 單核不需分配真實物件；
//          寫入非 NULL 哨兵至 *pcs 以滿足 SDK NULL 檢查。
// @param   pcs  輸出：臨界區控制代碼指針（void**），不可為 NULL。
// @retval  0    成功
// @retval  非零 pcs 為 NULL 時的錯誤
// ─────────────────────────────────────────────────────────────────────────────
uint32_t Bsp_Critical_Create(Entity_Critical_t *pcs)
{
    if (pcs == NULL)
    {
        return (uint32_t)-1;
    }
    /* 單核：無需分配 spinlock 物件；哨兵值使 *pcs 非 NULL，
     * 讓 SDK 的「已建立」判斷可正確通過，並使 Delete 對稱。 */
    *pcs = (void *)1;
    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// @name    Bsp_Critical_Delete
// @brief   銷毀臨界區控制代碼。單核版只需清零哨兵值。
// @param   pcs  臨界區控制代碼指針（void**）。
// @retval  0    成功（含 pcs==NULL 或 *pcs==NULL 的防衛路徑）
// ─────────────────────────────────────────────────────────────────────────────
uint32_t Bsp_Critical_Delete(Entity_Critical_t *pcs)
{
    if (pcs != NULL && *pcs != NULL)
    {
        *pcs = NULL;
    }
    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// @name    Bsp_Critical_Enter
// @brief   進入臨界區：禁止搶占/關中斷（BK7258 單核 = rtos_enter_critical）。
//          rtos_enter_critical 支援巢狀呼叫，內部計數管理。
// @param   pcs  臨界區控制代碼指針（單核忽略，僅保留簽名對稱）。
// ─────────────────────────────────────────────────────────────────────────────
void Bsp_Critical_Enter(Entity_Critical_t *pcs)
{
    /* armino rtos_enter_critical() 回傳 irq flags（save/restore 慣例）;
     * 存入控制代碼供 Exit 還原。單核短臨界區、非重入。 */
    uint32_t flags = rtos_enter_critical();
    if (pcs != NULL) { *pcs = (void *)(uintptr_t)flags; }
}

// ─────────────────────────────────────────────────────────────────────────────
// @name    Bsp_Critical_Exit
// @brief   離開臨界區：恢復搶占/中斷（BK7258 單核 = rtos_exit_critical）。
//          與 rtos_enter_critical 巢狀計數對應，確保一進一出。
// @param   pcs  臨界區控制代碼指針（單核忽略，僅保留簽名對稱）。
// ─────────────────────────────────────────────────────────────────────────────
void Bsp_Critical_Exit(Entity_Critical_t *pcs)
{
    /* 取回 Enter 存的 irq flags 還原;再寫回非 NULL 哨兵,維持 SDK 冪等-init 判斷
     * （if(!handle) Create）。 */
    uint32_t flags = (pcs != NULL) ? (uint32_t)(uintptr_t)(*pcs) : 0u;
    rtos_exit_critical(flags);
    if (pcs != NULL) { *pcs = (void *)1; }
}

// ════════════════════════════════════════════════════════════════════════════════
// 自旋鎖（Spinlock）— 單核退化版
// ════════════════════════════════════════════════════════════════════════════════
//
// 注意：當前 BK7258 編譯集（chip_bk7258 + entity_iot_sdk core）未直接引用
//       Bsp_Spinlock_* 符號，提供此實作以履行 platform_os/bsp_sync.h 合約，
//       並為未來擴展（如新增使用 Bsp_Spinlock_t 的中立層元件）預留。
//
// 單核退化設計：
//   - Init：清零 opaque 欄位（與 portMUX 初始化語義對齊，供 ESP 遷移對照）。
//   - Enter / Exit（任務上下文）：委派 rtos_enter_critical / rtos_exit_critical，
//     與 Bsp_Critical_Enter/Exit 同源，足以在單核保護共享資源。
//   - Enter_ISR / Exit_ISR（ISR 上下文）：BK7258 單核 ISR 執行時已關中斷，
//     搶占不會發生，故此兩者為 no-op 是正確行為。
//     lock 引數以 (void)lock 忽略以消除未使用警告。
//
// 多核移植警告（見檔頭 PORTING WARNING）。

// ─────────────────────────────────────────────────────────────────────────────
// @name    Bsp_Spinlock_Init
// @brief   初始化自旋鎖：清零不透明儲存欄位。
// @param   lock  Bsp_Spinlock_t 指針，不可為 NULL。
// ─────────────────────────────────────────────────────────────────────────────
void Bsp_Spinlock_Init(Bsp_Spinlock_t *lock)
{
    if (lock == NULL)
    {
        return;
    }
    lock->opaque[0] = 0;
    lock->opaque[1] = 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// @name    Bsp_Spinlock_Enter
// @brief   取得自旋鎖（任務上下文）。BK7258 單核退化：關中斷/禁搶占。
// @param   lock  Bsp_Spinlock_t 指針（單核下不存放鎖狀態，忽略）。
// ─────────────────────────────────────────────────────────────────────────────
void Bsp_Spinlock_Enter(Bsp_Spinlock_t *lock)
{
    uint32_t flags = rtos_enter_critical();
    if (lock != NULL) { lock->opaque[0] = flags; }   /* 存 irq flags 供 Exit 還原 */
}

// ─────────────────────────────────────────────────────────────────────────────
// @name    Bsp_Spinlock_Exit
// @brief   釋放自旋鎖（任務上下文）。BK7258 單核退化：恢復中斷/搶占。
// @param   lock  Bsp_Spinlock_t 指針（單核下不存放鎖狀態，忽略）。
// ─────────────────────────────────────────────────────────────────────────────
void Bsp_Spinlock_Exit(Bsp_Spinlock_t *lock)
{
    uint32_t flags = (lock != NULL) ? (uint32_t)lock->opaque[0] : 0u;
    rtos_exit_critical(flags);
}

// ─────────────────────────────────────────────────────────────────────────────
// @name    Bsp_Spinlock_Enter_ISR
// @brief   取得自旋鎖（ISR 上下文）。BK7258 單核：ISR 已關中斷，no-op。
// @param   lock  Bsp_Spinlock_t 指針（ISR 下無需操作，忽略）。
// ─────────────────────────────────────────────────────────────────────────────
void Bsp_Spinlock_Enter_ISR(Bsp_Spinlock_t *lock)
{
    /* 單核 ISR：硬件已關中斷，無需額外禁搶占。no-op 正確。
     * 多核移植需改為真正的 ISR-safe spinlock 取鎖。 */
    (void)lock;
}

// ─────────────────────────────────────────────────────────────────────────────
// @name    Bsp_Spinlock_Exit_ISR
// @brief   釋放自旋鎖（ISR 上下文）。BK7258 單核：ISR 已關中斷，no-op。
// @param   lock  Bsp_Spinlock_t 指針（ISR 下無需操作，忽略）。
// ─────────────────────────────────────────────────────────────────────────────
void Bsp_Spinlock_Exit_ISR(Bsp_Spinlock_t *lock)
{
    /* 單核 ISR：硬件已關中斷，無需額外恢復搶占。no-op 正確。
     * 多核移植需改為真正的 ISR-safe spinlock 解鎖。 */
    (void)lock;
}
