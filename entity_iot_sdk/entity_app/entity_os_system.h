//entity_os_system.h
#pragma once

#include "entity_authorization.h"
#include "entity_iot_func.h"
#include "entity_iot_cloud.h"

//SDK系统初始化
int Entity_Iot_System_Init(const char *pid, const char *product_secret, const char *dev_version, const char *sub_version, Entity_Triple_Info_t *test_triple);

//手动进入配网
void Entity_Config_Net_Manual_Start(void);

//手动进入配网，不执行系统重启
void Entity_Manual_Config_Net_Process(unsigned char need_clear);

//设备本地重置的处理接口
void Entity_Manual_Reset_Process(unsigned char need_clear);
