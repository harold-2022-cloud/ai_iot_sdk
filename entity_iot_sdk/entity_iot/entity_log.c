//entity_log.c
#include "entity_log.h"

#include "entity_iot_func.h"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>


static Entity_Log_Info_t Entity_Log_Info={
    .Log_Level = ENTITY_LOG_LEVEL_DEBUG,
};

/**
*@名称 		Entity_Log_Info_Init
*@功能 		设置日志参数
*@参数 		Entity_Log_Info_t *info
*@返回值 	void
*@使用说明	
*/
void Entity_Log_Info_Init(Entity_Log_Info_t *info)
{
    if(info->Log_Level < ENTITY_LOG_LEVEL_MAX)
    {
        Entity_Log_Info.Log_Level = info->Log_Level;
    }
    if(info->Print)
        Entity_Log_Info.Print = info->Print;
}

/**
*@名称 		Set_Entity_Log_Level
*@功能 		设置日志水平
*@参数 		Entity_Log_Level_e level
*@返回值 	void
*@使用说明	
*/
void Set_Entity_Log_Level(Entity_Log_Level_e level)
{
    Entity_Log_Info.Log_Level = level;
}


/**
*@名称 		Get_Entity_Log_Level
*@功能 	    获取日志水平
*@参数 		void
*@返回值 	Entity_Log_Level_e
*@使用说明	
*/
Entity_Log_Level_e Get_Entity_Log_Level(void)
{
    return Entity_Log_Info.Log_Level;
}


/**
*@名称 		Entity_Log_Print
*@功能 		日志输出
*@参数 		Entity_Log_Level_e level, const char *format, ...
*@返回值 	void
*@使用说明	
*/
void Entity_Log_Print(Entity_Log_Level_e level, const char *format, ...)
{
	if(level <= Entity_Log_Info.Log_Level)
	{
		//Rtc_Date_Time_t *Date_Time_Info = Platform_Rtc_Get_Date_Time_Info();
		//printf("[%d-%d-%d %d:%d:%d]",
		//Date_Time_Info->Year+2000,Date_Time_Info->Month, Date_Time_Info->Day,
		//Date_Time_Info->Hour, Date_Time_Info->Minute, Date_Time_Info->Second);
		va_list args;
		va_start(args, format);
        if(Entity_Log_Info.Print)
            Entity_Log_Info.Print(format, args);
		va_end(args);
	}
}


/**
*@名称 		Entity_Log_Dump_Hex_To_String
*@功能 		将HEX以字符串形式输出
*@参数 		Entity_Log_Level_e log_level, char *title, unsigned char *hex_array, uint16_t len
*@返回值 	void
*@使用说明	
*/
void Entity_Log_Dump_Hex_To_String(Entity_Log_Level_e log_level, char *title, unsigned char *hex_array, unsigned short len)
{
	int i;
	uint16_t offset=0;
	if(hex_array == NULL || len == 0)
		return;
	char *buffer = Entity_Mem_Calloc(1, 1024+100);
	if(buffer == NULL)
	{
		return ;
	}
    Entity_Log_Print(log_level, "%s\r\n", title);
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
			Entity_Log_Print(log_level, "%s\r\n", buffer);
			offset = 0;
		}
	}
	Entity_Log_Print(log_level, "%s\r\n", buffer);
	Entity_Mem_Free(buffer);
}


/**
*@名称 		Entity_Log_Long_String
*@功能 		打印长字符串
*@参数 		Entity_Log_Level_e log_level, char *title, char *string, uint16_t len
*@返回值 	void
*@使用说明	
*/
void Entity_Log_Long_String(Entity_Log_Level_e log_level, char *title, char *string, unsigned short len)
{
	if(string == NULL || len == 0)
		return;
    Entity_Log_Print(log_level, "%s\r\n", title);
	uint16_t per_packet_len=1024;
	uint16_t current_len=per_packet_len;
	uint16_t total_len=len;
	uint16_t have_log_len=0;
	uint16_t remain_len=len;
	while(have_log_len<total_len)
	{
		char buf[1024+1]={0};
		if(remain_len < per_packet_len)
			current_len = remain_len;
		memcpy(buf, string+have_log_len, current_len);
		buf[current_len]=0;
		Entity_Log_Print(log_level, "%s", buf);
		have_log_len += current_len;
		remain_len -= current_len;
	}
	Entity_Log_Print(log_level, "\r\n");
}




