//entity_ble_app.h
#pragma once



/* 云端下发topic */
#define ENTITY_BLE_PROPERTY_SET_TOPIC                     "thing.property.set"
#define ENTITY_BLE_PROPERTY_GET_TOPIC                     "thing.property.get"
#define ENTITY_BLE_NETWORK_SET_TOPIC                      "thing.network.set"
#define ENTITY_BLE_NETWORK_GETWIFIS_TOPIC                 "thing.network.getwifis"
#define ENTITY_BLE_DEV_INFO_GET_TOPIC                     "device.information.get"
#define ENTITY_BLE_THING_MODEL_GET_TOPIC                  "thing.model.get.response"
#define ENTITY_BLE_GROUP_TIME_TOPIC                       "time.response"
#define ENTITY_BLE_OTA_UPGRADE_INIT_TOPIC                 "ota.upgrade.initiate"
#define ENTITY_BLE_OTA_FILE_INFO_TOPIC                    "ota.file.info"
#define ENTITY_BLE_OTA_FILE_OFFSET_TOPIC                  "ota.file.offset"
#define ENTITY_BLE_OTA_FILE_DATA_TOPIC                    "ota.file.data"
#define ENTITY_BLE_OTA_COMPLETE_TOPIC                     "ota.complete"
#define ENTITY_BLE_DATA_CLEAR_TOPIC                       "device.data.clear"


/* 回复云端topic */
#define ENTITY_BLE_PROPERTY_SET_RESPONSE_TOPIC            "thing.property.set.response"
#define ENTITY_BLE_PROPERTY_GET_RESPONSE_TOPIC            "thing.property.get.response"
#define ENTITY_BLE_NETWORK_SET_RESPONSE_TOPIC             "thing.network.set.response"
#define ENTITY_BLE_NETWORK_GETWIFIS_RESPONSE_TOPIC        "thing.network.getwifis.response"
#define ENTITY_BLE_DEV_INFO_GET_RESPONSE_TOPIC            "device.information.get.response"
#define ENTITY_BLE_THING_PROPERTY_REPORT                  "thing.property.report"
#define ENTITY_BLE_GROUP_RESPONSE_TIME_TOPIC              "time"
#define ENTITY_BLE_OTA_UPGRADE_INIT_RESPONSE_TOPIC        "ota.upgrade.initiate.response"
#define ENTITY_BLE_OTA_FILE_INFO_RESPONSE_TOPIC           "ota.file.info.response"
#define ENTITY_BLE_OTA_FILE_OFFSET_RESPONSE_TOPIC         "ota.file.offset.response"
#define ENTITY_BLE_OTA_FILE_DATA_RESPONSE_TOPIC           "ota.file.data.response"
#define ENTITY_BLE_OTA_COMPLETE_RESPONSE_TOPIC            "ota.complete.response"
#define ENTITY_BLE_DATA_CLEAR_RESPONSE_TOPIC              "device.data.clear.response"



typedef int (*Entity_Ble_App_Send_Data_f)(unsigned char *send_data, unsigned short len);



void Entity_Ble_App_Msg_Process(unsigned char *msg, unsigned short msg_len);

void Entity_Ble_App_Register_Send_Data_Callback(Entity_Ble_App_Send_Data_f cb);








