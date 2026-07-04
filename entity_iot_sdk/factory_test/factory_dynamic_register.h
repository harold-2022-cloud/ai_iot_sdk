//factory_dynamic_register.h
#pragma once

typedef struct 
{
    void (*Set_Burn_Info_Callback)(char *msg_id, void *info); 
}Register_Mqtt_Msg_Cbs_t;


//获取动态注册MQTT消息处理回调
Register_Mqtt_Msg_Cbs_t *Get_Register_Mqtt_Msg_Cbs(void);

//删除动态注册MQTT任务
int Register_Test_Mqtt_Client_Task_Stop(void);

//动态注册MQTT任务
int Register_Test_Mqtt_Client_Task_Start(char *mqtt_host, unsigned short mqtt_port, Register_Mqtt_Msg_Cbs_t *cbs);

//mqtt事件应答消息处理
void Register_Mqtt_Msg_Event_Respone_Parse_Process(char *msg);

















