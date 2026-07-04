//bsp_isr.c
#include "bsp_isr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

Bsp_Status_t Bsp_Sema_Give_From_ISR(Bsp_Sema_t *sema, bool *need_yield)
{
    if (sema == NULL || *sema == NULL)
    {
        return BSP_ERR_INVALID_ARG;
    }
    /* 非 PSRAM:slot 直接持原生 handle(对齐 Bsp_Set_Semaphore 的 *psema 用法) */
    SemaphoreHandle_t handle = (SemaphoreHandle_t)(*sema);
    BaseType_t hp_woken = pdFALSE;
    BaseType_t ok = xSemaphoreGiveFromISR(handle, &hp_woken);
    if (need_yield != NULL)
    {
        *need_yield = (hp_woken == pdTRUE);
    }
    return (ok == pdTRUE) ? BSP_OK : BSP_FAIL;
}

void Bsp_Yield_From_ISR(bool need_yield)
{
    if (need_yield)
    {
        portYIELD_FROM_ISR();
    }
}
