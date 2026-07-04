#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ENTITY_INTERFACE_REFERENCE_TEMPLATE_VERSION "2026-07-01"

typedef enum
{
    ENTITY_INTERFACE_REF_WORK_NONE = 0,
    ENTITY_INTERFACE_REF_WORK_REPORT_PROPERTY,
    ENTITY_INTERFACE_REF_WORK_ENTER_PROVISIONING,
    ENTITY_INTERFACE_REF_WORK_DEVICE_ACCESS_REQUEST,
    ENTITY_INTERFACE_REF_WORK_DEVICE_ACCESS_STOP,
    ENTITY_INTERFACE_REF_WORK_MANUAL_RESET,
    ENTITY_INTERFACE_REF_WORK_MANUAL_CONFIG_NET,
    ENTITY_INTERFACE_REF_WORK_PRODUCT_CUSTOM,
} Entity_Interface_Reference_Work_Type_e;

typedef struct
{
    Entity_Interface_Reference_Work_Type_e type;
    uint32_t value;
    void *payload;
} Entity_Interface_Reference_Work_t;

typedef struct
{
    int result;
    const char *rtc_token;
    const char *channel_name;
    const char *app_id;
    int uid;
} Entity_Interface_Reference_Ai_Token_Result_t;

typedef struct
{
    void (*on_dev_status_change)(unsigned char status);
    void (*on_dp_obj_received)(void *dp_obj);
    void (*on_ai_token_result)(const Entity_Interface_Reference_Ai_Token_Result_t *result, void *user);
    void *ai_token_user;
} Entity_Interface_Reference_Hooks;

typedef struct
{
    void (*init_system_imports)(void);
    void (*init_periph_imports)(void);
    void (*init_mqtt_imports)(void);

    void (*report_property)(uint32_t value, void *payload, void *user);
    void (*enter_provisioning)(unsigned char need_clear, void *user);
    bool (*request_device_access)(void *user);
    void (*stop_device_access)(void *user);
    void (*manual_reset)(unsigned char need_clear, void *user);
    void (*manual_config_net)(unsigned char need_clear, void *user);
    void (*product_custom)(const Entity_Interface_Reference_Work_t *work, void *user);
    void *user;
} Entity_Interface_Reference_Config_t;

int Entity_Interface_Reference_Init(const Entity_Interface_Reference_Config_t *config);
int Entity_Interface_Reference_Work_Enqueue(const Entity_Interface_Reference_Work_t *work);

void Entity_Product_Hooks_Register(const Entity_Interface_Reference_Hooks *hooks);
void Entity_Product_Hooks_On_Dev_Status(unsigned char status);
void Entity_Product_Hooks_On_Dp_Received(void *dp_obj);
void Entity_Product_Hooks_On_Ai_Token_Result(const Entity_Interface_Reference_Ai_Token_Result_t *result);

void Entity_Iot_Interface_Init(void);
bool Entity_Device_Access_Export_Interface(void);
void Entity_Device_Access_Stop_Export_Interface(void);
void Entity_Manual_Reset_Export_Interface(unsigned char need_clear);
void Entity_Manual_Config_Net_Export_Interface(unsigned char need_clear);

#ifdef __cplusplus
}
#endif
