//entity_timer_countdown.h
#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct  
{
    unsigned int End_Time_Ms;
}Entity_Timer_t;


typedef struct 
{
    uint32_t (*Get_System_Run_Time_Ms)(void);
    void (*System_Timer_Countdown_Ms)(void *timer, unsigned int timeout_ms);
    void (*System_Timer_Countdown)(void *timer, unsigned int timeout);
    uint32_t (*System_Timer_Remain)(void *timer);
    bool (*System_Timer_Expired)(void *timer);

}Entity_Timer_Func_t;


//Timer板载接口函数初始化
void Entity_Timer_Func_Init(Entity_Timer_Func_t *cbs);

//获取系统运行时间ms
uint32_t Entity_Get_System_Run_Time_Ms(void);

//设置MS定时时间 
void Entity_System_Timer_Countdown_Ms(Entity_Timer_t *timer, unsigned int timeout_ms);

//设置S定时时间 
void Entity_System_Timer_Countdown(Entity_Timer_t *timer, unsigned int timeout);

//剩余时间 MS
uint32_t Entity_System_Timer_Remain(Entity_Timer_t *timer);

//定时是否到达
bool Entity_System_Timer_Expired(Entity_Timer_t *timer);


