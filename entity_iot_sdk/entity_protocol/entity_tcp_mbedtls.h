//entity_tcp_mbedbls.h
#pragma once



#define TCP_HEARTBEAT_USE_V2		//TCP电路采用V2协议，可上报数据，需要与鉴权一样加密

//命令类型 0-鉴权请求  1-鉴权回复  2-心跳包  3-唤醒包
typedef enum
{
	KEEPALIVE_CMD_TYPE_AUTH_REQUEST	=0,	//0-鉴权请求
	KEEPALIVE_CMD_TYPE_AUTH_RESPONE	=1,	//1-鉴权回复
	KEEPALIVE_CMD_TYPE_HEARTBEAT	=2,	//2-心跳包
	KEEPALIVE_CMD_TYPE_WAKEUP		=3,	//3-唤醒包
}Keepalive_Cmd_Type_e;

//保活心跳 auth
#define ENTITY_AUTH_VERSION		1	//协议版本号
#define ENTITY_AUTH_TYPE			KEEPALIVE_CMD_TYPE_AUTH_REQUEST	//0-鉴权请求 
#define ENTITY_AUTH_FLAG			1	//0-不加密(心跳，唤醒)  1-加密(鉴权)

#ifdef TCP_HEARTBEAT_USE_V2
#define ENTITY_HEARTBEAT_VERSION		2	//协议版本号2
#define ENTITY_HEARTBEAT_TYPE			KEEPALIVE_CMD_TYPE_HEARTBEAT	//2-心跳包
#define ENTITY_HEARTBEAT_FLAG			1	//1加密(鉴权、心跳上报)
#else
#define ENTITY_HEARTBEAT_VERSION		1	//协议版本号
#define ENTITY_HEARTBEAT_TYPE			KEEPALIVE_CMD_TYPE_HEARTBEAT	//2-心跳包
#define ENTITY_HEARTBEAT_FLAG			0	//0-不加密(心跳，唤醒)
#endif


#define UUID_LEN 32
#define DATA_LEN 32
#define IV_LEN 17


#define QUICK_INFO_LEN_MAX	    2152


//保活信息
typedef struct
{
    char Server_Ip[32];      	// tcp服务器IP或域名
    unsigned short Server_Port; // tcp服务器端口
    unsigned int Interval;     	// 保活包发送间隔, 单位秒
    char Aes_Key[33]; 			// 固定KEY 72696e6f696f742d6c6f77706f776572
    char Uuid[32];    			// 三元组uuid
    char Secret[128]; 			// 三元组密钥
}__attribute__((packed)) Entity_Tcp_Keepalive_Info_t;//取消编译器默认的字节对齐方式,不对齐



//TCP应答数据解密
typedef struct
{
	unsigned char Current_Type;//当前结构体的数据类型  0-鉴权应答  1-唤醒数据
	unsigned char Version;  // 版本号
    unsigned char Type;     // 类型
    unsigned char Flag;     // 标志位
    char Time[32];          // 服务器返回的时间戳
    char Random[32];        // 服务器返回的随机数
    char Signature[64];     // 服务器返回的签名
    unsigned char Uuid[32]; // 解密后的UUID
    //char Data[1152];         // 解密后的JSON数据
	char Data[QUICK_INFO_LEN_MAX]; // 解密后的JSON数据
	unsigned short Data_Len;	//
	int Error;              // 错误码
    int Interval;           // 保活间隔
    unsigned char Flag_Quick_Data_Vaild;    //快启信息是否有效标志
}Entity_Tcp_Respone_Decrypt_t;


//获取鉴权加密数据
int Entity_Get_Auth_Encryption_Data(char *uuid, char *secret, char *aes_key, unsigned char *enc_data, int *enc_data_len);

//获取心跳加密数据
int Entity_Get_Heartbeat_Encryption_Data(unsigned char flag_bat_low_report, char *uuid, char *secret, \
                                        char *aes_key, unsigned char *enc_data, int *enc_data_len);

//解析鉴权应答数据
int Entity_Auth_Resp_Parse(const unsigned char *auth_data, int auth_data_len, char *aes_key, Entity_Tcp_Respone_Decrypt_t *result);

//解析唤醒数据
int Entity_Keepalive_Wakeup_Resp_Parse(const unsigned char *wakeup_data, int wakeup_data_len, char *aes_key,Entity_Tcp_Respone_Decrypt_t *result);

//解析心跳应答数据
int Entity_Keepalive_Heartbeat_Resp_Parse(const unsigned char *rsp_data, int rsp_data_len, char *aes_key, Entity_Tcp_Respone_Decrypt_t *result);




































