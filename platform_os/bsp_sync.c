//bsp_sync.c
#include "bsp_sync.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

_Static_assert(sizeof(portMUX_TYPE) <= sizeof(((Bsp_Spinlock_t *)0)->opaque),
               "Bsp_Spinlock_t.opaque too small for portMUX_TYPE");

void Bsp_Spinlock_Init(Bsp_Spinlock_t *lock)
{
    portMUX_TYPE init = portMUX_INITIALIZER_UNLOCKED;
    *(portMUX_TYPE *)lock->opaque = init;
}

void Bsp_Spinlock_Enter(Bsp_Spinlock_t *lock)
{
    portENTER_CRITICAL((portMUX_TYPE *)lock->opaque);
}

void Bsp_Spinlock_Exit(Bsp_Spinlock_t *lock)
{
    portEXIT_CRITICAL((portMUX_TYPE *)lock->opaque);
}

void Bsp_Spinlock_Enter_ISR(Bsp_Spinlock_t *lock)
{
    portENTER_CRITICAL_ISR((portMUX_TYPE *)lock->opaque);
}

void Bsp_Spinlock_Exit_ISR(Bsp_Spinlock_t *lock)
{
    portEXIT_CRITICAL_ISR((portMUX_TYPE *)lock->opaque);
}
