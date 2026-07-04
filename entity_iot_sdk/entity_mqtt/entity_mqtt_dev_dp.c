//entity_mqtt_dev_dp.c
#include "entity_mqtt_dev_dp.h"

#include "entity_mqtt_app.h"

#include "entity_log.h"
#include "entity_wifi.h"
#include "entity_iot_func.h"
#include "entity_dev_info.h"
#include "entity_mqtt_event_report.h"

#include "com_utils.h"
#include "cJSON.h"

#include <sys/time.h>
#include <string.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

static __attribute__((unused)) const char *accessModeBuff[] = {"r", "w", "rw"}; // 只上发-只下发-可上报可下发

#define PROP_ENUM_USE_VALUE     //枚举采用值的方式
//#define DP_PROPERTIES_REPORT_ALL_ONETIME    //一次上报所有DP属性, 暂不支持


/**
*@名称 		Value_String_To_Hex
*@功能 		将字符串转成HEX值
*@参数 		const char value_h, char value_l 高低4BIT对应的字符
*@返回值 	unsigned char HEX值
*@使用说明	
*/
static unsigned char Value_String_To_Hex(const char *value_str)
{
    unsigned char value_bcd[2];
    for(int i=0; i<2; i++)
    {
        if(value_str[i] >= '0' && value_str[i] <= '9')
            value_bcd[i] = value_str[i]-'0';
        else if(value_str[i] >= 'A' && value_str[i] <= 'Z')
           value_bcd[i] = value_str[i]-'A'+10;
        else if(value_str[i] >= 'a' && value_str[i] <= 'z')
           value_bcd[i] = value_str[i]-'a'+10;
        else
            return 0;
    }
    return value_bcd[0]*16+value_bcd[1];
}

/**
*@名称 		Byte_Stream_String_To_Hex_Array
*@功能 		将字符串转成HEX数组
*@参数 		const char *stream, unsigned short stream_len, unsigned char *byte_array
*@返回值 	
*@使用说明	
*/
static void Byte_Stream_String_To_Hex_Array(const char *stream, unsigned short stream_len, unsigned char *byte_array)
{
    for(int i=0; i<(stream_len/2); i++) 
    {
        byte_array[i] = Value_String_To_Hex(&stream[i*2]);
    }
}


/**
*@名称 		Thing_Model_Get_BusiId_Type_By_Identifier
*@功能 		根据标识获取物模型中的BID,TYPE
*@参数 		cJSON *model_Properties, const char* identifier, uint8_t *busiId, uint8_t *type
*@返回值 	int
*@使用说明	
*/
int Thing_Model_Get_BusiId_Type_By_Identifier(cJSON *model_Properties, const char* identifier, uint8_t *busiId, uint8_t *type) 
{
    if (!model_Properties || !identifier|| !busiId || !type)
    {
        return -1;
    }
    int count = cJSON_GetArraySize(model_Properties);//获取物模型属性列表
   
    for (int i = 0; i < count; i++)//遍历属性列表
    {
        cJSON *item = cJSON_GetArrayItem(model_Properties, i);//获取当前属性内容
        if(item == NULL)   
            continue; 

        cJSON *identifier_cjson = cJSON_GetObjectItem(item, "i");
        if (!cJSON_IsString(identifier_cjson))
        {
            continue;
        }
        if (strcmp(identifier_cjson->valuestring, identifier) == 0)//找到属性唯一标识符 MINI版
        {
            cJSON *busiId_cjson = cJSON_GetObjectItem(item, "b");//mini
            if(busiId_cjson && cJSON_IsString(busiId_cjson) && strlen(busiId_cjson->valuestring))
            {
                *busiId = atoi(busiId_cjson->valuestring);//获取属性业务ID
            }
            cJSON *dataType_cjson = cJSON_GetObjectItem(item, "t");//获取类型 MINI版
            if(dataType_cjson && cJSON_IsString(dataType_cjson) && strlen(dataType_cjson->valuestring))
            {
                *type = THING_MODEL_DATA_TYPE_STR2ID(dataType_cjson->valuestring);
            }                   
            return 0;
        }
    }
    return -1;
}

/**
*@名称 		Thing_Model_Get_BusiId_Type_By_Identifier
*@功能 		根据标识符及枚举字符中获取物模型中的枚举值
*@参数 		cJSON *Properties, const char* identifier, char *enum_string, unsigned short *enum_value
*@返回值 	int
*@使用说明	
*/
int Thing_Model_Get_Enum_Value_By_Identifier_Enum_String(cJSON *Properties, const char* identifier, char *enum_string, unsigned short *enum_value) 
{
    if (!Properties || !identifier|| !enum_string || !enum_value)
    {
        return -1;
    }
    int count = cJSON_GetArraySize(Properties);//获取物模型属性列表
   
    for (int i = 0; i < count; i++)//遍历属性列表
    {
        cJSON *arrayJson = cJSON_GetArrayItem(Properties, i);//获取当前属性内容
        if(arrayJson == NULL)   
            continue; 

        cJSON *identifier_cjson = cJSON_GetObjectItem(arrayJson, "i");
        if (!cJSON_IsString(identifier_cjson))
        {
            continue;
        }
        if (strcmp(identifier_cjson->valuestring, identifier) == 0)//找到属性唯一标识符 MINI版
        {

            cJSON *specs = cJSON_GetObjectItem(arrayJson, "s");
            cJSON *enums = cJSON_GetObjectItem(specs, "enums");
            int enumLen = cJSON_GetArraySize(enums);
            for (int i = 0; i<enumLen; i++)
            {
                cJSON *items = cJSON_GetArrayItem(enums, i);
                if (cJSON_IsString(items) && strcmp(enum_string, items->valuestring) == 0)
                {
                    *enum_value = i;
                    ENTITY_LOGD("enum string:%s, index:%d\r\n", enum_string, *enum_value);
                    return 0;
                }
            }
            return -1;
        }
    }
    return -1;
}

/**
*@名称 		Get_Dp_Value_By_Cjson_Type
*@功能 		根据CJSON类型获取值
*@参数 		cJSON *Properties, cJSON *object, uint8_t type, Dp_Obj_t* dp_obj
*@返回值 	int
*@使用说明	枚举类型需要用来物模型
*/
int Get_Dp_Value_By_Cjson_Type(cJSON *Properties, cJSON *object, uint8_t type, Dp_Obj_t* dp_obj)
{
    int ret;
    
    switch (type)
    {
        case PROP_BOOL:
            if (!cJSON_IsBool(object) && !cJSON_IsNumber(object))
                return -1;
            dp_obj->Value.Dp_Bool = object->valueint ? 1 : 0;
            dp_obj->Len = 1;
            break;
        case PROP_INT:
            if (!cJSON_IsNumber(object))
                return -1;
            dp_obj->Value.Dp_Int = object->valueint;
            dp_obj->Len = 4;
            break;
        case PROP_FLOAT:
            break;
        case PROP_ENUM:
        case PROP_FAULT:
        {
            if (!cJSON_IsString(object))
                return -1;
            unsigned short enum_value=0;
            ret = Thing_Model_Get_Enum_Value_By_Identifier_Enum_String(Properties, object->string, object->valuestring, &enum_value);
            if(ret == 0)
            {
                dp_obj->Value.Dp_Enum = enum_value; 
                dp_obj->Len = 2;
            }
            break;
        }
        case PROP_TEXT:
        {
            if (!cJSON_IsString(object))
                return -1;
            unsigned short dp_data_len = strlen(object->valuestring);
            dp_obj->Value.Dp_Str = (char *)Entity_Mem_Malloc(dp_data_len + 1);
            if (dp_obj->Value.Dp_Str == NULL)
            {
                ENTITY_LOGE("dp_str malloc fail,is null\r\n");
                return -1;
            }
            else
            {
                strcpy(dp_obj->Value.Dp_Str, object->valuestring);
                dp_obj->Len = dp_data_len;
            }
            break;
        }
        case PROP_RAW:
        {
            if (!cJSON_IsString(object))
                return -1;
            unsigned short dp_data_len = strlen(object->valuestring);
            dp_obj->Value.Dp_Raw = (unsigned char *)Entity_Mem_Malloc(dp_data_len / 2);
            if (dp_obj->Value.Dp_Raw == NULL)
            {
                ENTITY_LOGE("dp_raw malloc fail,is null\r\n");
                return -1;
            }
            else
            {
                dp_obj->Len = dp_data_len / 2;
                Byte_Stream_String_To_Hex_Array(object->valuestring, dp_data_len, (unsigned char*)dp_obj->Value.Dp_Raw);
            }
            break;
        }
        case PROP_STRUCT:
            break;
        case PROP_ARRAY:
            break;
        default:
            ENTITY_LOGE("data type is faill!!!\r\n");
            return -1;  
    }
    return 0;
}

                
/**
*@名称 		Entity_Dev_Dp_Property_Parse
*@功能 		设备的DP点属性解析
*@参数 		Entity_Mqtt_Cmd_Parse_Cbs_t *cbs
*@返回值 	Dp_Obj_Collect_t*
*@使用说明	
*/
Dp_Obj_Collect_t* Entity_Dev_Dp_Property_Parse(cJSON *properties)
{
    int ret;
    cJSON *model_properties = Entity_Mqtt_Get_Dev_Thing_Model_Cjson();//获取设备的物模型 CJSON结构体
    if(!model_properties)//没有物模型
    {
        ENTITY_LOGE("no model_properties\r\n");
        return NULL;
    }
    if(!properties)
    {
        ENTITY_LOGE("properties is null\r\n");
        return NULL;  
    }
        
    int array_size = cJSON_GetArraySize(properties);//获取JSON属性组大小
    if(array_size == 0)
        return NULL;

    //准备提取所有DP点信息的存储空间
    Dp_Obj_Collect_t *Dp_Obj_Collect = Entity_Dp_Obj_Collect_Mem_Malloc(array_size);
    if(Dp_Obj_Collect == NULL)
    {
        ENTITY_LOGE("Dp_Obj_Collect malloc faild\r\n");
        return NULL;
    }
  
    uint8_t dpid, type;
    uint8_t vaild_cnt = 0;
    for (int i=0; i<array_size; i++)
    {
        cJSON *item = cJSON_GetArrayItem(properties, i);//遍历属性，查找DP信息
        Dp_Obj_t *dp_obj =  &Dp_Obj_Collect->Dp_Objs[vaild_cnt];

        //根据属性字段名称查找物模型busiId, type，得到dpid,type， 
        ret = Thing_Model_Get_BusiId_Type_By_Identifier(model_properties, item->string, &dpid, &type);
        if(ret != 0)//物模型中未查到
        {
            ENTITY_LOGE("not find:%s\r\n", item->string);
            continue;
        }   
             
        dp_obj->Dpid = dpid;//获取网关物模型中的属性DPID
        dp_obj->Type = type;//获取网关物模型中的属性数据类型
        if(Get_Dp_Value_By_Cjson_Type(model_properties, item, type, dp_obj) != 0)   
        {
            ENTITY_LOGE("Get_Dp_Value_By_Cjson_Type faild. type:%d\r\n", item->string, type);
            continue;
        }
            
        dp_obj->Name = Utils_Strdup(item->string);//属性字段名称, 后面释放资源
        vaild_cnt++;
    } 
    Dp_Obj_Collect->Dp_Num = vaild_cnt;
    if(Dp_Obj_Collect->Dp_Num == 0)
    {
        Entity_Mem_Free(Dp_Obj_Collect->Dp_Objs);
        Entity_Mem_Free(Dp_Obj_Collect);
        return NULL;
    }
    return Dp_Obj_Collect;
}

/**
*@名称 		Entity_Dp_Obj_Mem_Free
*@功能 		释放DP空间
*@参数 		Dp_Obj_Collect_t *Dp_Obj_Collect
*@返回值 	void
*@使用说明	
*/
void Entity_Dp_Obj_Mem_Free(Dp_Obj_t* dp_obj)
{
    if(!dp_obj) 
        return;

    if (dp_obj->Type == PROP_TEXT && dp_obj->Value.Dp_Str)
    {
        Entity_Mem_Free(dp_obj->Value.Dp_Str); 
    }
    else if (dp_obj->Type == PROP_RAW && dp_obj->Value.Dp_Raw)
    {
        Entity_Mem_Free(dp_obj->Value.Dp_Raw); 
    }
    if(dp_obj->Name)
        Entity_Mem_Free(dp_obj->Name);
}

/**
*@名称 		Entity_Dp_Obj_Collect_Mem_Free
*@功能 		释放DP集合的空间
*@参数 		Dp_Obj_Collect_t *Dp_Obj_Collect
*@返回值 	void
*@使用说明	
*/
void Entity_Dp_Obj_Collect_Mem_Free(Dp_Obj_Collect_t *Dp_Obj_Collect)
{
    if(Dp_Obj_Collect)
    {
        if(Dp_Obj_Collect->Dp_Objs)
        {
            for(int i=0; i<Dp_Obj_Collect->Dp_Num; i++) 
            {
                Entity_Dp_Obj_Mem_Free(&Dp_Obj_Collect->Dp_Objs[i]);
            }
            Entity_Mem_Free(Dp_Obj_Collect->Dp_Objs);
        }
        Entity_Mem_Free(Dp_Obj_Collect);
    }
}
 
/**
*@名称 		Entity_Dp_Obj_Collect_Mem_Malloc
*@功能 		申请DP集合的空间
*@参数 		unsigned int dp_num
*@返回值 	Dp_Obj_Collect_t *
*@使用说明	
*/
Dp_Obj_Collect_t *Entity_Dp_Obj_Collect_Mem_Malloc(unsigned int dp_num)
{
    if (0 == dp_num)
    {
        ENTITY_LOGE("dp num is 0, not need malloc\r\n");
        return NULL;
    }
    
    Dp_Obj_Collect_t *dp_obj_collect = (Dp_Obj_Collect_t *)Entity_Mem_Malloc(sizeof(Dp_Obj_Collect_t));//申请一个DP集合
    if (NULL == dp_obj_collect)
    {
        ENTITY_LOGE("malloc Dp_Obj_Collect_t failed!\r\n");
        return NULL;
    }
    memset(dp_obj_collect, 0, sizeof(Dp_Obj_Collect_t));
    dp_obj_collect->Dp_Objs = Entity_Mem_Calloc(1, sizeof(Dp_Obj_t)*dp_num);//申请空间存储参数
    if (NULL == dp_obj_collect->Dp_Objs)
    {
        ENTITY_LOGE("malloc Dp_Obj_t failed!\r\n");
        Entity_Mem_Free(dp_obj_collect);
        return NULL;
    }
    
    dp_obj_collect->Dp_Num = dp_num;
    dp_obj_collect->Cur_Index = 0;
    return dp_obj_collect;
}

/**
*@名称 		Entity_Dp_Bool_Frame_Load
*@功能 		布尔数据类型加入DP点集合
*@参数 		Dp_Obj_Collect_t *dp_collect, unsigned char dpid, const char *identifier, unsigned char dp_bool
*@返回值 	void
*@使用说明	
*/
void Entity_Dp_Bool_Frame_Load(Dp_Obj_Collect_t *dp_collect, unsigned char dpid, const char *identifier, unsigned char dp_bool)
{
    if (NULL == dp_collect)
    {
        ENTITY_LOGE("dp_collect is null\r\n");
        return;
    }
    if (dp_collect->Cur_Index >= dp_collect->Dp_Num)
    {
        ENTITY_LOGE("dp space is full\r\n");
        return;
    }
    Dp_Obj_t *dp_obj = &(dp_collect->Dp_Objs[dp_collect->Cur_Index]);
    dp_obj->Type = PROP_BOOL;
    dp_obj->Dpid = dpid;
    dp_obj->Name = Utils_Strdup(identifier);
    dp_obj->Value.Dp_Bool = dp_bool;
    dp_collect->Cur_Index++;
}

/**
*@名称 		Entity_Dp_Enum_Frame_Load
*@功能 		 枚举数据类型加入DP点集合
*@参数 		Dp_Obj_Collect_t *dp_collect, unsigned char dpid, const char *identifier, unsigned char dp_enum
*@返回值 	void
*@使用说明	
*/
void Entity_Dp_Enum_Frame_Load(Dp_Obj_Collect_t *dp_collect, unsigned char dpid, const char *identifier, unsigned char dp_enum)
{
     if (NULL == dp_collect)
    {
        ENTITY_LOGE("dp_collect is null\r\n");
        return;
    }
    if (dp_collect->Cur_Index >= dp_collect->Dp_Num)
    {
        ENTITY_LOGE("dp space is full\r\n");
        return;
    }
    Dp_Obj_t *dp_obj = &(dp_collect->Dp_Objs[dp_collect->Cur_Index]);
    dp_obj->Type = PROP_ENUM;
    dp_obj->Dpid = dpid;
    dp_obj->Name = Utils_Strdup(identifier);
#ifdef PROP_ENUM_USE_VALUE
    dp_obj->Value.Dp_Enum = dp_enum;
#else
    char buf[10]={0};
    sprintf(buf,"%d", dp_enum);
    dp_obj->Value.Dp_Str = Utils_Strdup(buf);
#endif
    dp_collect->Cur_Index++;
}

/**
*@名称 		Entity_Dp_Int_Frame_Load
*@功能 		整数数据类型加入DP点集合
*@参数 		Dp_Obj_Collect_t *dp_collect, unsigned char dpid, const char *identifier,  int dp_int
*@返回值 	void
*@使用说明	
*/
void Entity_Dp_Int_Frame_Load(Dp_Obj_Collect_t *dp_collect, unsigned char dpid, const char *identifier, int dp_int)
{
    if (NULL == dp_collect)
    {
        ENTITY_LOGE("dp_collect is null\r\n");
        return;
    }
    if (dp_collect->Cur_Index >= dp_collect->Dp_Num)
    {
        ENTITY_LOGE("dp space is full\r\n");
        return;
    }
    Dp_Obj_t *dp_obj = &(dp_collect->Dp_Objs[dp_collect->Cur_Index]);
    dp_obj->Type = PROP_INT;
    dp_obj->Dpid = dpid;
    dp_obj->Name = Utils_Strdup(identifier);
    dp_obj->Value.Dp_Int = dp_int;
    dp_collect->Cur_Index++;
}

/**
*@名称 		Entity_Dp_Int_Frame_Load
*@功能 		文本数据类型加入DP点集合
*@参数 		Dp_Obj_Collect_t *dp_collect, unsigned char dpid, const char *identifier,  const char *dp_str
*@返回值 	void
*@使用说明	
*/
void Entity_Dp_String_Frame_Load(Dp_Obj_Collect_t *dp_collect, unsigned char dpid, const char *identifier, const char *dp_str)
{
    if (NULL == dp_collect)
    {
        ENTITY_LOGE("dp_collect is null\r\n");
        return;
    }
    if (dp_collect->Cur_Index >= dp_collect->Dp_Num)
    {
        ENTITY_LOGE("dp space is full\r\n");
        return;
    }
    Dp_Obj_t *dp_obj = &(dp_collect->Dp_Objs[dp_collect->Cur_Index]);
    dp_obj->Type = PROP_TEXT;
    dp_obj->Dpid = dpid;
    dp_obj->Name = Utils_Strdup(identifier);
   
    if (dp_str)
    {
        dp_obj->Value.Dp_Str = Utils_Strdup(dp_str);
        if (NULL == dp_obj->Value.Dp_Str)
        {
            ENTITY_LOGE("dp_str is null, Utils_Strdup fail\r\n");
            return;
        }
    }
    else
    {
        dp_obj->Value.Dp_Str = NULL;
    }
    dp_collect->Cur_Index++;
}

/**
*@名称 		Entity_Dp_Raw_Frame_Load
*@功能 		透传数据类型加入DP点集合
*@参数 		Dp_Obj_Collect_t *dp_collect, unsigned char dpid, const char *identifier,  const unsigned char *dp_raw, unsigned short length
*@返回值 	void
*@使用说明	
*/
void Entity_Dp_Raw_Frame_Load(Dp_Obj_Collect_t *dp_collect, unsigned char dpid, const char *identifier, const unsigned char *dp_raw, unsigned short length)
{
    if (NULL == dp_collect)
    {
        ENTITY_LOGE("dp_collect is null\r\n");
        return;
    }
    if (dp_collect->Cur_Index >= dp_collect->Dp_Num)
    {
        ENTITY_LOGE("dp space is full\r\n");
        return;
    }
    Dp_Obj_t *dp_obj = &(dp_collect->Dp_Objs[dp_collect->Cur_Index]);
    dp_obj->Type = PROP_RAW;
    dp_obj->Dpid = dpid;
    dp_obj->Name = Utils_Strdup(identifier);

    if (dp_raw)
    {
        dp_obj->Value.Dp_Raw = (unsigned char *)Entity_Mem_Malloc(length);
        if (NULL == dp_obj->Value.Dp_Raw)
        {
            ENTITY_LOGE("dp_raw is null, malloc fail\r\n");
            return;
        }
        dp_obj->Len = length;
        memcpy(dp_obj->Value.Dp_Raw, dp_raw, length);
    }
    else
    {
        dp_obj->Value.Dp_Raw = NULL;
    }
    dp_collect->Cur_Index++;
}

/**
*@名称 		Entity_Dp_Collect_Report
*@功能 		DP点集合属性上报
*@参数 		Dp_Obj_Collect_t *dp_collect
*@返回值 	void
*@使用说明	
*/
void Entity_Dp_Collect_Report(Dp_Obj_Collect_t *dp_collect)
{
#ifdef DP_PROPERTIES_REPORT_ALL_ONETIME
    cJSON *properties = cJSON_CreateArray();
     if(properties == NULL)
    {
        ENTITY_LOGE("%s cJSON_CreateArray faild\r\n", __func__);
        return;
    }
#endif
    for (int i=0; i<dp_collect->Cur_Index; i++)
    {
        char *identifier = dp_collect->Dp_Objs[i].Name;
        int type = dp_collect->Dp_Objs[i].Type;
        
        cJSON *object = cJSON_CreateObject();
        if (!object)
        {
            ENTITY_LOGE("%s cJSON_CreateObject error\r\n", __func__);
            return;
        }
        switch (type)
        {
        case PROP_BOOL:
            cJSON_AddBoolToObject(object, identifier, dp_collect->Dp_Objs[i].Value.Dp_Bool);
            break;
        case PROP_INT:
            cJSON_AddNumberToObject(object, identifier, dp_collect->Dp_Objs[i].Value.Dp_Int);
            break;
        case PROP_FLOAT:
            break;
        case PROP_ENUM://枚举是字符串
#ifdef PROP_ENUM_USE_VALUE
            cJSON_AddNumberToObject(object, identifier, dp_collect->Dp_Objs[i].Value.Dp_Enum);
#else
            cJSON_AddStringToObject(object, identifier, dp_collect->Dp_Objs[i].Value.Dp_Str);
#endif
            break;
        case PROP_FAULT:
            break;
        case PROP_TEXT:
            cJSON_AddStringToObject(object, identifier, dp_collect->Dp_Objs[i].Value.Dp_Str);
            break;
        case PROP_RAW:
        {
            unsigned short length = dp_collect->Dp_Objs[i].Len;
            char *dp_raw = (char *)Entity_Mem_Malloc(length * 2);
            if (NULL == dp_raw)
            {
                ENTITY_LOGE("%s raw malloc fail\r\n", __func__);
                break;
            }
            memset(dp_raw, 0, length * 2);
            Hex_Array_To_String(dp_collect->Dp_Objs[i].Value.Dp_Raw, length, dp_raw);
            cJSON_AddStringToObject(object, identifier, dp_raw);
            Entity_Mem_Free(dp_raw);
        }
            break;
        case PROP_STRUCT:
            break;
        case PROP_ARRAY:
            break;
        default:
            ENTITY_LOGE("data type is faill!!!\r\n");
            break;
        }
        if (cJSON_GetArraySize(object))
        {
#ifdef DP_PROPERTIES_REPORT_ALL_ONETIME
            cJSON_AddItemToArray(properties, object);
#else
            Entity_Mqtt_Event_Property_Report(object, 0);
#endif
        }
    }
#ifdef DP_PROPERTIES_REPORT_ALL_ONETIME
    Entity_Mqtt_Event_Property_Report(properties, 0);
#endif
}
 
