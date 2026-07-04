//entity_http_download.c
#include "entity_http_download.h"

#include "entity_log.h"
#include "entity_iot_func.h"
#include "entity_error_code.h"
#include "entity_http_client.h"
#include "entity_param_check.h"
#include "entity_dev_info.h"
#include "entity_iot_ca.h"

#include <string.h>
#include <stdio.h>

#include "com_utils.h"
#include "com_mbedtls.h"

#define HTTP_HEAD_CONTENT_LEN (1024)

/**
*@名称 		Entity_Http_Download_Init
*@功能 		HTTP客户端初始化请求头部数据
*@参数 		const char *url, uint32_t offset, uint32_t size
*@返回值 	int
*@使用说明	
*/
void *Entity_Http_Download_Init(const char *url, uint32_t offset, uint32_t size)
{
#if 1
    POINTER_SAFETY_CHECK_RETURN_ERR(url, NULL);
    NUMBERICAL_SAFETY_CHECK_RETURN_ERR(size, NULL);

    Entity_Http_Download_t *Download_Handle = NULL;

    Download_Handle = Entity_Mem_Malloc(sizeof(Entity_Http_Download_t));
    MALLOC_POINTER_SAFETY_CHECK_RETURN_ERR(Download_Handle, "malloc for url download handle failed!", NULL);
    memset(Download_Handle, 0, sizeof(Entity_Http_Download_t));

    Download_Handle->Http_Client.Header = Entity_Mem_Malloc(HTTP_HEAD_CONTENT_LEN);
    if (!Download_Handle->Http_Client.Header) 
    {
        Entity_Mem_Free(Download_Handle);
        ENTITY_LOGE("malloc for http header failed!\r\n");
        return NULL;
    }
    memset(Download_Handle->Http_Client.Header, 0, HTTP_HEAD_CONTENT_LEN);

    Entity_App_Param_t *app_param = Get_Entity_App_Param();

    uint32_t sign_ts = Entity_Get_Time_Stamp();

    char plaintxt[128] = {0};
    snprintf(plaintxt, 128, "uuid=%s,ts=%ld,productId=%s", app_param->Dev_Triple_Info.Triple_Info.Uuid, sign_ts, app_param->Dev_Info.Pid);
    ENTITY_LOGD("plaintxt:%s\r\n", plaintxt);

    char sign[41] = {0};
    Hmac_Sha1(plaintxt, app_param->Dev_Triple_Info.Triple_Info.Secret, (unsigned char*)sign);
    //ENTITY_LOGD("sign=%s\r\n", sign);

    snprintf(Download_Handle->Http_Client.Header, HTTP_HEAD_CONTENT_LEN,
                 "Accept: "
                 "text/html,application/xhtml+xml,application/xml;q=0.9,*/"
                 "*;q=0.8\r\n"
                 "rino-signMethod: hmacSha1\r\n"
                 "rino-timestamp:%ld\r\n"
                 "rino-signature:%s\r\n"
                 "productId:%s\r\n"
                 "uuid:%s\r\n"
                 "Accept-Encoding: gzip, deflate\r\n"
                 "Range: bytes=%ld-%ld\r\n",
                 sign_ts, sign, app_param->Dev_Info.Pid, app_param->Dev_Triple_Info.Triple_Info.Uuid, offset, size);

    ENTITY_LOGI("head_content:%s\r\n", Download_Handle->Http_Client.Header);

    Download_Handle->Url = url;
    return Download_Handle;
#else
    POINTER_SAFETY_CHECK_RETURN_ERR(url, NULL);
    NUMBERICAL_SAFETY_CHECK_RETURN_ERR(size, NULL);

    Entity_Http_Download_t *Download_Handle = NULL;

    Download_Handle = Entity_Mem_Malloc(sizeof(Entity_Http_Download_t));
    MALLOC_POINTER_SAFETY_CHECK_RETURN_ERR(Download_Handle, "malloc for url download handle failed!", NULL);
    memset(Download_Handle, 0, sizeof(Entity_Http_Download_t));

    Download_Handle->Http_Client.Header = Entity_Mem_Malloc(HTTP_HEAD_CONTENT_LEN);
    if (!Download_Handle->Http_Client.Header) 
    {
        Entity_Mem_Free(Download_Handle);
        ENTITY_LOGE("malloc for http header failed!\r\n");
        return NULL;
    }
    memset(Download_Handle->Http_Client.Header, 0, HTTP_HEAD_CONTENT_LEN);

    char host[256] = {0};
    Entity_Http_Client_Parse_Host(url, host, sizeof(host));
    sprintf(Download_Handle->Http_Client.Header, \
            "GET %s HTTP/1.1\r\n"\
            "Accept:text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\r\n"\
            "User-Agent:Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537(KHTML, like Gecko) Chrome/47.0.2526Safari/537.36\r\n"\
            "Host:%s\r\n"\
            "\r\n"\
        ,url, host);

    ENTITY_LOGD("head len:%d, content:%s\r\n", strlen(Download_Handle->Http_Client.Header), Download_Handle->Http_Client.Header);

    Download_Handle->Url = url;
    return Download_Handle;
#endif
}

/**
*@名称 		Entity_Http_Download_Deinit
*@功能 		HTTP客户端下载文件反初始化
*@参数 		void *handle
*@返回值 	int
*@使用说明	
*/
int Entity_Http_Download_Deinit(void *handle)
{
    POINTER_SAFETY_CHECK_RETURN_ERR(handle, QCLOUD_RET_SUCCESS);
    Entity_Http_Download_t *Download_Handle = (Entity_Http_Download_t *)handle;
    Entity_Network_t *Entity_Network = &Download_Handle->Http_Client.Network_Stack;
    if (Entity_Network->Is_Connected(Entity_Network)) 
    {
        Entity_Network->Disconnect(Entity_Network);
    }
    Entity_Mem_Free(Download_Handle->Http_Client.Header);
    Entity_Mem_Free(Download_Handle);

    return QCLOUD_RET_SUCCESS;
}

/**
*@名称 		Entity_Http_Download_Connect
*@功能 		HTTP客户端下载建立连接
*@参数 		void *handle, int https_enabled
*@返回值 	int
*@使用说明	
*/
int32_t Entity_Http_Download_Connect(void *handle, int https_enabled)
{
    POINTER_SAFETY_CHECK_RETURN_ERR(handle, QCLOUD_ERR_INVAL);
    Entity_Http_Download_t *Download_Handle = (Entity_Http_Download_t *)handle;

    const char *ca_crt = NULL;
    int port   = 80;
    port = Entity_Http_Client_Parse_Port(Download_Handle->Url);
    ENTITY_LOGI("http download url port=%d\r\n", port);

    if (strstr(Download_Handle->Url, "https") && https_enabled) 
    {
        port   = 443;
        ca_crt = Entity_Iot_Https_Ca_Get();
    }

    int32_t rc = Entity_Http_Client_Common(&Download_Handle->Http_Client, Download_Handle->Url, port, ca_crt, HTTP_GET, &Download_Handle->Http_Client_Data);

    return rc;
}

/**
*@名称 		Entity_Http_Download_Fetch_Data
*@功能 		HTTP客户端获取接收数据
*@参数 		void *handle, int https_enabled
*@返回值 	int
*@使用说明	第一包为文件相关信息，后续为相关数据
*/
int32_t Entity_Http_Download_Fetch_Data(void *handle, char *buf, uint32_t buf_len, uint32_t timeout_s)
{
    POINTER_SAFETY_CHECK_RETURN_ERR(handle, QCLOUD_ERR_INVAL);
    Entity_Http_Download_t *Download_Handle = (Entity_Http_Download_t *)handle;
    Download_Handle->Http_Client_Data.Response_Buf = buf;
    Download_Handle->Http_Client_Data.Response_Buf_Len = buf_len;
    int diff = Download_Handle->Http_Client_Data.Response_Content_Len - Download_Handle->Http_Client_Data.Retrieve_Len;//已接收的数据长度

    int rc = Entity_Http_Recv_Data(&Download_Handle->Http_Client, timeout_s * 1000, &Download_Handle->Http_Client_Data);
    if (QCLOUD_RET_SUCCESS != rc) 
    {
        return rc;
    }
    return Download_Handle->Http_Client_Data.Response_Content_Len - Download_Handle->Http_Client_Data.Retrieve_Len - diff;//本次接收的数据长度
}





