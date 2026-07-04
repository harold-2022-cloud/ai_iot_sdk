/**    
*@名称 			com_utils.c 
*@编译环境 		MDK-Lite  Version: 5.36
*@时间 			2025-01-17
*@功能 			公共函数
*/
#include "com_utils.h"

#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "entity_iot_func.h"


/**
*@名称 		Average_Filter
*@功能 		平均值滤波
*@参数 		unsigned short * array, unsigned int len
*@返回值 	unsigned short
*@使用说明	
*/
unsigned short Average_Filter(unsigned short *array, unsigned int len)
{
	unsigned int sum=0;
	
	for(int i=0; i<len; i++)
	{
		sum += array[i];
	}
	return sum/len;
}

/**
*@名称 		Direct_insert_sort
*@功能 		直接插入排序 
*@参数 		unsigned short * array, unsigned int len
*@返回值 	void
*@使用说明	时间复杂度最好：n 最差：n^2  平均 n^2 
*/
void Direct_insert_sort(unsigned short * array, unsigned int len)
{
	int i, j;
	unsigned short temp;
	
	for(i=1; i<len; i++)
	{
		temp = array[i];
		for(j=i-1; j>=0 && temp<array[j]; j--)
		{
			array[j+1] = array[j];
		}
		array[j+1] = temp;
	}
}

/**
*@名称 		Exponential_Smoothing_Filter
*@功能 		指数平滑滤波
*@参数 		uint8_t *final_value, uint8_t new_value, uint8_t factor, uint8_t force_first
*@返回值 	void
*@使用说明	factor按百分比来算 0-100
*/
void Exponential_Smoothing_Filter(uint8_t *final_value, uint8_t new_value, uint8_t factor, uint8_t force_first)
{
	static uint8_t flag_init = 0;
	if(factor > 100)
		return;
	if (force_first || !flag_init)
    {
		*final_value = new_value;
		flag_init = 1;
    }
    else
    {
		*final_value = ((unsigned int)(*final_value) * (100-factor) + (unsigned int)new_value * factor)/100;
    }
	
}

/**
*@名称 		Get_Difference_Value
*@功能 		获取绝对差值
*@参数 		int a, int b
*@返回值 	int
*@使用说明
*/
int Get_Difference_Value(int a, int b)
{
	if(a > b)
		return a-b;
	else
		return b-a;
}

/**
*@名称 		Parse_Json_Field
*@功能 		解析JSON字符串中的某个字段
*@参数 		const char* json, const char* field, char *value
*@返回值 	int 内容长度
*@使用说明
*/
int Parse_Json_Field(const char* json, const char* field, char *value) 
{
	int len=0;
    const char* p = strstr(json, field);
    if (!p) 
		return 0;
 
    p = strchr(p, ':');
    if (!p) 
		return 0;
 
    p++; // 跳过冒号
    while(*p == ' ') 
		p++; // 跳过空格
 
    if (*p == '\"') 
	{
        p++; // 跳过引号
        const char* end = strchr(p, '\"');
        if (!end) 
			return 0;
 
        len = end - p;
        memcpy(value, p, len);
        value[len] = '\0';
    } 
	else 
	{
        const char* end = strchr(p, ',');
        if (!end) 
			end = strchr(p, '}');
        if (!end) 
			end = p + strlen(p);
 
        len = end - p;
        memcpy(value, p, len);
        value[len] = '\0';
    }
	return len;
}

/**
*@名称 		Count_Commas
*@功能 		分隔符数量统计
*@参数 		const char *str, const char delimiter
*@返回值 	int 分隔符数量
*@使用说明
*/
int Count_Commas(const char *str, const char delimiter) 
{
    int count = 0;
    const char *ptr = str;
    while ((ptr = strchr(ptr, delimiter)) != NULL)// 查找分隔符位置 
	{ 
        count++;
        ptr++; // 跳过已找到的分隔符
    }
    return count;
}

/**
*@名称 		Extract_Between_Nth_Commas
*@功能 		将字符串按照分隔符拆分，提取第n-n+1分隔符之间的内容
*@参数 		const char *src_str, const char delimiter, int n, char *dec_str
*@返回值 	int 0-成功  非0-失败
*@使用说明
*/
int Extract_Between_Nth_Commas(const char *src_str, const char delimiter, int n, char *dec_str) 
{
    if (n < 0 || src_str == NULL || dec_str == NULL) 
		return -1;

    const char *start = src_str;
    const char *end = NULL;
    int count = 0;

    // 定位第n个逗号
    while (count < n) 
	{
        start = strchr(start, delimiter);//查找分隔符，若找到返回分隔符所在位置
        if (start == NULL) 
			return -1; 		// 分隔符不足n个
        start++; 			// 跳过分隔符
        count++;
    }

    // 定位第n+1个逗号
    end = strchr(start, delimiter);
    if (end == NULL)
	{	
		strcpy(dec_str, start);// 最后一个字段
		return 0; 
	}

    // 计算子串长度并复制
    int len = end - start;
    strncpy(dec_str, start, len);
    dec_str[len] = '\0';
    return 0;
}

/**
*@名称 		Random_MsgId
*@功能 		生成随机消息ID
*@参数 		void
*@返回值 	char *
*@使用说明
*/
char *Random_MsgId(void)
{
    char *buf = (char *)Entity_Mem_Malloc(17);
    char *p = buf;

    if(buf == NULL) {
        return buf;
    }

    for(int n = 0; n < 8; ++n )
    {
        int b = (int)(Entity_Rand() & 0xFF);

        switch( n )
        {
            case 6: sprintf(p, "4%x", b % 15 ); break;
            default: sprintf( p, "%02x", b ); break;
        }

        p += 2;
    }
    p--;
    *p = 0;
    return buf;
}

/**
*@名称 		Array_Index_Of
*@功能 		获取队列/数组的索引
*@参数 		const char *element, const char *array[], unsigned char length
*@返回值 	unsigned char
*@使用说明
*/
unsigned char Array_Index_Of(const char *element, const char *array[], unsigned char length)
{
    unsigned char index = -1;
    for (int i = 0; i < length; i++)
    {
        if (strcmp(element, array[i]) == 0)
        {
            index = i;
            break;
        }
    }
    return index;
}

/**
*@名称 		Hex_Array_To_String
*@功能 		u8转字节流字符串
*@参数 		const unsigned char* array, unsigned short length, char *str
*@返回值 	unsigned char
*@使用说明
*/
void Hex_Array_To_String(const unsigned char* array, unsigned short length, char *str)
{
    int index = 0;

    for (int i = 0; i < length; i++) {
        index += sprintf(str + index, "%02x", array[i]);
    } 
}


/**
*@名称 		Hexchar_To_Int
*@功能 		将单个十六进制字符转换为其对应的数字值
*@参数 		char c
*@返回值 	int
*@使用说明	
*/
int Hexchar_To_Int(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return 10 + c - 'a';
    if (c >= 'A' && c <= 'F')
        return 10 + c - 'A';
    return 0;
}

/**
*@名称 		Hexstr_To_Bytes
*@功能 		HEX字符串转字节
*@参数 		const char *hexstr, unsigned char *bytes, unsigned int bytes_len
*@返回值 	void
*@使用说明	
*/
void Hexstr_To_Bytes(const char *hexstr, unsigned char *bytes, unsigned int bytes_len)
{
    unsigned int  hexstr_len = strlen(hexstr);
    for (unsigned int i = 0; i < hexstr_len && i / 2 < bytes_len; i += 2)
    {
        bytes[i / 2] = (Hexchar_To_Int(hexstr[i]) << 4) + Hexchar_To_Int(hexstr[i + 1]);
    }
}

/**
*@名称 		Str_To_Hex
*@功能 		字符串转HEX
*@参数 		unsigned char *str, int length, char *hexStr
*@返回值 	int
*@使用说明	
*/
int Str_To_Hex(unsigned char *str, int length, char *hexStr)
{
    int index = 0;
    for (int i = 0; i < length; i++)
    {
        sprintf(hexStr + index, "%02x", str[i]);
        index += 2;
    }
    hexStr[index] = '\0';
    return 0;
}

/**
*@名称 		Get_Random_Str
*@功能 		获取随机字符串
*@参数 		char *random_str, const int random_len
*@返回值 	int
*@使用说明	
*/
int Get_Random_Str(char *random_str, const int random_len)
{
    if (random_str == NULL || random_len <= 0)
    {
        return -1;
    }

    int i, random_num, seed_str_len;
    unsigned int seed_num;
    char seed_str[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    seed_str_len = strlen(seed_str);
	//此处随机数
    //seed_num = hal_random_get();
	seed_num = Entity_Get_Time_Stamp();//修改为用时间戳做随机种子
    srand(seed_num);

    for (i = 0; i < random_len; i++)
    {
        random_num = rand() % seed_str_len;
        random_str[i] = seed_str[random_num];
    }
    random_str[random_len] = '\0';

    return 0;
}

/**
*@名称 		Hex_Array_Append_To_String
*@功能 		将字节数组中的字符追加到字符串中
*@参数 		char *str, const unsigned char *pdata, unsigned short len
*@返回值 	void
*@使用说明	
*/
void Hex_Array_Append_To_String(char *str, const unsigned char *pdata, unsigned short len) 
{
    for (int i = 0; i < len; i++) 
    {
        strncat(str, (const char *)&pdata[i], 1); // 将数组中的每个元素拼接到目标字符串
    }
}

void Utils_Mask_Secret(const char *src, char *dst, unsigned int dst_len)
{
    if (dst == NULL || dst_len == 0)
    {
        return;
    }
    if (src == NULL)
    {
        snprintf(dst, dst_len, "(null)");
        return;
    }

    size_t len = strlen(src);
    if (len == 0)
    {
        snprintf(dst, dst_len, "(empty)");
    }
    else if (len <= 8)
    {
        snprintf(dst, dst_len, "<short:%u>", (unsigned int)len);
    }
    else
    {
        snprintf(dst, dst_len, "len=%u %.2s***%s",
                 (unsigned int)len,
                 src,
                 src + len - 2U);
    }
}

/**
*@名称 		Utils_Strdup
*@功能 		申请内在并复制字符串到新空间中
*@参数 		const char *string
*@返回值 	char *
*@使用说明	
*/
char *Utils_Strdup(const char *string)
{
    unsigned int len = strlen(string);
    if(len == 0)
        return NULL;
    char *new_string = Entity_Mem_Malloc(len+1);
    if(new_string != NULL)
    {
        strcpy(new_string, string);
        new_string[len] = 0;
    }
    return new_string;
}

/**
*@名称 		Mac_Addr_Format_String
*@功能 		将MAC地址格式化为字符串
*@参数 		unsigned char *mac_addr, char *mac_string
*@返回值 	void
*@使用说明	
*/
void Mac_Addr_Format_String(unsigned char *mac_addr, char *mac_string)
{
    int i;
    for(i=0; i<5; i++)
		sprintf(mac_string+i*3, "%02x:", mac_addr[i]);
	sprintf(mac_string+i*3, "%02x", mac_addr[i]);
}
