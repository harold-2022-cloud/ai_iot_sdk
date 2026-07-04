#ifndef CHIP_BK7258_H
#define CHIP_BK7258_H

#ifdef __cplusplus
extern "C" {
#endif

/* 顯式綁定入口：armino app 早期呼叫，賦值所有 Entity_* 全域指針。
 * 顯式 bind → 不需 WHOLE_ARCHIVE：app 主動呼叫此函式，連結器不會丟棄被引用的賦值函式。 */
void Entity_Chip_Bk7258_Bind(void);

#ifdef __cplusplus
}
#endif

#endif /* CHIP_BK7258_H */
