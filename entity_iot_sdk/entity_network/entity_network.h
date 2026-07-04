//entity_network.h
#pragma once

#include <inttypes.h>
#include <stddef.h>


typedef struct 
{
    uint32_t (*Tcp_Connect)(const char *host, uint16_t port);
    int (*Tcp_Disconnect)(uint32_t fd);
    int (*Tcp_Write)(uint32_t fd, const char *buf, uint32_t len, uint32_t timeout_ms);
    int (*Tcp_Read)(unsigned int fd, char *buf, uint32_t len, uint32_t timeout_ms);
    int (*Udp_Broadcast_Init)(int boardcast_addr, unsigned short boardcast_port, void **board_socket_addr);
    int (*Udp_Broadcast_Send_Bytes)(int sockfd, char *pdata, unsigned int len, void *board_socket_addr);
    int (*Udp_Server_Init)(unsigned short server_port, void **server_socket_addr);
    int (*Udp_Server_Recv_Bytes)(int sockfd, char *pdata, unsigned int *len, void *client_socket_addr);
    int (*Udp_Server_Send_Bytes)(int sockfd, char *pdata, unsigned int len, void *client_socket_addr);
    int (*Network_Socket_Close)(int sockfd);
}Entity_Network_Func_t;

typedef enum 
{ 
    NETWORK_TCP = 0, NETWORK_UDP = 1, NETWORK_TLS = 2, NETWORK_DTLS = 3 
}Network_Type_e;

typedef struct Entity_Network_t Entity_Network_t;
struct Entity_Network_t
{
    int (*Init)(Entity_Network_t *);
	int (*Connect)(Entity_Network_t*);
    int (*Read)(Entity_Network_t*,  char *, uint32_t, uint32_t);
    int (*Write)(Entity_Network_t *,  char *, uint32_t, uint32_t);
    void (*Disconnect)(Entity_Network_t *);
    int (*Is_Connected)(Entity_Network_t *);
    int Handle;
	const char *Host;
	uint16_t Port;
	uint16_t Ca_Crt_Len;
	const char *Ca_Crt;
    Network_Type_e Type;

};



/* 网络 socket 选项（平台中立）；val 语义随 opt：超时=毫秒，缓冲=字节 */
typedef enum
{
    ENTITY_NET_OPT_RECV_TIMEOUT_MS = 0,
    ENTITY_NET_OPT_SEND_TIMEOUT_MS = 1,
    ENTITY_NET_OPT_RECV_BUF_BYTES  = 2,
} Entity_Net_Opt_e;

/**
 * @name   Entity_Net_Set_Option_f
 * @brief  设 fd 的 socket 选项；成功 0，失败 -1。清 entity_http_client.c 的裸 setsockopt + lwIP 依赖
 * @param  fd  socket 文件描述符
 * @param  opt 平台中立 socket 选项枚举
 * @param  val 选项值（超时单位毫秒，缓冲单位字节）
 * @retval 0 成功，-1 失败
 */
typedef int (*Entity_Net_Set_Option_f)(int fd, Entity_Net_Opt_e opt, int val);
extern Entity_Net_Set_Option_f Entity_Net_Set_Option;

//Network接口函数初始化
void Entity_Network_Func_Init(Entity_Network_Func_t *func);

//获取Network接口函数
Entity_Network_Func_t *Get_Entity_Network_Func(void);

//IOT网络接口初始化
int Entity_Network_Init(Entity_Network_t *pNetwork);

//TCP建立连接
uint32_t Entity_Mqtt_Net_Connect(const char *host, uint16_t port);

//TCP断开连接
void Entity_Mqtt_Net_Disconnect(uint32_t socket);

//TCP发送数据
int Entity_Mqtt_Net_Write(uint32_t socket, char *data, uint32_t datalen, uint32_t timeout_ms);

//TCP接收数据
int Entity_Mqtt_Net_Read(uint32_t socket, char *data, uint32_t datalen, uint32_t timeout_ms);

//UDP广播初始化
int Entity_Udp_Broadcast_Init(int boardcast_addr, unsigned short boardcast_port, void **board_socket_addr);

//UDP广播发送数据
int Entity_Udp_Broadcast_Send_Bytes(int sockfd, char *pdata, unsigned int len, void *board_socket_addr);

//UDP服务端初始化
int Entity_Udp_Server_Init(unsigned short server_port, void **server_socket_addr);

//UDP服务接收数据
int Entity_Udp_Server_Recv_Bytes(int sockfd, char *pdata, unsigned int *len, void *client_socket_addr);

//UDP服务端发送数据
int Entity_Udp_Server_Send_Bytes(int sockfd, char *pdata, unsigned int len, void *client_socket_addr);

//关闭网络套接字
int Entity_Network_Socket_Close(int sockfd);

