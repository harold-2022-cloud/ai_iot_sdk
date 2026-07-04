//entity_authorization.h
#pragma once


#define ENTITY_AUTH_INTERFACE_UART        0
#define ENTITY_AUTH_INTERFACE_UDP         1

#define ENTITY_AUTH_INTERFACE_USE         ENTITY_AUTH_INTERFACE_UART
//#define ENTITY_AUTH_INTERFACE_USE         ENTITY_AUTH_INTERFACE_UDP

/*
首字节      数据头1     数据头2     协议版本    命令类型  负载长度(2字节)   负载   校验(累加和)
0x00        0x55        0xaa        0x00       0x0d     0X00 0X01       0X10    

首字节：暂未校验
校验:前面所有数据的累加和

*/

#define AUTH_PROTOCOL_HEAD_LEN      0x08     // 固定协议头长度
#define AUTH_HEAD_FIRST_INDEX       1       //第一个数据头，跳过了第一字节
#define AUTH_HEAD_SECOND_INDEX      2       //第二个数据头
#define AUTH_PROTOCOL_VERSION_INDEX 3       //协议版本
#define AUTH_CMD_TYPE_INDEX         4       //命令类型
#define AUTH_LENGTH_HIGH_INDEX      5       //负载长度高字节
#define AUTH_LENGTH_LOW_INDEX       6       //负载长度低字节
#define AUTH_DATA_START_INDEX       7       //负载数据超始

#define AUTH_HEAD_FIRST_VALUE       0x55     //第一个数据头的值
#define AUTH_HEAD_SECOND_VALUE      0xaa     //第二个数据头的值
#define AUTH_PROTOCOL_VERSION_VALUE 0x00     //模块接收帧协议版本号

#define ENTITY_AUTH_CMD               0x0d    // entity iot授权命令

#define ENTITY_AUTH_DATA_LEN_MAX      300


//#define AUTH_REC_DATA_USE_MBEDTLS_AES   //接收数据采用加密
#define AUTH_TRANFER_AES_KEY         "SyncLightSpacexx"
#define AUTH_TRANFER_AES_KEY_BITS    (128)

#define AUTH_PAYLOAD_SIZE_MAX       512        // 负载数据接收最大数量
#define AUTH_RX_BUF_SIZE            (AUTH_PAYLOAD_SIZE_MAX+AUTH_PROTOCOL_HEAD_LEN)




typedef struct 
{
    unsigned char *Rx_Buf;
    unsigned short Rx_Buf_Size;
    unsigned short Rx_Len;
    int (*Write_Bytes)(unsigned char *pdata, unsigned short data_len);
    int (*Read_Bytes)(unsigned char *pdata, unsigned short *data_len);
    int (*Wait_Data)(unsigned int timeout_ms);  // 阻塞等待数据就绪，0=永久等待，-1=超时

}Entity_Auth_Cbs_t;

//注册授权相关操作接口 
void Register_Entity_Auth_Cbs(Entity_Auth_Cbs_t *cbs);

//设备授权初始化
int Entity_Auth_Init(void);

//设备授权协议解析
int Entity_Auth_Protocol_Parse_Handle(unsigned char *pdata, unsigned int len);






