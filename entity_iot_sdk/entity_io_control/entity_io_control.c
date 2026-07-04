//entity_io_control.c
#include "entity_io_control.h"

#include "entity_log.h"

static Entity_Io_Control_Funcs_t Entity_Io_Control_Funcs;

/**
*@名称 		Entity_Io_Control_Funcs_Init
*@功能 		初始化IO操作接口
*@参数 		void
*@返回值 	Entity_Io_Control_Funcs_t *cbs
*@使用说明	
*/
void Entity_Io_Control_Funcs_Init(Entity_Io_Control_Funcs_t *funcs)
{
    Entity_Io_Control_Funcs = *funcs;
}


/**
*@名称 		Entity_Get_Key_Num
*@功能 		获取按键数量
*@参数 		void
*@返回值 	unsigned char 
*@使用说明	
*/
unsigned char Entity_Get_Key_Num(void)
{
	if(Entity_Io_Control_Funcs.Get_Key_Num)
		return Entity_Io_Control_Funcs.Get_Key_Num();
	return 0;
}

/**
*@名称 		Entity_Get_Key_Value
*@功能 		获取按键状态
*@参数 		unsigned char index	
*@返回值 	unsigned char 
*@使用说明	
*/
unsigned char Entity_Get_Key_Value(unsigned char index)
{
	if(Entity_Io_Control_Funcs.Get_Key_Value)
		return Entity_Io_Control_Funcs.Get_Key_Value(index);
	return 0;
}









