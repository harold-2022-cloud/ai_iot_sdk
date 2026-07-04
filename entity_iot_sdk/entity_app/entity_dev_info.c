//entity_dev_info.c
#include "entity_dev_info.h"

#include "entity_log.h"
#include "entity_config_net.h"
#include "com_utils.h"
#include "cJSON.h"

#include <string.h>
#include <stdbool.h>

static Entity_App_Param_t Entity_App_Param;
static Entity_Flash_Cbs_t Entity_Flash_Cbs;
static Entity_Dev_Cbs_t Entity_Dev_Cbs;
static unsigned char Entity_Dev_State;
static Entity_Mutex_t s_state_mutex = NULL;


/**
*@名称 		Get_Entity_Dev_State
*@功能 		获取设备当前状态
*@参数 		void
*@返回值 	unsigned char
*@使用说明	
*/
unsigned char Get_Entity_Dev_State(void)
{
    Entity_Mutex_Lock(&s_state_mutex, ENTITY_WAIT_FOREVER);
    unsigned char state = Entity_Dev_State;
    Entity_Mutex_Unlock(&s_state_mutex);
    return state;
}

/**
*@名称 		Entity_Flash_Cbs_Init
*@功能 		初始化FLASH操作接口
*@参数 		void
*@返回值 	Entity_Flash_Cbs_t *cbs
*@使用说明	
*/
void Entity_Flash_Cbs_Init(Entity_Flash_Cbs_t *cbs)
{
    Entity_Flash_Cbs = *cbs;
}

/**
*@名称 		Entity_Flash_Read_Key_Value
*@功能 		按 key 读取 NVS/Flash 键值（通用 KV 读）
*@参数 		const char *key, unsigned char *pdata, unsigned int len
*@返回值 	int —— 平台实现返回值；未绑定回调返回 -1
*@使用说明
*/
int Entity_Flash_Read_Key_Value(const char *key, unsigned char *pdata, unsigned int len)
{
    if(Entity_Flash_Cbs.Flash_Read_Key_Value_Cb)
        return Entity_Flash_Cbs.Flash_Read_Key_Value_Cb(key, pdata, len);
    return -1;
}

/**
*@名称 		Entity_Dev_Cbs_Init
*@功能 		初始化设备相关回调接口
*@参数 		Entity_Dev_Cbs_t *cbs
*@返回值 	void
*@使用说明	
*/
void Entity_Dev_Cbs_Init(Entity_Dev_Cbs_t *cbs)
{
    if (s_state_mutex == NULL)
        Entity_Mutex_Create(&s_state_mutex);
    Entity_Dev_Cbs = *cbs;
}

/**
*@名称 		Get_Entity_Dev_Cbs
*@功能 		获取初始化设备相关回调接口
*@参数 		void
*@返回值 	Entity_Dev_Cbs_t *
*@使用说明	
*/
Entity_Dev_Cbs_t *Get_Entity_Dev_Cbs(void)
{
    return &Entity_Dev_Cbs;
}

/**
*@名称 		Entity_Set_Dev_Status
*@功能 		设置设备状态
*@参数 		unsigned char status
*@返回值 	void
*@使用说明	
*/
void Entity_Set_Dev_Status(unsigned char status)
{
    Entity_Mutex_Lock(&s_state_mutex, ENTITY_WAIT_FOREVER);
    Entity_Dev_State = status;
    Entity_Mutex_Unlock(&s_state_mutex);
    /* callback 在鎖外呼叫，避免 callback 內再取鎖造成死鎖 */
    if(Entity_Dev_Cbs.State_Callback)
        Entity_Dev_Cbs.State_Callback(status);
}
/**
*@名称 		Get_Entity_App_Param
*@功能 		获取应用程序参数
*@参数 		void
*@返回值 	Entity_App_Param_t *
*@使用说明	
*/
Entity_App_Param_t *Get_Entity_App_Param(void)
{
    return &Entity_App_Param;
}

/**
*@名称 		Entity_Get_Triple_Info
*@功能 		获取三元组信息
*@参数 		void
*@返回值 	Entity_Triple_Info_t*
*@使用说明	
*/
Entity_Triple_Info_t *Entity_Get_Triple_Info(void)
{
    return &Entity_App_Param.Dev_Triple_Info.Triple_Info;
}


/**
*@名称 		Entity_Get_Config_Net_Info
*@功能 		获取配网信息
*@参数 		void
*@返回值 	Entity_Config_Net_Info_t*
*@使用说明	
*/
Entity_Config_Net_Info_t *Entity_Get_Config_Net_Info(void)
{
    return &Entity_App_Param.Dev_Config_Net_Info.Config_Net_Info;
}

/**
*@名称 		Entity_Get_Dev_Config_Net_Info
*@功能 		获取设备配网信息
*@参数 		void
*@返回值 	Entity_Dev_Config_Net_Info_t*
*@使用说明	
*/
Entity_Dev_Config_Net_Info_t *Entity_Get_Dev_Config_Net_Info(void)
{
    return &Entity_App_Param.Dev_Config_Net_Info;
}

/**
*@名称 		Get_Dev_Thing_Model_Info
*@功能 		获取物模型信息
*@参数 		void
*@返回值 	Entity_Dev_Thing_Model_Info_t *
*@使用说明	
*/
Entity_Dev_Thing_Model_Info_t *Get_Dev_Thing_Model_Info(void)
{
    return &Entity_App_Param.Dev_Thing_Model_Info;
}

/**
*@名称 		Entity_Mqtt_Get_Dev_Thing_Model_Cjson
*@功能 		获取设备保存的物模型信息接口CJSON
*@参数 		void
*@返回值 	cJSON*
*@使用说明	
*/
cJSON* Entity_Mqtt_Get_Dev_Thing_Model_Cjson(void)
{
    Entity_App_Param_t *app_param = Get_Entity_App_Param();
    return app_param->Dev_Info.Thing_Model_Properties;
}

/**
*@名称 		Entity_Get_Dev_Info
*@功能 		获取设备信息
*@参数 		void
*@返回值 	Entity_Device_Info_t*
*@使用说明	
*/
Entity_Device_Info_t *Entity_Get_Dev_Info(void)
{
    return &Entity_App_Param.Dev_Info;
}

/**
*@名称 		Entity_Get_Wifi_Info
*@功能 		获取WIFI信息
*@参数 		void
*@返回值 	Entity_Wifi_Info_t*
*@使用说明	
*/
Entity_Wifi_Info_t *Entity_Get_Wifi_Info(void)
{
    return &Entity_App_Param.Dev_Config_Net_Info.Config_Net_Info.Wifi_Info;
}

/**
*@名称 		Entity_Get_Mqtt_Info
*@功能 		获取MQTT信息
*@参数 		void
*@返回值 	Entity_Mqtt_Info_t*
*@使用说明	
*/
Entity_Mqtt_Info_t *Entity_Get_Mqtt_Info(void)
{
    return &Entity_App_Param.Dev_Config_Net_Info.Config_Net_Info.Mqtt_Info;
}


/**
*@名称 		Entity_Reset_Triple_Info_To_Flash
*@功能 		将FLASH中三元组信息恢复出厂设置
*@参数 		void
*@返回值 	int
*@使用说明	
*/
int Entity_Reset_Triple_Info_To_Flash(void)
{
    ENTITY_LOGD("%s\r\n", __func__);
    memset(&Entity_App_Param.Dev_Triple_Info, 0, sizeof(Entity_Dev_Triple_Info_t));
    Entity_App_Param.Dev_Triple_Info.Magic_Header = MAGIC_HEADER_VALUE;
    Entity_App_Param.Dev_Triple_Info.Flag_Vaild = false;
    return Entity_Save_Triple_Info_To_Flash();
}


/**
*@名称 		Entity_Triple_Data_Check
*@功能 		三元组参数检查
*@参数 		void
*@返回值 	void
*@使用说明	
*/
void Entity_Triple_Data_Check(void)
{
    Entity_Triple_Info_t *triple_info = &Entity_App_Param.Dev_Triple_Info.Triple_Info;
    if(Entity_App_Param.Dev_Triple_Info.Magic_Header != MAGIC_HEADER_VALUE)
    {
        Entity_Reset_Triple_Info_To_Flash();
    }
    else
    {
        //unsigned char uuid_len = strlen(triple_info->Uuid);
        //unsigned char secret_len = strlen(triple_info->Secret);
        //unsigned char mac_len = strlen(triple_info->Mac);
        //if(!uuid_len || !secret_len || !mac_len)
        //{
        //    Entity_Reset_Triple_Info_To_Flash();
        //}
    }
    char secret_mask[32];
    Utils_Mask_Secret(triple_info->Secret, secret_mask, sizeof(secret_mask));
    ENTITY_LOGI("%s Flag_Vaild:%d, uuid:%s, secret_mask:%s, mac:%s, pid:%s\r\n", __func__, Entity_App_Param.Dev_Triple_Info.Flag_Vaild,\
        triple_info->Uuid, secret_mask, triple_info->Mac, triple_info->Pid);
}

/**
*@名称 		Entity_Save_Triple_Info_To_Flash
*@功能 		保存三元 组信息到FLASH
*@参数 		void
*@返回值 	int
*@使用说明	
*/
int Entity_Save_Triple_Info_To_Flash(void)
{
    int ret = 0;
    ENTITY_LOGD("%s\r\n", __func__);
    if(Entity_Flash_Cbs.Flash_Write_Triple_Cb)
        ret = Entity_Flash_Cbs.Flash_Write_Triple_Cb((unsigned char *)&Entity_App_Param.Dev_Triple_Info, sizeof(Entity_Dev_Triple_Info_t));
    return ret;
}

/**
*@名称 		Entity_Read_Triple_Info_From_Flash
*@功能 		从FLASH中读三元组信息
*@参数 		void
*@返回值 	int
*@使用说明	
*/
int Entity_Read_Triple_Info_From_Flash(void)
{
    int ret = 0;
    ENTITY_LOGD("%s\r\n", __func__);
    if(Entity_Flash_Cbs.Flash_Read_Triple_Cb)
    {
        ret = Entity_Flash_Cbs.Flash_Read_Triple_Cb((unsigned char *)&Entity_App_Param.Dev_Triple_Info, sizeof(Entity_Dev_Triple_Info_t));
        if(ret <= 0)//不存在
        {
            ENTITY_LOGE("------------Triple_Info key not create! not init------------\r\n");
            Entity_Reset_Triple_Info_To_Flash();
            return 0;
        }
        else
        {
            Entity_Triple_Data_Check();
        }
    }
    return 0;
}


/**
*@名称 		Entity_Reset_Config_Net_Info_To_Flash
*@功能 		将FLASH中配网信息恢复出厂设置
*@参数 		void
*@返回值 	int
*@使用说明	
*/
int Entity_Reset_Config_Net_Info_To_Flash(void)
{
    ENTITY_LOGD("%s\r\n", __func__);
    memset(&Entity_App_Param.Dev_Config_Net_Info, 0, sizeof(Entity_Dev_Config_Net_Info_t));
    Entity_App_Param.Dev_Config_Net_Info.Magic_Header = MAGIC_HEADER_VALUE;
    return Entity_Save_Config_Net_Info_To_Flash();
}


/**
*@名称 		Entity_Config_Net_Data_Check
*@功能 		配网信息检查
*@参数 		void
*@返回值 	void
*@使用说明	
*/
void Entity_Config_Net_Data_Check(void)
{
    Entity_Config_Net_Info_t *config_net_info = &Entity_App_Param.Dev_Config_Net_Info.Config_Net_Info;
    if(Entity_App_Param.Dev_Config_Net_Info.Magic_Header != MAGIC_HEADER_VALUE)
    {
        Entity_Reset_Config_Net_Info_To_Flash();
    }
    else
    {
       
    }
    ENTITY_LOGD("%s Flag_Bind:%d, Bind_Type:%d, ssid:%s, passwd_len:%u, mqtt:%s, port:%d\r\n", \
       __func__, Entity_App_Param.Dev_Config_Net_Info.Flag_Bind, Entity_App_Param.Dev_Config_Net_Info.Bind_Type, config_net_info->Wifi_Info.Ssid,\
         (unsigned int)strlen(config_net_info->Wifi_Info.Key), config_net_info->Mqtt_Info.Mqtt_Host, config_net_info->Mqtt_Info.Mqtt_Port);
}
 
/**
*@名称 		Entity_Save_Config_Net_Info_To_Flash
*@功能 		保存配网信息到FLASH
*@参数 		uint8_t *pdata, uint16_t len
*@返回值 	int
*@使用说明	
*/
int Entity_Save_Config_Net_Info_To_Flash(void)
{
    int ret = 0;
    ENTITY_LOGD("%s\r\n", __func__);
    if(Entity_Flash_Cbs.Flash_Write_Config_Net_Cb)
        ret = Entity_Flash_Cbs.Flash_Write_Config_Net_Cb((unsigned char *)&Entity_App_Param.Dev_Config_Net_Info, sizeof(Entity_Dev_Config_Net_Info_t));
    return ret;
}


/**
*@名称 		Entity_Read_Config_Net_Info_From_Flash
*@功能 		从FLASH中读配网信息
*@参数 		void
*@返回值 	int
*@使用说明	
*/
int Entity_Read_Config_Net_Info_From_Flash(void)
{
    int ret = 0;
    ENTITY_LOGD("%s\r\n", __func__);
    if(Entity_Flash_Cbs.Flash_Read_Config_Net_Cb)
    {
        ret = Entity_Flash_Cbs.Flash_Read_Config_Net_Cb((unsigned char *)&Entity_App_Param.Dev_Config_Net_Info, sizeof(Entity_Dev_Config_Net_Info_t));
        if(ret <= 0)
        {
            ENTITY_LOGE("Config_Net_Info key not create! now init\r\n");
            Entity_Reset_Config_Net_Info_To_Flash();
            return 0;
        }
        else
        {   
            Entity_Config_Net_Data_Check();
        }
        
    }
    return ret;
}


/**
*@名称 		Entity_Reset_Thing_Model_To_Flash
*@功能 		将FLASH中物模型信息恢复出厂设置
*@参数 		void
*@返回值 	void
*@使用说明	
*/
void Entity_Reset_Thing_Model_To_Flash(void)
{
    ENTITY_LOGD("%s\r\n", __func__);
    if(Entity_App_Param.Dev_Info.Thing_Model_Properties)
    {
        ENTITY_LOGD("cJSON_Delete Thing_Model_Properties\r\n");
        cJSON_Delete(Entity_App_Param.Dev_Info.Thing_Model_Properties);
        Entity_App_Param.Dev_Info.Thing_Model_Properties = NULL;
    }
    memset(&Entity_App_Param.Dev_Thing_Model_Info, 0, sizeof(Entity_Dev_Thing_Model_Info_t));
    Entity_App_Param.Dev_Thing_Model_Info.Magic_Header = MAGIC_HEADER_VALUE;
    Entity_Save_Thing_Model_Info_To_Flash();
}

/**
*@名称 		Entity_Thing_Model_Data_Check
*@功能 	    物模型信息检查
*@参数 		void
*@返回值 	void
*@使用说明	
*/
void Entity_Thing_Model_Data_Check(void)
{
    if(Entity_App_Param.Dev_Thing_Model_Info.Magic_Header != MAGIC_HEADER_VALUE)
    {
        Entity_Reset_Thing_Model_To_Flash();
    }
    else 
    {
       
    }
    ENTITY_LOGD("%s Have_Model:%d, Len:%d\r\n", __func__, Entity_App_Param.Dev_Thing_Model_Info.Have_Model, Entity_App_Param.Dev_Thing_Model_Info.Len);
}
 
/**
*@名称 		Entity_Save_Thing_Model_Info_To_Flash
*@功能 		保存物模型信息到FLASH
*@参数 		uint8_t *pdata, uint16_t len
*@返回值 	int
*@使用说明	
*/
int Entity_Save_Thing_Model_Info_To_Flash(void)
{
    int ret = 0;
    ENTITY_LOGD("%s\r\n", __func__);
    if(Entity_Flash_Cbs.Flash_Write_Thing_Model_Info_Cb)
        ret = Entity_Flash_Cbs.Flash_Write_Thing_Model_Info_Cb((unsigned char *)&Entity_App_Param.Dev_Thing_Model_Info, sizeof(Entity_Dev_Thing_Model_Info_t));
    return ret;
}


/**
*@名称 		Entity_Read_Thing_Model_Info_From_Flash
*@功能 		从FLASH中读物模型信息
*@参数 		void
*@返回值 	int
*@使用说明	
*/
int Entity_Read_Thing_Model_Info_From_Flash(void)
{
    int ret = 0;
    ENTITY_LOGD("%s\r\n", __func__);
    if(Entity_Flash_Cbs.Flash_Read_Thing_Model_Info_Cb)
    {
        ret = Entity_Flash_Cbs.Flash_Read_Thing_Model_Info_Cb((unsigned char *)&Entity_App_Param.Dev_Thing_Model_Info, sizeof(Entity_Dev_Thing_Model_Info_t));
        if(ret <= 0)
        {
            ENTITY_LOGE("thing model info key not create! now init\r\n");
            Entity_Reset_Thing_Model_To_Flash();
            return 0;
        }
        else
        {
            Entity_Thing_Model_Data_Check();
        } 
    }
    return 0;
}

/**
*@名称 		Entity_Load_Dev_Info
*@功能 		初始化设备信息
*@参数 		const char *pid, const char *product_secret, const char *dev_version, const char *sub_version, Entity_Triple_Info_t *test_triple
*@返回值 	int
*@使用说明	
*/
int Entity_Load_Dev_Info(const char *pid, const char *product_secret, const char *dev_version, const char *sub_version, Entity_Triple_Info_t *test_triple)
{
    Entity_Read_Triple_Info_From_Flash();
    Entity_Read_Config_Net_Info_From_Flash();
    Entity_Read_Thing_Model_Info_From_Flash();
    if(Entity_App_Param.Dev_Thing_Model_Info.Have_Model && Entity_App_Param.Dev_Thing_Model_Info.Len > 0)
    {
        Entity_App_Param.Dev_Info.Thing_Model_Properties = cJSON_Parse(Entity_App_Param.Dev_Thing_Model_Info.Content);
    }
    Wifi_Account_Param_t *wifi_account = Get_Wifi_Account_Param();
    wifi_account->Flag_Wifi_Info_Vaild = Entity_App_Param.Dev_Config_Net_Info.Flag_Wifi_Info_Vaild;
    wifi_account->Flag_Config_Net_Success = wifi_account->Flag_Wifi_Info_Vaild;
    strncpy(Entity_App_Param.Dev_Info.Pid, pid, sizeof(Entity_App_Param.Dev_Info.Pid)-1);
    strncpy(Entity_App_Param.Dev_Info.Product_Secret, product_secret, sizeof(Entity_App_Param.Dev_Info.Product_Secret)-1);
    strncpy(Entity_App_Param.Dev_Info.Dev_Version, dev_version, sizeof(Entity_App_Param.Dev_Info.Dev_Version)-1);
    strncpy(Entity_App_Param.Dev_Info.Sub_Version, sub_version, sizeof(Entity_App_Param.Dev_Info.Sub_Version)-1);

    Entity_Triple_Info_t *triple_info = Entity_Get_Triple_Info();
    Entity_App_Param_t *app_param = Get_Entity_App_Param();
    strncpy(triple_info->Pid, pid, sizeof(triple_info->Pid)-1);
    ENTITY_LOGI("Flag_Bind:%d, Bind_Type:%d\r\n", app_param->Dev_Config_Net_Info.Flag_Bind, app_param->Dev_Config_Net_Info.Bind_Type);

    if (test_triple != NULL)//使用测试三元组
    {
        //如果三元组不一致，则更新 
        if(memcmp(triple_info->Uuid, test_triple->Uuid, strlen(test_triple->Uuid)) || \
            memcmp(triple_info->Secret, test_triple->Secret, strlen(test_triple->Secret)) || \
            memcmp(triple_info->Mac, test_triple->Mac, strlen(test_triple->Mac)) )
        {
            ENTITY_LOGE("new triple update\r\n");
            memcpy(triple_info->Uuid, test_triple->Uuid, sizeof(triple_info->Uuid));
            memcpy(triple_info->Secret, test_triple->Secret, sizeof(triple_info->Secret));
            memcpy(triple_info->Mac, test_triple->Mac, sizeof(triple_info->Mac));
            Entity_Save_Triple_Info_To_Flash();
        }
        else
        {
            memcpy(triple_info->Uuid, test_triple->Uuid, sizeof(triple_info->Uuid));
            memcpy(triple_info->Secret, test_triple->Secret, sizeof(triple_info->Secret));
            memcpy(triple_info->Mac, test_triple->Mac, sizeof(triple_info->Mac));
        }
    }
  
    if(!strlen(triple_info->Uuid) || !strlen(triple_info->Secret) || !strlen(triple_info->Mac))
    {
        Entity_Dev_State = DEV_UNAUTHORIZED_STATE;
    }
    else
    {
#if 0 // 临时强制清除绑定状态，用于测试配网
       // app_param->Dev_Config_Net_Info.Flag_Bind = 0;
#endif
        if(app_param->Dev_Config_Net_Info.Flag_Bind)
        {
            if(app_param->Dev_Config_Net_Info.Bind_Type == WIFI_BIND_TYPE)
                Entity_Dev_State = DEV_WIFI_PROVISION_STATE;
            else if(app_param->Dev_Config_Net_Info.Bind_Type == BLE_BIND_TYPE)
                Entity_Dev_State = DEV_BLE_PROVISION_STATE;
            else
                Entity_Dev_State = DEV_UNPROVISION_STATE;
        }
        else
        {
            Entity_Dev_State = DEV_UNPROVISION_STATE;
        }
    }
    
    
    char secret_mask[32];
    Utils_Mask_Secret(triple_info->Secret, secret_mask, sizeof(secret_mask));
    ENTITY_LOGI("三元组 Uuid:%s, SecretMask:%s, Mac:%s, Entity_Dev_State:%d\r\n", triple_info->Uuid, secret_mask, triple_info->Mac, Entity_Dev_State);
    return Entity_Dev_State;
}

















