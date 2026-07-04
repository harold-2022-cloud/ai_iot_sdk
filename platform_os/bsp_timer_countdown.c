//bsp_timer_countdown.c
#include "bsp_timer_countdown.h"
#include "bsp_system.h"


/**
*@名称 		Bsp_System_Timer_Countdown_Ms
*@功能 		设置MS定时时间 
*@参数 		void *timer, unsigned int timeout_ms
*@返回值 	void
*@使用说明	
*/
void Bsp_System_Timer_Countdown_Ms(void *timer, unsigned int timeout_ms)
{
    Bsp_Timer_t *bsp_timer = (Bsp_Timer_t *)timer;
    bsp_timer->End_Time_Ms = Bsp_Get_Run_Time_Ms();
    bsp_timer->End_Time_Ms += timeout_ms;
}

/**
*@名称 		Bsp_System_Timer_Countdown_Ms
*@功能 		设置S定时时间 
*@参数 		void *timer, unsigned int timeout_ms
*@返回值 	void
*@使用说明	
*/
void Bsp_System_Timer_Countdown(void *timer, unsigned int timeout)
{
    Bsp_Timer_t *bsp_timer = (Bsp_Timer_t *)timer;
    bsp_timer->End_Time_Ms = Bsp_Get_Run_Time_Ms();
    bsp_timer->End_Time_Ms += timeout * 1000;
}

/**
*@名称 		Bsp_System_Timer_Remain
*@功能 		剩余时间 MS
*@参数 		void *timer
*@返回值 	uint32_t
*@使用说明	
*/
uint32_t Bsp_System_Timer_Remain(void *timer)
{
    Bsp_Timer_t *bsp_timer = (Bsp_Timer_t *)timer;
    uint32_t now_time = Bsp_Get_Run_Time_Ms();
    if(bsp_timer->End_Time_Ms <= now_time)
        return 0;
    return bsp_timer->End_Time_Ms - now_time;
}

/**
*@名称 		Bsp_System_Timer_Expired
*@功能 		定时是否到达
*@参数 		void *timer
*@返回值 	bool
*@使用说明	
*/
bool Bsp_System_Timer_Expired(void *timer)
{
    Bsp_Timer_t *bsp_timer = (Bsp_Timer_t *)timer;
    uint32_t now_ts;
    now_ts = Bsp_Get_Run_Time_Ms();
    if ((now_ts - bsp_timer->End_Time_Ms) < (UINT32_MAX / 2))
		return 1;
	else
		return 0;
}


/**
*@名称 		Bsp_System_Time_Left
*@功能 		倒计时剩余时间 MS
*@参数 		uint32_t t_end, uint32_t t_now
*@返回值 	uint32_t
*@使用说明	
*/
uint32_t Bsp_System_Time_Left(uint32_t t_end, uint32_t t_now)
{
	int32_t left;
	left = (t_end - t_now);
	if (left < 0)
		left = 0;
	return  left;
}






