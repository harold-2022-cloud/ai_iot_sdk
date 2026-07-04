//bsp_network.h
#pragma once


#ifdef __cplusplus
extern "C" {
#endif

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>



//错误码
typedef enum
{
    NET_FAILED              = -1,
    NET_CONNECT_FAILED      = -2,
    NET_SOCKET_FAILED       = -3,
    NET_UNKNOWN_HOST        = -4,
    NET_CONN_EOF            = -5,
    NET_TIMEOUT             = -6,
    NET_CERT_VERIFY_FAILED  = -7,
    NET_HANDSHAKE_FAILED    = -8,
	NET_INVALID_PARM		= -9, 

} NETWORK_ERR_E;


//网络信息结构体，具体在后面
typedef struct Network_Context Network_Context_t;

//加密的TCP上下文，具体定义在相应接口文件
typedef struct Tls_Context Tls_Context_t;

//TCP上下文，具体定义在相应接口文件
typedef struct Tcp_Context Tcp_Context_t;

// ⚠ Tcp/Tls_Connect_Params_t 與 library/mi_mqtt/mi_mqtt_types.h 共用同一組 guard 巨集，
//   欄位必須逐一保持一致（先 include 者勝，分歧會被 guard 靜默吞掉）。改此處務必同步另一檔。
//TCP连接参数
#ifndef ENTITY_TCP_CONNECT_PARAMS_T_DEFINED
#define ENTITY_TCP_CONNECT_PARAMS_T_DEFINED
typedef struct {
    const char*     Host;             //域名或IP地址字符串
    uint16_t        Port;             //连接端口 HTTP(80 or 443) */
    uint32_t        Timeout_Ms;       //超时时间
}Tcp_Connect_Params_t;
#endif

//TLS网络层，用于创建TLS安全套接字
#ifndef ENTITY_TLS_CONNECT_PARAMS_T_DEFINED
#define ENTITY_TLS_CONNECT_PARAMS_T_DEFINED
typedef struct {
	const uint8_t*  Cacert;           //SSL服务器证书，若客户端需验证服务器
	size_t          Cacert_Len;       //SSL服务器证书长度
	const uint8_t*  Client_Cert;      //SSL客户端证书，如果服务器要求验证客户端
	size_t          Client_Cert_Len;  //SSL客户端证书长度
	const uint8_t*  Client_Key;       //SSL客户端密钥
	size_t          Client_Key_Len;   //SSL客户端密钥长度
	bool            Cert_Verify;      //使能服务器证书主机名验证
}Tls_Connect_Params_t;
#endif

//网络接口信息
typedef struct{
    char Name[64];  //Name of this network interface.
    char Mac[32];   //Mac address of this network interface.
    char Ip[64];    //IP address of this network interface.
	uint16_t Local_Port;
}Network_Interface_Info_t;

//网络连接信息
struct Network_Context {
	int (*Connect)(Network_Context_t *, const void *, const void *);
	int (*Read)(Network_Context_t *, unsigned char *, size_t);  
	int (*Write)(Network_Context_t *, const unsigned char *, size_t);   
	int (*Disconnect)(Network_Context_t *);   
	int (*Destroy)(Network_Context_t *);        
	Tls_Connect_Params_t Tls_Connect_Params;       
    Tcp_Connect_Params_t Tcp_Connect_Params;
	Tls_Context_t *Tls_Context;
    Tcp_Context_t *Tcp_Context;
	Network_Interface_Info_t Interface_Info;
};

//用于在网络上接收数据的传输接口
typedef int32_t ( * Transport_Recv_f)( Network_Context_t * pNetworkContext, void * pBuffer, size_t bytesToRecv );

//用于通过网络发送数据的传输接口
typedef int32_t ( * Transport_Send_f)( Network_Context_t * pNetworkContext, const void * pBuffer, size_t bytesToSend );


//传输接口
typedef struct TransportInterface
{
    Transport_Recv_f Recv;               /**< Transport receive interface. */
    Transport_Send_f Send;               /**< Transport send interface. */
    Network_Context_t * Network_Context; /**< Implementation-defined network context. */
} Transport_Interface_t;

//TCP网络接口初始化
int Network_Tcp_Init(Network_Context_t *pNetwork, const Tcp_Connect_Params_t *params);

//TCP反初始化
int Network_Tcp_Destroy(Network_Context_t *pNetwork);

//TCP连接
int Network_Tcp_Connect(Network_Context_t *pNetwork, const void *tcp_params, const void *tls_params);

//TCP断开连接
int Network_Tcp_Disconnect(Network_Context_t *pNetwork) ;

//TCP读数据
int Network_Tcp_Read(Network_Context_t *pNetwork, unsigned char *pMsg, size_t len);

//TCP写数据
int Network_Tcp_Write(Network_Context_t *pNetwork, const unsigned char *pMsg, size_t len);

//网络安全连接初始化
int Network_Tls_Init(Network_Context_t *pNetwork, const Tcp_Connect_Params_t *tcp_params, const Tls_Connect_Params_t *tls_params);

//网络安全连接读数据
int Network_Tls_Read(Network_Context_t *pNetwork, unsigned char *pMsg, size_t len);

//网络安全连接写数据
int Network_Tls_Write(Network_Context_t *pNetwork, const unsigned char *pMsg, size_t len);

//TCP网络安全连接
int Network_Tls_Connect(Network_Context_t *pNetwork, const void *tcp_params, const void *tls_params);

//断开TCP网络安全连接
int Network_Tls_Disconnect(Network_Context_t *pNetwork) ;

//销毁TCP网络安全连接
int Network_Tls_Destroy(Network_Context_t *pNetwork);



//------------------------提供给HTTP的接口-------------------------------//
typedef enum {
    TCP_SUCCESS = 0,           // 成功
    TCP_ERROR_CLOSED = -1,     // 连接关闭
    TCP_ERROR_IO = -2,         // IO错误
    TCP_ERROR_TIMEOUT = -3,    // 超时
    TCP_ERROR_INVALID = -4,    // 参数错误
    TCP_ERROR_CONNECT = -5     // 连接失败
} tcp_result_t;

//TCP建立连接
uint32_t Bsp_Tcp_Connect(const char *host, uint16_t port);

//TCP断开连接
int Bsp_Tcp_Disconnect(uint32_t fd);

//TCP发送数据
int Bsp_Tcp_Write(uint32_t fd, const char *buf, uint32_t len, uint32_t timeout_ms);

//TCP接收数据
int Bsp_Tcp_Read(unsigned int fd, char *buf, uint32_t len, uint32_t timeout_ms);

//设置TCP socket选项
int Bsp_Tcp_Set_Options(uint32_t fd, int keepalive, int timeout_ms);

//检查TCP连接状态
int Bsp_Tcp_Get_Status(uint32_t fd);

//UDP广播初始化
int Bsp_Udp_Broadcast_Init(int boardcast_addr, unsigned short boardcast_port, void **board_socket_addr);

//UDP广播发送数据
int Bsp_Udp_Broadcast_Send_Bytes(int sockfd, char *pdata, unsigned int len, void *board_socket_addr);

//UDP服务端初始化
int Bsp_Udp_Server_Init(unsigned short server_port, void **server_socket_addr);

//UDP服务接收数据
int Bsp_Udp_Server_Recv_Bytes(int sockfd, char *pdata, unsigned int *len, void *client_socket_addr);

//UDP服务端发送数据
int Bsp_Udp_Server_Send_Bytes(int sockfd, char *pdata, unsigned int len, void *client_socket_addr);

//关闭网络套接字
int Bsp_Network_Socket_Close(int sockfd);

#ifdef __cplusplus
}
#endif
