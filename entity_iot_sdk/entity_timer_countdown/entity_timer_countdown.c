//entity_timer_countdown.c
#include "entity_timer_countdown.h"

#include "entity_iot_func.h"

static Entity_Timer_Func_t Entity_Timer_Func;

/**
*@名称 		Entity_Timer_Func_Init
*@功能 		定时函数
*@参数 		Entity_Timer_Cbs_t *cbs
*@返回值 	void
*@使用说明	
*/
void Entity_Timer_Func_Init(Entity_Timer_Func_t *cbs)
{
    Entity_Timer_Func = *cbs;
}

/**
*@名称 		Entity_Get_System_Run_Time_Ms
*@功能 		获取系统运行时间ms
*@参数 		void
*@返回值 	uint32_t
*@使用说明	
*/
uint32_t Entity_Get_System_Run_Time_Ms(void)
{
    return Entity_Timer_Func.Get_System_Run_Time_Ms();
}

/**
*@名称 		Entity_System_Timer_Countdown_Ms
*@功能 		设置MS定时时间 
*@参数 		Entity_Timer_t *timer, unsigned int timeout_ms
*@返回值 	void
*@使用说明	
*/
void Entity_System_Timer_Countdown_Ms(Entity_Timer_t *timer, unsigned int timeout_ms)
{
    Entity_Timer_Func.System_Timer_Countdown_Ms(timer, timeout_ms);
}

/**
*@名称 		Entity_System_Timer_Countdown_Ms
*@功能 		设置S定时时间 
*@参数 		Entity_Timer_t *timer, unsigned int timeout_ms
*@返回值 	void
*@使用说明	
*/
void Entity_System_Timer_Countdown(Entity_Timer_t *timer, unsigned int timeout)
{
    Entity_Timer_Func.System_Timer_Countdown(timer, timeout);
}

/**
*@名称 		Entity_System_Timer_Remain
*@功能 		剩余时间 MS
*@参数 		Entity_Timer_t *timer
*@返回值 	uint32_t
*@使用说明	
*/
uint32_t Entity_System_Timer_Remain(Entity_Timer_t *timer)
{
    return Entity_Timer_Func.System_Timer_Remain(timer);
}

/**
*@名称 		Entity_System_Timer_Expired
*@功能 		定时是否到达
*@参数 		Entity_Timer_t *timer
*@返回值 	bool
*@使用说明	
*/
bool Entity_System_Timer_Expired(Entity_Timer_t *timer)
{
    return Entity_Timer_Func.System_Timer_Expired(timer);
}













