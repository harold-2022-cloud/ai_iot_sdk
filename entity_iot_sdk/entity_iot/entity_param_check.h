//entity_param_check.h
#pragma once

#include "entity_log.h"


#define POINTER_SAFETY_CHECK_RETURN_ERR(ptr, err)          \
    do {                                                   \
        if (NULL == (ptr)) {                               \
            ENTITY_LOGE("%s-%d Invalid argument point, %s = %p\r\n", __func__, __LINE__, #ptr, ptr); \
            return (err);                                  \
        }                                                  \
    } while (0)


#define POINTER_SAFETY_CHECK_RETURN_NONE(ptr)              \
    do {                                                   \
        if (NULL == (ptr)) {                               \
            ENTITY_LOGE("%s-%d Invalid argument point, %s = %p\r\n", __func__, __LINE__,#ptr, ptr); \
            return;                                        \
        }                                                  \
    } while (0)

#define NUMBERICAL_SAFETY_CHECK_RETURN_ERR(num, err)       \
    do {                                          \
        if (0 == (num)) {                         \
            ENTITY_LOGE("%s-%d Invalid argument number, numerical 0\r\n", __func__, __LINE__); \
            return (err);                         \
        }                                         \
    } while (0)

#define NUMBERICAL_SAFETY_CHECK_RETURN_NONE(num)  \
    do {                                          \
        if (0 == (num)) {                         \
            ENTITY_LOGE("%s-%d Invalid argument number, numeric 0\r\n", __func__, __LINE__); \
            return;                               \
        }                                         \
    } while (0)



#define MALLOC_POINTER_SAFETY_CHECK_RETURN_ERR(ptr, string, err)          \
    do {                                                   \
        if (NULL == (ptr)) {                               \
            ENTITY_LOGE("%s\r\n",string);                     \
            return (err);                                  \
        }                                                  \
    } while (0)


#define MALLOC_POINTER_SAFETY_CHECK_RETURN_NONE(ptr, string)              \
    do {                                                   \
        if (NULL == (ptr)) {                               \
            ENTITY_LOGE("%s\r\n",string);                    \
            return;                                        \
        }                                                  \
    } while (0)


#define POINTER_SAFETY_CHECK_GOTO_LABEL(ptr, label)          \
    do {                                                   \
        if (NULL == (ptr)) {                               \
            ENTITY_LOGE("%s-%d Invalid argument point! %s = %p\r\n", __func__, __LINE__, #ptr, ptr); \
            goto label;                                  \
        }                                                  \
    } while (0)

#define RET_VALUE_SUCCESS_CHECK_RETURN_ERR(ret, string, err)          \
    do {                                                   \
        if (0 != (ret)) {                               \
            ENTITY_LOGE("%s\r\n",string);                     \
            return (err);                                  \
        }                                                  \
    } while (0)

#define RET_VALUE_SUCCESS_CHECK_RETURN_NONE(ret, string)          \
    do {                                                   \
        if (0 != (ret)) {                               \
            ENTITY_LOGE("%s\r\n",string);                     \
            return ;                                  \
        }                                                  \
    } while (0)












