//entity_mqtt_cmd_parse.h
#pragma once

#include "cJSON.h"

#include "entity_mqtt_dev_dp.h"

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

typedef void (*Mqtt_Cmd_Parse_Cb)(cJSON* root);

typedef struct 
{                       
    void (*Cmd_Reset_Parse_Cb)(unsigned char need_clear);
    void (*Cmd_Ota_Parse_Cb) (char *firmware_url);
    void (*Cmd_Clean_Data_Parse_Cb)(void);
    void (*Cmd_Property_Set_Parse_Cb)(Dp_Obj_Collect_t *);
    //待接
    Mqtt_Cmd_Parse_Cb command_update_local_config_callback;
    Mqtt_Cmd_Parse_Cb command_find_bind_callback;
    Mqtt_Cmd_Parse_Cb command_sub_property_set_callback;
    Mqtt_Cmd_Parse_Cb command_sub_delete_callback;
    Mqtt_Cmd_Parse_Cb command_local_group_set_callback;
    Mqtt_Cmd_Parse_Cb command_local_rule_exec_callback;
    Mqtt_Cmd_Parse_Cb command_wake_up_callback;
    Mqtt_Cmd_Parse_Cb command_agora_join_callback;
    Mqtt_Cmd_Parse_Cb command_ipc_cloud_open_callback;
    Mqtt_Cmd_Parse_Cb command_agora_event_callback;
    Mqtt_Cmd_Parse_Cb command_common_cmd_callback;
    Mqtt_Cmd_Parse_Cb command_local_group_update_callback;
    Mqtt_Cmd_Parse_Cb command_local_scene_update_callback;
    Mqtt_Cmd_Parse_Cb command_log_switch_callback;
    Mqtt_Cmd_Parse_Cb command_reboot_callback;
    Mqtt_Cmd_Parse_Cb command_sub_replace_callback;
    Mqtt_Cmd_Parse_Cb command_dp_bind_callback;
    Mqtt_Cmd_Parse_Cb command_local_dp_group_update_callback;
    Mqtt_Cmd_Parse_Cb command_config_settings_callback;
    
}Entity_Mqtt_Cmd_Parse_Cbs_t;


//mqtt命令消息处理回调注册
void Entity_Mqtt_Cmd_Parse_Cbs_Init(Entity_Mqtt_Cmd_Parse_Cbs_t *cbs);

//mqtt命令消息处理
void Entity_Mqtt_Msg_Cmd_parse_Process(char *msg);











