// bsp_sync_bk7258.h — BK7258 chip backend：臨界區原語宣告。
//
// 範圍（Task 2.2）：宣告 Bsp_Critical_* 四個函式，供 Task 6.1 bind 檔
// #include 後將其賦值至 Entity_Critical_* 函式指針。
//
// Bsp_Spinlock_* 不在此宣告——它們已由 platform_os/bsp_sync.h 統一宣告；
// 本頭僅補充 bsp_sync.h 未涵蓋的 Bsp_Critical_* 系列。
//
// Task 6.1 用法：
//   #include "bsp_sync_bk7258.h"
//   func->Critical_Create = Bsp_Critical_Create;
//   func->Critical_Delete = Bsp_Critical_Delete;
//   func->Critical_Enter  = Bsp_Critical_Enter;
//   func->Critical_Exit   = Bsp_Critical_Exit;

#ifndef BSP_SYNC_BK7258_H
#define BSP_SYNC_BK7258_H

#include "entity_iot_func.h"   /* Entity_Critical_t typedef (void*) + uint32_t */

#ifdef __cplusplus
extern "C" {
#endif

// ── 臨界區（Critical Section）────────────────────────────────────────────────
//
// 簽名與 Entity_Critical_*_f 函式指針完全對齊，使 Task 6.1 可直接賦值：
//   Entity_Critical_Create_f  = uint32_t (*)(Entity_Critical_t *pcs)
//   Entity_Critical_Delete_f  = uint32_t (*)(Entity_Critical_t *pcs)
//   Entity_Critical_Enter_f   = void     (*)(Entity_Critical_t *pcs)
//   Entity_Critical_Exit_f    = void     (*)(Entity_Critical_t *pcs)
// 其中 Entity_Critical_t = void*，故 pcs 為 void**。

uint32_t Bsp_Critical_Create(Entity_Critical_t *pcs);
uint32_t Bsp_Critical_Delete(Entity_Critical_t *pcs);
void     Bsp_Critical_Enter (Entity_Critical_t *pcs);
void     Bsp_Critical_Exit  (Entity_Critical_t *pcs);

#ifdef __cplusplus
}
#endif

#endif /* BSP_SYNC_BK7258_H */
