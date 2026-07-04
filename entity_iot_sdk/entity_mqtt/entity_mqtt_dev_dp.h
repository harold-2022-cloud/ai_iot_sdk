//entity_mqtt_dev_dp.h
#pragma once

#include "cJSON.h"

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

//DP点的值类型
typedef enum {
    PROP_BOOL,    //1字节数值
    PROP_INT,     //4字节数值
    PROP_FLOAT,   //4字节数值
    PROP_ENUM,    //字符串
    PROP_FAULT,   //字符串
    PROP_TEXT,    //字符串
    PROP_RAW,     //字符串，内容为 16 进制字节转换而来
    PROP_STRUCT,
    PROP_ARRAY,
    PROP_UNKNOWN=0xff,
}Dp_Prop_Type_e;

//物模型中的数据类型， 字符串转为C语言的DP类型值
#define THING_MODEL_DATA_TYPE_STR2ID(S)\
    ((0 == strcmp((S), "bool"))   ? PROP_BOOL:\
    ((0 == strcmp((S), "int"))    ? PROP_INT:\
    ((0 == strcmp((S), "float"))  ? PROP_FLOAT:\
    ((0 == strcmp((S), "enum"))   ? PROP_ENUM:\
    ((0 == strcmp((S), "fault"))  ? PROP_FAULT:\
    ((0 == strcmp((S), "text"))   ? PROP_TEXT:\
    ((0 == strcmp((S), "raw"))    ? PROP_RAW:\
    ((0 == strcmp((S), "struct")) ? PROP_STRUCT:\
    ((0 == strcmp((S), "array"))  ? PROP_ARRAY:\
                                    PROP_UNKNOWN)))))))))

////物模型中的数据类型， C语言的DP类型值转为字符串
#define THING_MODEL_DATA_TYPE_ID2STR(S)\
    ((S) == PROP_BOOL   ? "bool":\
    ((S) == PROP_INT    ? "int":\
    ((S) == PROP_FLOAT  ? "float":\
    ((S) == PROP_ENUM   ? "enum":\
    ((S) == PROP_FAULT  ? "fault":\
    ((S) == PROP_TEXT   ? "text":\
    ((S) == PROP_RAW    ? "raw":\
    ((S) == PROP_STRUCT ? "struct":\
    ((S) == PROP_ARRAY  ? "array":\
                          "unknown")))))))))

//CJSON数据类型转为物模型中的数据类型字符串
#define CJSON_DATA_TYPE_ID2STR(S)\
    ((S) == cJSON_False   ? "bool":\
    ((S) == cJSON_True   ? "bool":\
    ((S) == cJSON_Number    ? "int":\
    ((S) == cJSON_String  ? "text":\
                          "unknown"))))

//DP值的联合体
typedef union 
{
    int Dp_Int;   // valid when dp type is value
    float Dp_Float;
    unsigned short Dp_Enum;   // valid when dp type is enum
    char *Dp_Str;       // valid when dp type is str
    unsigned char *Dp_Raw;       // valid when dp type is raw
    unsigned char Dp_Bool;    // valid when dp type is bool
}Dp_Value_u;

//
typedef struct {
    char *Name;
    unsigned char Dpid;       // dp id
    unsigned char Type;       // dp type
    unsigned short Len;        //数据长度
    Dp_Value_u Value;  // 数据,可能是指针
}Dp_Obj_t; 

typedef struct {
    unsigned int Dp_Num;     //DP数量
    unsigned int Cur_Index; 
    Dp_Obj_t *Dp_Objs;  //所有DP存储区
}Dp_Obj_Collect_t;




//释放DP空间
void Entity_Dp_Obj_Mem_Free(Dp_Obj_t* dp_obj);

//释放DP集合的空间
void Entity_Dp_Obj_Collect_Mem_Free(Dp_Obj_Collect_t *Dp_Obj_Collect);

//申请DP集合的空间
Dp_Obj_Collect_t *Entity_Dp_Obj_Collect_Mem_Malloc(unsigned int dp_num);

//设备的DP点属性解析
Dp_Obj_Collect_t* Entity_Dev_Dp_Property_Parse(cJSON *properties);

//布尔数据类型加入DP点集合
void Entity_Dp_Bool_Frame_Load(Dp_Obj_Collect_t *dp_collect, unsigned char dpid, const char *identifier, unsigned char dp_bool);

//枚举数据类型加入DP点集合
void Entity_Dp_Enum_Frame_Load(Dp_Obj_Collect_t *dp_collect, unsigned char dpid, const char *identifier, unsigned char dp_enum);

//整数数据类型加入DP点集合
void Entity_Dp_Int_Frame_Load(Dp_Obj_Collect_t *dp_collect, unsigned char dpid, const char *identifier, int dp_int);

//文本数据类型加入DP点集合
void Entity_Dp_String_Frame_Load(Dp_Obj_Collect_t *dp_collect, unsigned char dpid, const char *identifier, const char *dp_str);

//透传数据类型加入DP点集合
void Entity_Dp_Raw_Frame_Load(Dp_Obj_Collect_t *dp_collect, unsigned char dpid, const char *identifier, const unsigned char *dp_raw, unsigned short length);

//DP点集合属性上报
void Entity_Dp_Collect_Report(Dp_Obj_Collect_t *dp_collect);



