//entity_io_control.h
#pragma once





typedef struct 
{
    unsigned char (*Get_Key_Num)(void);
    unsigned char (*Get_Key_Value)(unsigned char index);
}Entity_Io_Control_Funcs_t;

//初始化IO操作接口
void Entity_Io_Control_Funcs_Init(Entity_Io_Control_Funcs_t *funcs);

//获取按键数量
unsigned char Entity_Get_Key_Num(void);

//获取按键唤醒状态
unsigned char Entity_Get_Key_Value(unsigned char index);



