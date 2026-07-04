//entity_http_download.h
#pragma once

#include "entity_http_client.h"

typedef struct 
{
    const char *Url;
    Entity_Http_Client_t Http_Client;   
    Entity_Http_Client_Data_t Http_Client_Data; 
}Entity_Http_Download_t;

//HTTP客户端初始化请求头部数据
void *Entity_Http_Download_Init(const char *url, uint32_t offset, uint32_t size);

//HTTP客户端下载文件反初始化
int Entity_Http_Download_Deinit(void *handle);

//HTTP客户端下载建立连接
int32_t Entity_Http_Download_Connect(void *handle, int https_enabled);

//HTTP客户端获取接收数据
int32_t Entity_Http_Download_Fetch_Data(void *handle, char *buf, uint32_t buf_len, uint32_t timeout_s);

