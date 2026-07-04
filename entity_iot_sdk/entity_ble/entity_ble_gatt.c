//entity_ble_gatt.c
#include "entity_ble_gatt.h"

#include <string.h>
#include <inttypes.h>
#include <stdbool.h>

#include "entity_log.h"
#include "entity_iot_func.h"
#include "entity_msg_queue.h"
#include "entity_dev_info.h"
#include "entity_config_net.h"

static uint8_t Ble_Connect_Id=0xff;   //连接ID
static uint8_t Connect_State;   //连接状态
static Entity_Ble_Cbs_t Entity_Ble_Cbs;
static __attribute__((unused)) unsigned char Flag_Adv_Start;//蓝牙广播启动标志
static Entity_Ble_Adv_Info_t Adv_Info={0};
static unsigned char s_ble_initialized;

/**
*@名称 		Entity_Ble_Cbs_Init
*@功能 		蓝牙回调函数初始化
*@参数 		Entity_Ble_Cbs_t *cbs
*@返回值 	void
*@使用说明	
*/
void Entity_Ble_Cbs_Init(Entity_Ble_Cbs_t *cbs)
{
    Entity_Ble_Cbs = *cbs;
}

/**
*@名称 		Get_Entity_Ble_Cbs
*@功能 		获取蓝牙回调函数
*@参数 		void
*@返回值 	Entity_Ble_Cbs_t *
*@使用说明	
*/
Entity_Ble_Cbs_t *Get_Entity_Ble_Cbs(void)
{
    return &Entity_Ble_Cbs;
}

/**
*@名称 		Entity_Ble_Get_Connect_Index
*@功能 		获取当前连接ID
*@参数 		void
*@返回值 	uint8_t
*@使用说明	
*/
uint8_t Entity_Ble_Get_Connect_Index(void)
{
    return Ble_Connect_Id;
}


/**
*@名称 		Entity_Ble_Connect_Envent_Callback
*@功能 		蓝牙连接，断连事件回调
*@参数 		uint8_t event_type,  uint8_t conn_index, const uint8_t *dst_addr
*@返回值 	void
*@使用说明	
*/
void Entity_Ble_Connect_Envent_Callback(uint8_t event_type, uint8_t conn_index, const uint8_t *dst_addr)
{
    //ENTITY_LOGI("%s, event_type:%d, conn_index:%d\r\n", __func__, event_type, conn_index);
    switch(event_type)
    {
        case ENTITY_BLE_CONNECT_EVENT_CONNECTED:   //已连接
            Ble_Connect_Id = conn_index;
            Connect_State = true;
            ENTITY_LOGI("%s, conn_index:%d\r\n", __func__, conn_index); //已连接
            break;
        case ENTITY_BLE_CONNECT_EVENT_DISCONNECTED://已断连
        {
            Ble_Connect_Id = 0xff;
            Connect_State = false;
            ENTITY_LOGI("%s, disconnect index:%d\r\n", __func__, conn_index);
            Wifi_Account_Param_t *wifi_account = Get_Wifi_Account_Param();
            if(!wifi_account->Flag_Have_Get_Config_Net_Info)
                Entity_Ble_Adv_Start();
        }
            break;   
        default:
            break;
    }
}

/**
*@名称 		Entity_Ble_Gatt_Data_In_Callback
*@功能 		蓝牙接收数据回调
*@参数 		const void *pdata, uint16_t len
*@返回值 	void
*@使用说明	
*/
void Entity_Ble_Gatt_Data_In_Callback(const void *pdata, uint16_t len)
{
    ENTITY_LOGI("%s\r\n", __func__);
    Entity_App_Msg_Queue_Send(ENTITY_MSG_TYPE_BLE, pdata, len);
}

/**
*@名称 		Entity_Ble_Gatt_Ccc_Cfg_Change_Callback
*@功能 		蓝牙通知指示功能状态改变回调
*@参数 		uint16_t value
*@返回值 	void
*@使用说明	
*/
void Entity_Ble_Gatt_Ccc_Cfg_Change_Callback(uint16_t value)
{
    ENTITY_LOGI("%s\r\n", __func__);
}


/**
*@名称 		Entity_Ble_Gatt_Notify_Send
*@功能 		以通知的方式发出数据
*@参数 		uint8_t *send_data, uint16_t len
*@返回值 	int
*@使用说明  
*/
int Entity_Ble_Gatt_Notify_Send(uint8_t *send_data, uint16_t len)
{
    ENTITY_LOGI("%s\r\n", __func__);

    if(Connect_State == false)
    {
        ENTITY_LOGE("%s, is not connected\r\n", __func__);
        return -1;
    }
    if(Entity_Ble_Cbs.Ble_Gatt_Notify_Send)
    {
        ENTITY_LOGI("%s, notify ble send\r\n", __func__);
        return Entity_Ble_Cbs.Ble_Gatt_Notify_Send(Ble_Connect_Id, send_data, len);
    }
    else
    {
        ENTITY_LOGE("%s, ble func is not init \r\n", __func__);
        return -1;
    }  
}

/**
*@名称 		Entity_Ble_Gatt_Indicate_Send
*@功能 		以指示的方式发出数据
*@参数 		uint8_t *send_data, uint16_t len
*@返回值 	int
*@使用说明  
*/
int Entity_Ble_Gatt_Indicate_Send(uint8_t *send_data, uint16_t len)
{
    ENTITY_LOGI("%s\r\n", __func__);
    /* TODO_RUNTIME_STUB: indicate path 目前直接回成功，後面的 BSP indicate callback 不可達。 */
    return 0;
    if(Connect_State == false)
    {
        ENTITY_LOGE("%s, is not connected\r\n", __func__);
        return -1;
    }
    if(Entity_Ble_Cbs.Ble_Gatt_Indicate_Send)
    {
        return Entity_Ble_Cbs.Ble_Gatt_Indicate_Send(Ble_Connect_Id, send_data, len);
    }
    else
    {
        ENTITY_LOGE("%s, ble func is not init \r\n", __func__);
        return -1;
    }  
}

/**
*@名称 		Entity_Ble_Adv_Start
*@功能 		启动蓝牙广播
*@参数 		Entity_Ble_Adv_Info_t *adv_info, uint8_t config_flag
*@返回值 	void
*@使用说明	
*/
void Entity_Ble_Load_Adv_Scan_Rsp_Data(Entity_Ble_Adv_Info_t *adv_info, uint8_t config_flag)
{
    Entity_Triple_Info_t *triple_info = Entity_Get_Triple_Info();
    ENTITY_LOGI("Pid:%s\r\n", triple_info->Pid);
    uint8_t pid_len = strlen(triple_info->Pid);
    //加载广播数据,共有3段 
    uint8_t adv_offset=0;
    //1、FLAGS
    adv_info->Adv_Data[adv_offset++] = 0x02;    //当前段长度2
    adv_info->Adv_Data[adv_offset++] = 0x01;    //当前段类型1 FLAGS
    adv_info->Adv_Data[adv_offset++] = 0x06;    //当前段数据 0X06
    //2、Service UUID
    adv_info->Adv_Data[adv_offset++] = 0x03;    //当前段长度3
    adv_info->Adv_Data[adv_offset++] = 0x02;    //当前段类型2  UUID16
    adv_info->Adv_Data[adv_offset++] = 0x01;    //当前段数据 A101
    adv_info->Adv_Data[adv_offset++] = 0xa1;  
    //3、Service Data 16-bit UUID
    adv_info->Adv_Data[adv_offset++] = 4 + pid_len;//当前段长度  
    adv_info->Adv_Data[adv_offset++] = 0x16;    //当前段类型 Service Data
    adv_info->Adv_Data[adv_offset++] = 0x01;    //UUID: A101
    adv_info->Adv_Data[adv_offset++] = 0xa1;    
    adv_info->Adv_Data[adv_offset++] = 0x00;    //0-pid
    memcpy(&adv_info->Adv_Data[adv_offset], triple_info->Pid, pid_len);             
    adv_offset += pid_len;
    adv_info->Adv_Len = adv_offset;

    //加载扫描应答数据
    //加载厂商自定义信息
    uint8_t company[24] = {0};
    uint8_t company_len = 0;
    uint8_t company_id[2] = {0x00, 0x00};   //厂商ID
    uint8_t protocol_version = 0x03;        //协议版本，双模配网
    uint8_t encryption_method = 0x00;       //加密方式: 1-加密，其它不加密
    uint16_t commun_capability = 1 << 2;//通信能力: bit2-有 WiFi2.4G通信能力
    
    company[company_len++] = company_id[0];         //厂商ID
    company[company_len++] = company_id[1];         //厂商ID
    company[company_len++] = config_flag;           //配网状态标志，0x01-待配网
    company[company_len++] = protocol_version;      //协议版本，双模配网
    company[company_len++] = encryption_method;     //加密方式: 1-加密，其它不加密
    company[company_len++] = commun_capability >> 8;//通信能力: bit2-有 WiFi2.4G通信能力
    company[company_len++] = commun_capability;
    company[company_len++] = 0;                     //0- uuid  1-mac
    ENTITY_LOGI("uuid:%s\r\n", triple_info->Uuid);
    uint8_t uuid_len = strlen(triple_info->Uuid);
    uuid_len = uuid_len > 19 ? 19 : uuid_len;
    memcpy(company+company_len, triple_info->Uuid, uuid_len); // 复制UUID到厂商数据
	company_len += uuid_len;

    //完整名称
    //const char *ble_name = "YLX"; // 缩短名称以适应31字节限制 (厂商数据26字节 + 名称头2字节 + 名称3字节 = 31字节)
    const char *ble_name = "RY"; // 缩短名称以适应31字节限制 (厂商数据26字节 + 名称头2字节 + 名称3字节 = 31字节)
    uint8_t name_len = strlen(ble_name);
    uint8_t scan_offset=0;
    adv_info->Scan_Rsp_Data[scan_offset++] = name_len + 1;//当前段长度 (名字长度 + 1个字节的类型)
    adv_info->Scan_Rsp_Data[scan_offset++] = 0x09;//当前段类型9 完整名称
    memcpy(&adv_info->Scan_Rsp_Data[scan_offset], ble_name, name_len);
    scan_offset += name_len;
    //厂商自定义数据
    adv_info->Scan_Rsp_Data[scan_offset++] = company_len + 1;//当前段长度
    adv_info->Scan_Rsp_Data[scan_offset++] = 0xff;//当前段类型 自定义数据
    memcpy(&adv_info->Scan_Rsp_Data[scan_offset], company, company_len); 
    scan_offset += company_len;
    adv_info->Scan_Rsp_len = scan_offset;    

    Entity_Log_Dump_Hex_To_String(ENTITY_LOG_LEVEL_INFO, "adv", adv_info->Adv_Data, adv_info->Adv_Len);
    Entity_Log_Dump_Hex_To_String(ENTITY_LOG_LEVEL_INFO, "rsp", adv_info->Scan_Rsp_Data, adv_info->Scan_Rsp_len);

}


/**
*@名称        Entity_Ble_Set_Adv_Data
*@功能        设置蓝牙广播数据内容
*@参数        uint8_t *adv_data, uint8_t adv_len
*@返回值   int 0-成功
*@使用说明  
*/
int Entity_Ble_Set_Adv_Data(uint8_t *adv_data, uint8_t adv_len)
{
    if(!s_ble_initialized)
    {
        ENTITY_LOGW("%s skip: BLE not initialized\r\n", __func__);
        return 0;
    }
    if(Entity_Ble_Cbs.Ble_Set_Adv_Data)
    {
        return Entity_Ble_Cbs.Ble_Set_Adv_Data(adv_data, adv_len);
    }
    return -1;  
}

/**
*@名称        Entity_Ble_Set_Scan_Rsp_Data
*@功能        设置蓝牙扫描应答数据内容
*@参数        uint8_t *scan_rsp_data, uint8_t scan_rsp_len
*@返回值   int 0-成功
*@使用说明  
*/
int Entity_Ble_Set_Scan_Rsp_Data(uint8_t *scan_rsp_data, uint8_t scan_rsp_len)
{
    if(!s_ble_initialized)
    {
        ENTITY_LOGW("%s skip: BLE not initialized\r\n", __func__);
        return 0;
    }
    if(Entity_Ble_Cbs.Ble_Set_Scan_Rsp_Data)
    {
        return Entity_Ble_Cbs.Ble_Set_Scan_Rsp_Data(scan_rsp_data, scan_rsp_len);
    }
    return -1;     
}

/**
*@名称        Entity_Ble_Set_Adv_Scan_Rsp_Data
*@功能        设置蓝牙广播及扫描应答数据内容
*@参数        uint8_t *scan_rsp_data, uint8_t scan_rsp_len
*@返回值   int 0-成功
*@使用说明  
*/
int Entity_Ble_Set_Adv_Scan_Rsp_Data(void)
{
    int ret = 0;
    Wifi_Account_Param_t *wifi_account = Get_Wifi_Account_Param();
    uint8_t config_flag = wifi_account->Entity_Config_Net_Flag;// 0x01;
    Entity_Ble_Load_Adv_Scan_Rsp_Data(&Adv_Info, config_flag);
    ret |= Entity_Ble_Set_Adv_Data(Adv_Info.Adv_Data, Adv_Info.Adv_Len);
    ret |= Entity_Ble_Set_Scan_Rsp_Data(Adv_Info.Scan_Rsp_Data, Adv_Info.Scan_Rsp_len);
    return ret;
}

/**
*@名称        Entity_Ble_Adv_Start
*@功能        加载广播数据后启动蓝牙广播
*@参数        void
*@返回值   void
*@使用说明  
*/
void Entity_Ble_Adv_Start(void)
{
    if(!s_ble_initialized)
    {
        ENTITY_LOGW("%s skip: BLE not initialized\r\n", __func__);
        return;
    }
    if(Entity_Ble_Cbs.Ble_Adv_Start)
        Entity_Ble_Cbs.Ble_Adv_Start();
}

/**
*@名称 		Entity_Ble_Adv_Stop
*@功能 		停止蓝牙广播
*@参数 		void
*@返回值 	void
*@使用说明	
*/
void Entity_Ble_Adv_Stop(void)
{
    if(!s_ble_initialized)
    {
        ENTITY_LOGD("%s skip: BLE not initialized\r\n", __func__);
        return;
    }
    if(Entity_Ble_Cbs.Ble_Adv_Stop)
        Entity_Ble_Cbs.Ble_Adv_Stop();
}

/**
*@名称 		Entity_Ble_Disable
*@功能 		釋放 BLE 控制器+協議棧（省內存模式）
*@參數 		void
*@返回值 	void
*@使用說明	釋放後本開機週期不能再重新配網
*/
void Entity_Ble_Disable(void)
{
    if(Entity_Ble_Cbs.Ble_Disable)
    {
        Entity_Ble_Cbs.Ble_Disable();
        s_ble_initialized = 0;
        Connect_State = false;
        Ble_Connect_Id = 0xff;
    }
}

/**
*@名称 		Entity_Ble_Init
*@功能 		蓝牙初始化
*@参数 		void
*@返回值 	void
*@使用说明	
*/
void Entity_Ble_Init(void)
{
    if(s_ble_initialized)
    {
        ENTITY_LOGI("%s skip: already initialized\r\n", __func__);
        return;
    }
    if(Entity_Ble_Cbs.Ble_Init)
    {
        Entity_Ble_Cbs.Ble_Init();
        s_ble_initialized = 1;
    }
    else
    {
        ENTITY_LOGE("%s failed: Ble_Init callback missing\r\n", __func__);
    }
}

int Entity_Ble_Ensure_Init(void)
{
    if(s_ble_initialized)
    {
        return 0;
    }

    Entity_Ble_Init();
    return s_ble_initialized ? 0 : -1;
}

unsigned char Entity_Ble_Is_Initialized(void)
{
    return s_ble_initialized;
}

/**
*@名称 		Entity_Ble_Disconnect
*@功能 		蓝牙主动断开当前连接
*@参数 		void
*@返回值 	int
*@使用说明  
*/
int Entity_Ble_Disconnect(void)
{
    if(!s_ble_initialized)
    {
        ENTITY_LOGD("%s skip: BLE not initialized\r\n", __func__);
        return 0;
    }
    if(Entity_Ble_Cbs.Ble_Disconnect)
        return Entity_Ble_Cbs.Ble_Disconnect(ENTITY_BT_CONN_CURRENT_INDEX); 
    return -1;
}

/**
*@名称 		Entity_Ble_Parser_Data
*@功能 		蓝牙传输层数据协议解析
*@参数 		void
*@返回值 	int
*@使用说明  
*/
int Entity_Ble_Parser_Data(unsigned char* data, unsigned short len)
{
    if(Entity_Ble_Cbs.Ble_Parser_Data)
    {
        return Entity_Ble_Cbs.Ble_Parser_Data(data, len);
    }
    return 0;
}
