//com_utils.h
#pragma once

#include "entity_log.h"
#include "entity_iot_func.h"


// 将 16 位整数转换为字节数组（小端序）
#define INT16_TO_LE_BYTES(val, bytes)  {(bytes)[0] = (unsigned char)((val) & 0xFF); (bytes)[1]=(unsigned char)(((val) >> 8) & 0xFF);}

	// 将 小端序 字节数组转换为 16 位整数
#define LE_BYTES_TO_INT16(bytes)       ((unsigned short)((bytes)[0] | ((bytes)[1] << 8)))

// 将 16 位整数转换为字节数组（大端序）
#define INT16_TO_BE_BYTES(val, bytes)  {(bytes)[0] = (unsigned char)(((val) >> 8) & 0xFF); (bytes)[1] = (unsigned char)((val) & 0xFF);}

	// 将 大端序 字节数组转换为 16 位整数
#define BE_BYTES_TO_INT16(bytes)       ((unsigned short)(((bytes)[0] << 8) | (bytes)[1]))


// 将 32 位整数转换为字节数组（小端序）
#define INT32_TO_LE_BYTES(val, bytes)  { (bytes)[0] = (unsigned char)((val) & 0xFF); \
                                         (bytes)[1] = (unsigned char)(((val) >> 8) & 0xFF); \
                                         (bytes)[2] = (unsigned char)(((val) >> 16) & 0xFF); \
                                         (bytes)[3] = (unsigned char)(((val) >> 24) & 0xFF); }
// 将 小端序 字节数组转换为 32 位整数
#define LE_BYTES_TO_INT32(bytes)       ((unsigned int)((bytes)[0] | ((bytes)[1] << 8) | ((bytes)[2] << 16)) | ((bytes)[3] << 24))


// 将 32 位整数转换为字节数组（大端序）
#define INT32_TO_BE_BYTES(val, bytes)  { (bytes)[0] = (unsigned char)(((val) >> 24) & 0xFF); \
                                         (bytes)[1] = (unsigned char)(((val) >> 16) & 0xFF); \
                                         (bytes)[2] = (unsigned char)(((val) >> 8) & 0xFF); \
                                         (bytes)[3] = (unsigned char)((val) & 0xFF); }

// 将 大端序 字节数组转换为 32 位整数
#define BE_BYTES_TO_INT32(bytes)       ((unsigned int)(((bytes)[0] << 24) | ((bytes)[1] << 16) | ((bytes)[2] << 8)) | (bytes)[3])


#ifndef ARRAY_SIZE
    #define ARRAY_SIZE(a)                  (sizeof(a) / sizeof((a)[0]))
#endif

#define FREE_MEMORY(x)                  Entity_Mem_Free(x)   



#define Custom_Assert_Check_RequireString_Return_Null(x, str)                               \
    do {                                                                                    \
        if (!(x)) {                                                                         \
            ENTITY_LOGE("%s",(str != NULL) ? str : "");                            \
            return;                                                                         \
        }                                                                                   \
    } while (0)

#define Custom_Assert_Check_RequireString_Goto(x, label, str)                               \
    do {                                                                                    \
        if (!(x)) {                                                                         \
            ENTITY_LOGE("%s",(str != NULL) ? str : "");                            \
            goto label;                                                                     \
        }                                                                                   \
    } while (0)






//无符号数据范围检查
unsigned char Unsigned_Data_Range_Check(unsigned short value, unsigned short min, unsigned short max);

//有符号数据范围检查
unsigned char Signed_Data_Range_Check(short value, short min, short max);

//平均值滤波
unsigned short Average_Filter(unsigned short *array, unsigned int len);

//直接插入排序
void Direct_insert_sort(unsigned short * array, unsigned int len);

//指数平滑滤波
void Exponential_Smoothing_Filter(unsigned char *final_value, unsigned char new_value, unsigned char factor, unsigned char force_first);

//获取绝对差值
int Get_Difference_Value(int a, int b);

//解析JSON字符串中的某个字段
int Parse_Json_Field(const char* json, const char* field, char *value);

//分隔符数量统计
int Count_Commas(const char *str, const char delimiter) ;

//将字符串按照分隔符拆分，提取第n-n+1分隔符之间的内容
int Extract_Between_Nth_Commas(const char *src_str, const char delimiter, int n, char *dec_str);

//生成随机消息ID
char *Random_MsgId(void) ;

//获取队列/数组的索引
unsigned char Array_Index_Of(const char *element, const char *array[], unsigned char length);

//u8转字节流字符串
void Hex_Array_To_String(const unsigned char* array, unsigned short length, char *str);

//将单个十六进制字符转换为其对应的数字值
int Hexchar_To_Int(char c);

//HEX字符串转字节
void Hexstr_To_Bytes(const char *hexstr, unsigned char *bytes, unsigned int bytes_len);

//敏感字串日志脱敏：只保留长度和首尾少量字符，避免泄露 token/secret/password。
void Utils_Mask_Secret(const char *src, char *dst, unsigned int dst_len);

//字符串转HEX
int Str_To_Hex(unsigned char *str, int length, char *hexStr);

//获取随机字符串
int Get_Random_Str(char *random_str, const int random_len);

//将字节数组中的字符追加到字符串中
void Hex_Array_Append_To_String(char *str, const unsigned char *pdata, unsigned short len) ;

//申请内在并复制字符串到新空间中
char *Utils_Strdup(const char *string);

//将MAC地址格式化为字符串
void Mac_Addr_Format_String(unsigned char *mac_addr, char *mac_string);
