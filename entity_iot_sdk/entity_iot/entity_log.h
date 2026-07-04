//entity_log.h
#pragma once

#include <stdarg.h>

typedef enum
{
    ENTITY_LOG_LEVEL_ERROR,
    ENTITY_LOG_LEVEL_WARN, 
    ENTITY_LOG_LEVEL_INFO,
    ENTITY_LOG_LEVEL_DEBUG,
    ENTITY_LOG_LEVEL_MAX,
}Entity_Log_Level_e;

typedef struct 
{
    int Log_Level;
    int (*Print)(const char *format, va_list args);
}Entity_Log_Info_t;


// 获取日志水平
Entity_Log_Level_e Get_Entity_Log_Level(void);

//设置日志水平
void Set_Entity_Log_Level(Entity_Log_Level_e level);

//日志输出
void Entity_Log_Print(Entity_Log_Level_e level, const char *format, ...);

//设置日志参数
void Entity_Log_Info_Init(Entity_Log_Info_t *info);

//将HEX以字符串形式输出
void Entity_Log_Dump_Hex_To_String(Entity_Log_Level_e log_level, char *title, unsigned char *hex_array, unsigned short len);

//打印长字符串
void Entity_Log_Long_String(Entity_Log_Level_e log_level, char *title, char *string, unsigned short len);

#define ENTITY_LOGD(fmt, ...)    Entity_Log_Print(ENTITY_LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)
#define ENTITY_LOGI(fmt, ...)    Entity_Log_Print(ENTITY_LOG_LEVEL_INFO, fmt, ##__VA_ARGS__)
#define ENTITY_LOGW(fmt, ...)    Entity_Log_Print(ENTITY_LOG_LEVEL_WARN, fmt, ##__VA_ARGS__)
#define ENTITY_LOGE(fmt, ...)    Entity_Log_Print(ENTITY_LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)




















