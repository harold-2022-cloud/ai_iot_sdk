//bsp_log.c
#include <stdio.h>

#include "bsp_log.h"

#include "bsp_system.h"
#include "esp_log.h"   /* ESP_LOGx:封 bsp_log.h 的 mcu.h 洩漏後,esp 依賴下沉到 .c */


#define BSP_PRINTF_BUF_SIZE 4096

static const char *TAG = "bsp log";



/**
*@名称        Bsp_Vprint
*@功能        
*@参数        const char *format, va_list args
*@返回值   void
*@使用说明  
*/
int Bsp_Vprint(const char *format, va_list args)
{
    return vprintf(format, args);
}

/**
*@名称        Bsp_Printf
*@功能        日志输出
*@参数        int level, const char *format, ...
*@返回值   void
*@使用说明  
*/
void Bsp_Printf(int level, const char *format, ...)
{
#if 1
    if(level<=CURRNT_LOG_LEVEL)
    {
        char *buffer = Bsp_Psram_Calloc(1, BSP_PRINTF_BUF_SIZE);
        if(buffer == NULL)
        {
            ESP_LOGE(TAG, "Bsp_Printf malloc faild");
            return;
        }
        va_list args;
        int len;
        
        va_start(args, format);
        len = vsnprintf(buffer, BSP_PRINTF_BUF_SIZE, format, args);
        va_end(args);
        
        if (len > 0 && len < BSP_PRINTF_BUF_SIZE) {
            if(level == LOG_LEVEL_DEBUG)
                ESP_LOGI(TAG, "%s", buffer);
            else if(level == LOG_LEVEL_INFO)
                ESP_LOGI(TAG, "%s", buffer);
            else if(level == LOG_LEVEL_WARN)
                ESP_LOGW(TAG, "%s", buffer);
            else if(level == LOG_LEVEL_ERROR)
                ESP_LOGE(TAG, "%s", buffer);
            
        } 
        Bsp_Psram_Free(buffer);
    }
#else
    if(level<=CURRNT_LOG_LEVEL)
    {
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
    }
#endif
}

/**
*@名称        Bsp_Dump_Hex_To_String
*@功能        将HEX以字符串形式输出
*@参数        unsigned char log_level, char *title, unsigned char *hex_array, uint16_t len
*@返回值   void
*@使用说明  
*/
void Bsp_Dump_Hex_To_String(unsigned char log_level, char *title, unsigned char *hex_array, unsigned short len)
{
    int i;
    uint16_t offset=0;
    if(hex_array == NULL || len == 0)
        return;
    char *buffer = Bsp_Psram_Calloc(1, 1024+100);
    if(buffer == NULL)
    {
        ESP_LOGE(TAG, "Bsp_Dump_Hex_To_String malloc faild");
        return ;
    }
    
    Bsp_Printf(log_level, "%s\r\n", title);
    for(i=0; i<len; i++)
    {
        sprintf(buffer+offset, "%02x ", hex_array[i]);
        offset += 3;
        if((i+1)%16 == 0)
        {
            sprintf(buffer+offset, "\r\n");
            offset += 2;
        }
        if(offset >= 1024)
        {
            buffer[offset]=0;
            Bsp_Printf(log_level, "%s\r\n", buffer);
            offset = 0;
        }
    }
    Bsp_Printf(log_level, "%s\r\n", buffer);
    Bsp_Psram_Free(buffer);
}
