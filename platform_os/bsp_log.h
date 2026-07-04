//bsp_log.h
#pragma once

#include <stdarg.h>


#define LOG_LEVEL_OFF		-1
#define LOG_LEVEL_ERROR		0
#define LOG_LEVEL_WARN		1
#define LOG_LEVEL_INFO		2
#define LOG_LEVEL_DEBUG		3

#define CURRNT_LOG_LEVEL	LOG_LEVEL_DEBUG


int Bsp_Vprint(const char *format, va_list param_list);

//日志输出
void Bsp_Printf(int level, const char *format, ...);

//将HEX以字符串形式输出
void Bsp_Dump_Hex_To_String(unsigned char log_level, char *title, unsigned char *hex_array, unsigned short len);


#if (CURRNT_LOG_LEVEL >= LOG_LEVEL_DEBUG)
    #define Log_Debug(fmt, ...)	    Bsp_Printf(LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)
#else
    #define Log_Debug(fmt, ...)
#endif

#if (CURRNT_LOG_LEVEL >= LOG_LEVEL_INFO)
    #define Log_Info(fmt, ...)	    Bsp_Printf(LOG_LEVEL_INFO, fmt, ##__VA_ARGS__)
#else
    #define Log_Info(fmt, ...)
#endif

#if (CURRNT_LOG_LEVEL >= LOG_LEVEL_WARN)
    #define Log_Warn(fmt, ...)	    Bsp_Printf(LOG_LEVEL_WARN, fmt, ##__VA_ARGS__)
#else
    #define Log_Warn(fmt, ...)
#endif

#if (CURRNT_LOG_LEVEL >= LOG_LEVEL_ERROR)
    #define Log_Error(fmt, ...)	    Bsp_Printf(LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)
#else
    #define Log_Error(fmt, ...)
#endif
