//entity_ble_gatt.h
#pragma once

/*蓝牙广播协议
广播数据：
02 01 06 03 02 01 A1 14 16 01 A1 00 52 54 4C 6E 47 35 70 69 35 38 7A 6C 61 48 35 35

扫描应答数据
03 09 52 59 19 FF 00 00 01 03 00 00 04 00 UUID

FLAG
bit7：设置非配网状态标志(1: 不在配网状态；0: 在配网状态)。 
bit6：绑定标志(1 绑定；0: 未绑定)。绑定成功则为非配网状态bit7置1
bit5:  wifi联网状态（1：在网， 0：不在网）表示设备的联网状态
bit4:  是否是通用固件  （1： 通用固件， 0：非通用固件）
bit1:  是否使用聚合协议  （1：是， 0：否）
bit0：请求连接标志，主要应用于按需连接的设备， 1-请求连接， 0-未请求连接
*/



#define ENTITY_BT_CONN_CURRENT_INDEX  0xFF

typedef enum
{
    ENTITY_BLE_CONNECT_EVENT_DISCONNECTED,
    ENTITY_BLE_CONNECT_EVENT_CONNECTED,
    ENTITY_BLE_CONNECT_EVENT_SECURITY_CHANGED,
}Entity_Ble_Connect_Event_e;

typedef struct
{
    unsigned char Adv_Len;      //广播数据长度，若为 0 则没有 Adv_Data 字段
    unsigned char Scan_Rsp_len; //扫描响应数据长度，若为 0 则没有 Scan_Rsp_Data 字段
    unsigned char Adv_Data[32];    //广播数据
    unsigned char Scan_Rsp_Data[32];//扫描响应数据
}Entity_Ble_Adv_Info_t;

typedef struct 
{
    /* 生命周期约定：
     * - Ble_Init 在 SDK 启动阶段调用，负责初始化芯片 BLE 控制器/GATT 服务。
     * - 配网/绑定期间可反复执行 Adv_Stop -> Set_Adv/ScanRsp -> Adv_Start 刷新广播内容。
     * - Ble_Disconnect 使用 ENTITY_BT_CONN_CURRENT_INDEX 断开当前连接，断开后允许继续广播重配。
     * - Ble_Disable 用于已联网后省内存释放 BLE 控制器+协议栈；释放后本开机周期不保证可重新配网，
     *   如产品需要免重启重配，后端必须保证 Ble_Init 能重新拉起 BLE 栈。
     * - SDK 核心只依赖本回调表，不直接 include 芯片 BLE/ESP/BSP 头。*/
    void (*Ble_Init)(void);
    int (*Ble_Gatt_Notify_Send)(unsigned char conn_index, unsigned char *send_data, unsigned short len);
    int (*Ble_Gatt_Indicate_Send)(unsigned char conn_index, unsigned char *send_data, unsigned short len);
    int (*Ble_Set_Adv_Data)(unsigned char *adv_data, unsigned char adv_len);
    int (*Ble_Set_Scan_Rsp_Data)(unsigned char *scan_rsp_data, unsigned char scan_rsp_len);
    int (*Ble_Adv_Start)(void);
    int (*Ble_Adv_Stop)(void);
    void (*Ble_Get_Local_Addr)(unsigned char *mac_addr);
    int (*Ble_Disconnect)(unsigned  char conn_index);
    int (*Ble_Parser_Data)(unsigned char* data, unsigned short len);
    void (*Ble_Disable)(void);   //釋放 BLE 控制器+協議棧（省內存模式）
}Entity_Ble_Cbs_t;


//蓝牙回调函数初始化
void Entity_Ble_Cbs_Init(Entity_Ble_Cbs_t *cbs);

//获取蓝牙回调函数
Entity_Ble_Cbs_t *Get_Entity_Ble_Cbs(void);

//蓝牙连接，断连事件回调
void Entity_Ble_Connect_Envent_Callback(unsigned char event_type, unsigned char conn_index, const unsigned char *dst_addr);

//蓝牙接收数据回调
void Entity_Ble_Gatt_Data_In_Callback(const void *pdata, unsigned short len);

//蓝牙通知指示功能状态改变回调
void Entity_Ble_Gatt_Ccc_Cfg_Change_Callback(unsigned short value);

//以通知的方式发出数据
int Entity_Ble_Gatt_Notify_Send(unsigned char *send_data, unsigned short len);

//以指示的方式发出数据
int Entity_Ble_Gatt_Indicate_Send(unsigned char *send_data, unsigned short len);

//设置蓝牙广播数据内容
int Entity_Ble_Set_Adv_Data(unsigned char *adv_data, unsigned char adv_len);

//设置蓝牙扫描应答数据内容
int Entity_Ble_Set_Scan_Rsp_Data(unsigned char *scan_rsp_data, unsigned char scan_rsp_len);

//设置蓝牙广播及扫描应答数据内容
int Entity_Ble_Set_Adv_Scan_Rsp_Data(void);

//加载广播数据后启动蓝牙广播
void Entity_Ble_Adv_Start(void);

//停止蓝牙广播
void Entity_Ble_Adv_Stop(void);

//釋放 BLE 控制器+協議棧（省內存模式，釋放後本開機週期不能再配網）
void Entity_Ble_Disable(void);

//蓝牙初始化
void Entity_Ble_Init(void);
int Entity_Ble_Ensure_Init(void);
unsigned char Entity_Ble_Is_Initialized(void);

//蓝牙主动断开当前连接
int Entity_Ble_Disconnect(void);

//蓝牙传输层数据协议解析
int Entity_Ble_Parser_Data(unsigned char* data, unsigned short len);
