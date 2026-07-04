//entity_http_client.h
#pragma once


#include <stdbool.h>
#include "entity_network.h"


#define AUTH_WITH_NOTLS

#define HTTP_PREFIX  ("http://")
#define HTTPS_PREFIX ("https://")
#define HTTP_PORT    80
#define HTTPS_PORT   443

typedef enum 
{ 
    HTTP_GET, HTTP_POST, HTTP_PUT, HTTP_DELETE, HTTP_HEAD 
}Entity_Http_Method_e;


typedef struct 
{
    int     Remote_Port;
    int     Response_Code;
    char *  Header;
    char *  Auth_User;
    char *  Auth_Password;
    Entity_Network_t Network_Stack;
}Entity_Http_Client_t;

typedef struct {
    bool  Is_More;               // if more data to check
    bool  Is_Chunked;            // if response in chunked data
    int   Retrieve_Len;          // 需要检索（接收）的数据长度，初始化为response_content_len，随着数据接收而递减
    int   Response_Content_Len;  // 整个响应文件的长度
    int   Post_Buf_Len;          // post data length
    int   Response_Buf_Len;      // length of response data buffer
    char *Post_Content_Type;     // type of post content
    char *Post_Buf;              // post data buffer
    char *Response_Buf;          // response data buffer
}Entity_Http_Client_Data_t;

//解析HOST数据，得到域名
int Entity_Http_Client_Parse_Host(const char *url, char *host, uint32_t host_max_len);

//解析HOST数据，得到端口
int Entity_Http_Client_Parse_Port(const char *url);

//关闭HTTP客户端连接
void Entity_Http_Client_Close(Entity_Http_Client_t *client);

//建立HTTP客户端连接
int Entity_Http_Client_Connect(Entity_Http_Client_t *client, const char *url, int port, const char *ca_crt);

//建立HTTP客户端连接并发起请求，获取头部数据
int Entity_Http_Client_Common(Entity_Http_Client_t *client, const char *url, int port, const char *ca_crt, Entity_Http_Method_e method,
                              Entity_Http_Client_Data_t *client_data);

//HTTP接收数据
int Entity_Http_Recv_Data(Entity_Http_Client_t *client, uint32_t timeout_ms, Entity_Http_Client_Data_t *client_data);

//HTTP发送数据
int Entity_Http_Send_Data(Entity_Http_Client_t *client, Entity_Http_Method_e method, uint32_t timeout_ms, Entity_Http_Client_Data_t *client_data);


