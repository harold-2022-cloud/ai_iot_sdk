//bsp_timer_countdown.h
#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint32_t End_Time_Ms;
}Bsp_Timer_t;


//设置MS定时时间
void Bsp_System_Timer_Countdown_Ms(void *timer, unsigned int timeout_ms);

//设置S定时时间
void Bsp_System_Timer_Countdown(void *timer, unsigned int timeout);

//剩余时间 MS
uint32_t Bsp_System_Timer_Remain(void *timer);

//定时是否到达
bool Bsp_System_Timer_Expired(void *timer);

//倒计时剩余时间 MS
uint32_t Bsp_System_Time_Left(uint32_t t_end, uint32_t t_now);
