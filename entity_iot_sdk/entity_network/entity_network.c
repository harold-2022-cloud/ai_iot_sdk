//entity_network.c
#include "entity_network.h"

#include "entity_log.h"

static Entity_Network_Func_t Entity_Network_Func;


/**
*@名称 		Entity_Network_Func_Init
*@功能 		Network接口函数初始化
*@参数 		Entity_Network_Func_t *func
*@返回值 	void
*@使用说明	
*/
void Entity_Network_Func_Init(Entity_Network_Func_t *func)
{
    Entity_Network_Func = *func;
}


/**
*@名称 		Get_Entity_Network_Cbs
*@功能 		获取Network接口函数
*@参数 		void
*@返回值 	Entity_Network_Func_t *
*@使用说明	
*/
Entity_Network_Func_t *Get_Entity_Network_Cbs(void)
{
    return &Entity_Network_Func;
}

/**
*@名称 		Entity_Tcp_Init
*@功能 		TCP初始化
*@参数 		Entity_Network_t *pNetwork
*@返回值 	int
*@使用说明  
*/
int Entity_Tcp_Init(Entity_Network_t *pNetwork)
{
    return 0;
}

/**
*@名称 		Entity_Tcp_Connect
*@功能 		TCP建立连接
*@参数 		Entity_Network_t *pNetwork
*@返回值 	int
*@使用说明  
*/
int Entity_Tcp_Connect(Entity_Network_t *pNetwork)
{
    if(pNetwork == NULL)
    {
        ENTITY_LOGE("%s network is null\r\n", __func__);
        return -2;
    }
	pNetwork->Handle = Entity_Network_Func.Tcp_Connect(pNetwork->Host, pNetwork->Port);
    ENTITY_LOGI("TCP 连接返回 fd(%d)。\r\n", pNetwork->Handle);
    if (pNetwork->Handle == 0) 
    {
        return -1;
    }
    return 0;
}


/**
*@名称 		Entity_Tcp_Disconnect
*@功能 		TCP断开连接
*@参数 		Entity_Network_t *pNetwork
*@返回值 	void
*@使用说明  
*/
void Entity_Tcp_Disconnect(Entity_Network_t *pNetwork)
{
    if(pNetwork == NULL)
    {
        ENTITY_LOGE("%s network is null\r\n", __func__);
        return ;
    }
    ENTITY_LOGI("TCP 断开连接，fd(%d)。\r\n", pNetwork->Handle);
    if(Entity_Network_Func.Tcp_Disconnect(pNetwork->Handle) == 0)
    {
        pNetwork->Handle = 0;
        return ;
    }
    return ;
}

/**
*@名称 		Entity_Tcp_Write
*@功能 		TCP发送数据
*@参数 		Entity_Network_t *pNetwork,  char *data, uint32_t datalen, uint32_t timeout_ms
*@返回值 	int 实际发送长度
*@使用说明  
*/
int Entity_Tcp_Write(Entity_Network_t *pNetwork,  char *data, uint32_t datalen, uint32_t timeout_ms)
{
    if(pNetwork == NULL)
    {
        ENTITY_LOGE("%s network is null\r\n", __func__);
        return -2;
    }
    return Entity_Network_Func.Tcp_Write(pNetwork->Handle, (char *)data, datalen, timeout_ms);
}


/**
*@名称 		Entity_Tcp_Read
*@功能 		TCP接收数据
*@参数 		Entity_Network_t *pNetwork,  char *data, uint32_t datalen, uint32_t timeout_ms
*@返回值 	int 读取长度
*@使用说明  
*/
int Entity_Tcp_Read(Entity_Network_t *pNetwork, char *data, uint32_t datalen, uint32_t timeout_ms)
{
    if(pNetwork == NULL)
    {
        ENTITY_LOGE("%s network is null\r\n", __func__);
        return -2;
    }
    return Entity_Network_Func.Tcp_Read(pNetwork->Handle, (char *)data, (uint32_t)datalen, timeout_ms);
}

/**
*@名称 		Entity_Network_Is_Connected
*@功能 		TCP是否已连接
*@参数 		Entity_Network_t *pNetwork
*@返回值 	int
*@使用说明  
*/
int Entity_Network_Is_Connected(Entity_Network_t *pNetwork)
{
    if(pNetwork == NULL)
    {
        ENTITY_LOGE("%s network is null\r\n", __func__);
        return 0;
    }
    return pNetwork->Handle > 0;
}


/**
*@名称 		Entity_Network_Init
*@功能 		IOT网络接口初始化
*@参数 		Entity_Network_t *pNetwork
*@返回值 	int
*@使用说明  
*/
int Entity_Network_Init(Entity_Network_t *pNetwork)
{
    if(pNetwork == NULL)
    {
        ENTITY_LOGE("%s network is null\r\n", __func__);
        return -1;
    }
    pNetwork->Init         = Entity_Tcp_Init;
    pNetwork->Connect      = Entity_Tcp_Connect;
    pNetwork->Read         = Entity_Tcp_Read;
    pNetwork->Write        = Entity_Tcp_Write;
    pNetwork->Disconnect   = Entity_Tcp_Disconnect;
    pNetwork->Is_Connected = Entity_Network_Is_Connected;
    pNetwork->Handle       = 0;    
    return pNetwork->Init(pNetwork);
}


/**
*@名称 		Entity_Mqtt_Net_Connect
*@功能 		TCP建立连接
*@参数 		const char *host, uint16_t port
*@返回值 	uint32_t
*@使用说明  
*/
uint32_t Entity_Mqtt_Net_Connect(const char *host, uint16_t port)
{
	return Entity_Network_Func.Tcp_Connect(host, port);
}


/**
*@名称 		Entity_Mqtt_Net_Disconnect
*@功能 		TCP断开连接
*@参数 		uint32_t socket
*@返回值 	void
*@使用说明  
*/
void Entity_Mqtt_Net_Disconnect(uint32_t socket)
{
    if(socket > 0)
    {
        if(Entity_Network_Func.Tcp_Disconnect(socket) != 0)
		    ENTITY_LOGI("%s faild\r\n", __func__);
        else
            ENTITY_LOGI("%s success\r\n", __func__);
    }
}

/**
*@名称 		Entity_Mqtt_Net_Write
*@功能 		TCP发送数据
*@参数 		uint32_t socket, char *data, uint32_t datalen, uint32_t timeout_ms
*@返回值 	int
*@使用说明  
*/
int Entity_Mqtt_Net_Write(uint32_t socket, char *data, uint32_t datalen, uint32_t timeout_ms)
{
    if(socket == 0)
    {
        return -1;
    }
    return Entity_Network_Func.Tcp_Write(socket, data, datalen, timeout_ms);
}


/**
*@名称 		Entity_Mqtt_Net_Read
*@功能 		TCP接收数据
*@参数 		uint32_t socket, char *data, uint32_t datalen, uint32_t timeout_ms
*@返回值 	int
*@使用说明  
*/
int Entity_Mqtt_Net_Read(uint32_t socket, char *data, uint32_t datalen, uint32_t timeout_ms)
{
    if(socket == 0)
    {
        return -1;
    }
    return Entity_Network_Func.Tcp_Read(socket, data, datalen, timeout_ms);
}


/**
*@名称 		Entity_Udp_Broadcast_Init
*@功能 		UDP广播初始化
*@参数 		int boardcast_addr, unsigned short boardcast_port, void **board_socket_addr
*@返回值 	int 0-失败 >0 socket id
*@使用说明	
*/
int Entity_Udp_Broadcast_Init(int boardcast_addr, unsigned short boardcast_port, void **board_socket_addr)
{
    return Entity_Network_Func.Udp_Broadcast_Init(boardcast_addr, boardcast_port, board_socket_addr);
}

/**
*@名称 		Entity_Udp_Broadcast_Send_Bytes
*@功能 		UDP广播发送数据
*@参数 		int sockfd, char *pdata, unsigned int len, void *board_socket_addr
*@返回值 	int
*@使用说明	
*/
int Entity_Udp_Broadcast_Send_Bytes(int sockfd, char *pdata, unsigned int len, void *board_socket_addr)
{
    if(sockfd == 0)
    {
        return -1;
    }
    return Entity_Network_Func.Udp_Broadcast_Send_Bytes(sockfd, pdata, len, board_socket_addr);
}

/**
*@名称 		Entity_Udp_Server_Init
*@功能 		UDP服务端初始化
*@参数 		unsigned short server_port, void **server_socket_addr
*@返回值 	int
*@使用说明	
*/
int Entity_Udp_Server_Init(unsigned short server_port, void **server_socket_addr)
{
    return Entity_Network_Func.Udp_Server_Init(server_port, server_socket_addr);
}

/**
*@名称 		Entity_Udp_Server_Recv_Bytes
*@功能 		UDP服务接收数据
*@参数 		int sockfd, char *pdata, unsigned int len, void *client_socket_addr
*@返回值 	int
*@使用说明	
*/
int Entity_Udp_Server_Recv_Bytes(int sockfd, char *pdata, unsigned int *len, void *client_socket_addr)
{
    if(sockfd == 0)
    {
        return -1;
    }
    return Entity_Network_Func.Udp_Server_Recv_Bytes(sockfd, pdata, len, client_socket_addr);
}

/**
*@名称 		Entity_Udp_Server_Send_Bytes
*@功能 		UDP服务端发送数据
*@参数 		int sockfd, char *pdata, unsigned int len, void *client_socket_addr
*@返回值 	int
*@使用说明	
*/
int Entity_Udp_Server_Send_Bytes(int sockfd, char *pdata, unsigned int len, void *client_socket_addr)
{
    if(sockfd == 0)
    {
        return -1;
    }
    return Entity_Network_Func.Udp_Server_Send_Bytes(sockfd, pdata, len, client_socket_addr);
}

/**
*@名称 		Entity_Network_Socket_Close
*@功能 		关闭网络套接字
*@参数 		int sockfd
*@返回值 	int
*@使用说明	
*/
int Entity_Network_Socket_Close(int sockfd)
{
   return Entity_Network_Func.Network_Socket_Close(sockfd);
}



























































