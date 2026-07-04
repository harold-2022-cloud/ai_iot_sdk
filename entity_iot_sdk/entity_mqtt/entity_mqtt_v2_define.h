//rlink_mqtt_define.h
#pragma once

#include "cJSON.h"


/* 设备主动上报 code */
#define ENTITY_EVENT_RESET_CODE                   "reset"
#define ENTITY_EVENT_TIME_CODE                    "time"
#define ENTITY_EVENT_MODEL_CODE                   "model"
#define ENTITY_EVENT_CONFIG_CODE                  "config"
#define ENTITY_EVENT_BIND_CODE                    "bind"
#define ENTITY_EVENT_INFO_CODE                    "info"
#define ENTITY_EVENT_PROPERTY_REPORT_CODE         "property_report"
#define ENTITY_EVENT_OTA_PROGRESS_CODE            "ota_progress"
#define ENTITY_EVENT_SUB_BIND_CODE                "sub_bind"
#define ENTITY_EVENT_SUB_DELETE_CODE              "sub_delete"
#define ENTITY_EVENT_SUB_LOGIN_CODE               "sub_login"
#define ENTITY_EVENT_SUB_LOGINOUT_CODE            "sub_loginout"
#define ENTITY_EVENT_SUB_MODEL_CODE               "sub_model"
#define ENTITY_EVENT_SUB_PROPERTY_REPORT_CODE     "sub_property_report"
#define ENTITY_EVENT_LOCAL_CONFIG_CODE            "local_config"
#define ENTITY_EVENT_SUB_LIST_CODE                "sub_list"
#define ENTITY_EVENT_IPC_CLOUD_CODE               "ipc_cloud"
#define ENTITY_EVENT_P2P_CONFIG_CODE              "p2p_config"
#define ENTITY_EVENT_AGORA_TOKEN_CODE             "agora_token"
#define ENTITY_EVENT_DIY_EVENT_CODE               "event"
#define ENTITY_EVENT_IPC_TOKEN_BIND_CODE          "ipc_token_bind"
#define ENTITY_EVENT_FIND_ALERT_CODE              "find_alert"
#define ENTITY_EVENT_FIND_REPORT_CODE             "find_report"
#define ENTITY_EVENT_IPC_LIVE_GET_CODE            "ipc_live_get"

#define ENTITY_EVENT_AGORA_AGENT_NFC_REPORT_CODE       "agora_agent_nfc_report"       //有NFC的设备请求声网访问信息
#define ENTITY_EVENT_AGORA_RTC_STOP_REPORT_CODE        "agora_rtc_stop_report"
#define ENTITY_EVENT_AGORA_AGENT_DEVICE_ACCESS_CODE    "agora_agent_device_access"    //无NFC的设备请求声网访问信息




/* 云端主动下发 code */
#define ENTITY_COMMAND_RESET_CODE                 "reset"
#define ENTITY_COMMAND_OTA_CODE                   "ota"
#define ENTITY_COMMAND_PING_CODE                  "ping"
#define ENTITY_COMMAND_PROPERTY_SET_CODE          "property_set"
#define ENTITY_COMMAND_UPDATE_LOCAL_CONFIG_CODE   "update_local_config"
#define ENTITY_COMMAND_FIND_BIND_CODE             "find_bind"
#define ENTITY_COMMAND_SUB_PROPERTY_SET_CODE      "sub_property_set"
#define ENTITY_COMMAND_SUB_DELETE_CODE            "sub_delete"
#define ENTITY_COMMAND_LOCAL_GROUP_SET_CODE       "local_group_set"
#define ENTITY_COMMAND_LOCAL_RULE_EXEC_CODE       "local_rule_exec"
#define ENTITY_COMMAND_WAKE_UP_CODE               "wake_up"
#define ENTITY_COMMAND_AGORA_JOIN_CODE            "agora_join"
#define ENTITY_COMMAND_IPC_CLOUD_OPEN_CODE        "ipc_cloud_open"
#define ENTITY_COMMAND_AGORA_EVENT_CODE           "agora_event"
#define ENTITY_COMMAND_COMMON_CMD_CODE            "common_cmd"
#define ENTITY_COMMAND_LOCAL_GROUP_UPDATE_CODE    "local_group_update"
#define ENTITY_COMMAND_LOCAL_SCENE_UPDATE_CODE    "local_scene_update"
#define ENTITY_COMMAND_LOG_SWITCH_CODE            "log_switch"
#define ENTITY_COMMAND_REBOOT_CODE                "reboot"
#define ENTITY_COMMAND_SUB_REPLACE_CODE           "sub_replace"
#define ENTITY_COMMAND_DP_BIND_CODE               "dp_bind"     //DP点绑定本地一键执行 20231229 add
#define ENTITY_COMMAND_LOCAL_DP_GROUP_UPDATE      "local_dp_group_update"//本地DP群组变化通知 20240111 add
#define ENTITY_COMMAND_CONFIG_SETTINGS            "config_settings"//设置设备配置 20240509 add
#define ENTITY_COMMAND_CLEAN_DATA_CODE            "clean_data"

