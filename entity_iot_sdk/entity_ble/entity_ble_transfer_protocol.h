//entity_ble_transfer_protocol.h
#pragma once


#define PER_PACKAGE_LEN         128     //蓝牙传输包最大值
#define PER_DATA_LEN            118     //去掉协议数据后的有效数据最大数量
#define PROTOCOL_DATA_LEN       10      //协议数据长度

#define PROTOCOL_HEAD_V1        0XFF    //V1协议的头部数据
#define PROTOCOL_DATA_TYPE      1


#define ENTITY_BLE_DATA_PARSER_HEAD_ERR 1501  // 1501 ble收到的数据解析，包头异常
#define ENTITY_BLE_DATA_PARSER_SN_ERR 1502    // 1502 ble收到的数据解析，sn异常
#define ENTITY_BLE_DATA_PARSER_CRC_ERR 1503   // 1503 ble收到的数据解析，crc异常
#define ENTITY_BLE_DATA_PARSER_MEM_1_ERR 1504 // 1504 ble收到的数据解析，设备缓存1异常
#define ENTITY_BLE_DATA_PARSER_MEM_2_ERR 1505 // 1505 ble收到的数据解析，设备缓存2异常
#define ENTITY_BLE_DATA_PARSER_LEN_ERR 1506   // 1506 ble收到的数据解析，总数据长度异常
#define ENTITY_BLE_DATA_JSON_PARSER_ERR 1700  // 1700 json解析失败
#define ENTITY_BLE_DATA_JSON_NET 1701         // 1701 json解析成功，开始联网
#define ENTITY_BLE_DATA_SERVER_ERR 1702       // 1702 服务器连接失败
#define ENTITY_BLE_DATA_SERVER_OK 1703        // 1703 服务器连接成功
#define ENTITY_BLE_DATA_BLE_BIND_RESP 1704    // 1704 ble bind response

#define ENTITY_BLE_DATA_BIND_OK 1801     // 1801 bind 成功
#define ENTITY_BLE_DATA_BIND_FAIL 1802   // 1802 bind 失败
#define ENTITY_BLE_DATA_UNBIND_OK 1803   // 1803 unbind 成功
#define ENTITY_BLE_DATA_UNBIND_FAIL 1804 // 1804 unbind 失败
#define ENTITY_BLE_DATA_INIT_OK 1805     // 1805 init 成功
#define ENTITY_BLE_DATA_INIT_FAIL 1806   // 1806 init 失败

#define ENTITY_BLE_DATA_THING_MODEL_OK 2000   // 2000 物模型解析成功
#define ENTITY_BLE_DATA_THING_MODEL_FAIL 2001 // 2001 物模型解析失败


//数据分包信息
typedef struct
{
    unsigned short Total_Sn;    //总包数
    unsigned short Total_Len;   //总长度
    unsigned short Recv_Len;    //已接收长度
    unsigned short Current_Sn;  //当前包序号
    unsigned short Recv_Sn;     //已接收到的包数
    unsigned char Cmd_Type;     //命令类型
    unsigned char *Buf;         //数据缓存区
} Entity_Ble_Gatt_Packet_Info_t;


//蓝牙应用层的消息处理回调
typedef void (*Ble_Msg_App_Process_f)(unsigned char *pdata, unsigned short len);

//根据蓝牙V1协议向APP端发送数据
int Entity_Ble_V1_Send_Packet_By_Notify(unsigned char *send_data, unsigned short len);

//注册蓝牙数据应用层处理回调
void Entity_Ble_V1_Register_Msg_App_Process_Cb(Ble_Msg_App_Process_f callback);

//根据蓝牙V1协议解析收到数据
int Entity_Ble_V1_Parser_Data(unsigned char* data, unsigned short len);


