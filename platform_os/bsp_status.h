//bsp_status.h — 统一错误码(OS HAL 契约)
#pragma once

typedef enum
{
    BSP_OK              =  0,
    BSP_ERR_INVALID_ARG = -1,
    BSP_ERR_NO_MEM      = -2,
    BSP_ERR_TIMEOUT     = -3,
    BSP_ERR_NOT_FOUND   = -4,
    BSP_ERR_STATE       = -5,
    BSP_ERR_FULL        = -6,
    BSP_ERR_EMPTY       = -7,
    BSP_ERR_UNSUPPORTED = -8,
    BSP_FAIL            = -100,
} Bsp_Status_t;
