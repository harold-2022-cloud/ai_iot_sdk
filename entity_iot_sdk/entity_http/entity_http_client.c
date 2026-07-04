//entity_http_client.c
#include "entity_http_client.h"

#include "entity_error_code.h"
#include "entity_log.h"
#include "entity_iot_func.h"
#include "entity_timer_countdown.h"
#include "entity_param_check.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>


#define HTTP_CLIENT_MIN(x, y) (((x) < (y)) ? (x) : (y))
#define HTTP_CLIENT_MAX(x, y) (((x) > (y)) ? (x) : (y))

#define HTTP_CLIENT_AUTHB_SIZE 128

#define HTTP_CLIENT_CHUNK_SIZE    1025
#define HTTP_CLIENT_SEND_BUF_SIZE 1024

#define HTTP_CLIENT_MAX_HOST_LEN 128
#define HTTP_CLIENT_MAX_URL_LEN  512

#define HTTP_RETRIEVE_MORE_DATA (1)

#define HTTP_SEND_TIMEOUT_MS    5000    //发送超时时间 

/**
*@名称 		Entity_Http_Client_Base64_Enc
*@功能 		base64 enc
*@参数 		char *out, const char *in
*@返回值 	int
*@使用说明	
*/
static void Entity_Http_Client_Base64_Enc(char *out, const char *in)
{
    const char code[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";
    int i = 0, x = 0, l = 0;

    for (; *in; in++) 
    {
        x = x << 8 | *in;
        for (l += 8; l >= 6; l -= 6) 
        {
            out[i++] = code[(x >> (l - 6)) & 0x3f];
        }
    }
    if (l > 0) 
    {
        x <<= 6 - l;
        out[i++] = code[x & 0x3f];
    }
    for (; i % 4;) 
    {
        out[i++] = '=';
    }
    out[i] = '\0';
}

/**
*@名称 		Entity_Http_Client_Get_Info
*@功能 		加载信息到发送缓存区
*@参数 		Entity_Http_Client_t *client,  char *send_buf, int *send_idx, char *buf, uint32_t len
*@返回值 	int
*@使用说明	
*/
static int Entity_Http_Client_Get_Info(Entity_Http_Client_t *client,  char *send_buf, int *send_idx, char *buf, uint32_t len)
{
    int rc = QCLOUD_RET_SUCCESS;
    int cp_len;
    int idx = *send_idx;

    if (len == 0) 
    {
        len = strlen(buf);//获取字符串长度 
    }

    do {
        if ((HTTP_CLIENT_SEND_BUF_SIZE - idx) >= len)//剩余空间够用 
        {
            cp_len = len;   //复制所有数据
        } 
        else 
        {
            cp_len = HTTP_CLIENT_SEND_BUF_SIZE - idx;   //复制剩余空间长度数据
        }

        memcpy(send_buf+idx, buf, cp_len);
        idx += cp_len;
        len -= cp_len;

        if (idx == HTTP_CLIENT_SEND_BUF_SIZE) //达到最大长度，立即发送
        {
            uint32_t byte_written_len = client->Network_Stack.Write(&(client->Network_Stack), (char*)send_buf, HTTP_CLIENT_SEND_BUF_SIZE, HTTP_SEND_TIMEOUT_MS);
            if (byte_written_len > 0) 
            {
                return (byte_written_len);//返回已发送长度
            }
        }
    } while (len);

    *send_idx = idx; //设置待发送长度
    return rc;
}

/**
*@名称 		Entity_Http_Client_Parse_Port
*@功能 		解析HOST数据，得到端口
*@参数 		const char *url
*@返回值 	int
*@使用说明	
*/
int Entity_Http_Client_Parse_Port(const char *url)
{
    int port = 80;
    const char *host_ptr = (const char *)strstr(url, "://");

    if (host_ptr == NULL) 
    {
        ENTITY_LOGE("Could not find host\r\n");
        return port;
    }
    host_ptr += 3;

    char *port_ptr = strchr(host_ptr, ':');
    if (port_ptr) 
    {
        port_ptr += 1;
        char *path_ptr = strchr(port_ptr, '/');
        if (path_ptr != NULL) 
        {
            int _len = path_ptr - port_ptr;
            char str[8] = {0};
            memcpy(str, port_ptr, _len);
            port = atoi(str);
        }
    }
    return port;
}

/**
*@名称 		Entity_Http_Client_Parse_Host
*@功能 		解析HOST数据，得到域名
*@参数 		const char *url, char *host, uint32_t host_max_len
*@返回值 	int
*@使用说明	
*/
int Entity_Http_Client_Parse_Host(const char *url, char *host, uint32_t host_max_len)
{
    const char *host_ptr = (const char *)strstr(url, "://");
    uint32_t host_len = 0;
    char *path_ptr;

    if (host_ptr == NULL) 
    {
        ENTITY_LOGE("Could not find host\r\n");
        return QCLOUD_ERR_HTTP_PARSE;
    }
    host_ptr += 3;

    uint32_t pro_len = 0;
    pro_len = host_ptr - url;

    char *port_ptr = strchr(host_ptr, ':');
    if (port_ptr) 
    {
        host_len = port_ptr - host_ptr;
    }
    else 
    {
        path_ptr = strchr(host_ptr, '/');
        if (path_ptr != NULL)
            host_len = path_ptr - host_ptr;
        else
            host_len = strlen(url) - pro_len;
    }

    if (host_max_len < host_len + 1) 
    {
        ENTITY_LOGE("Host str is too small (%d >= %d)\r\n", host_max_len, host_len + 1);
        return QCLOUD_ERR_HTTP_PARSE;
    }
    memcpy(host, host_ptr, host_len);
    host[host_len] = '\0';

    return QCLOUD_RET_SUCCESS;
}

/**
*@名称 		Entity_Http_Client_Parse_Url
*@功能 		提取URL数据
*@参数 		const char *url, char *scheme, uint32_t max_scheme_len, char *host,uint32_t maxhost_len, int *port, char *path, uint32_t max_path_len
*@返回值 	int
*@使用说明	
*/
static int Entity_Http_Client_Parse_Url(const char *url, char *scheme, uint32_t max_scheme_len, char *host,
                                  uint32_t maxhost_len, int *port, char *path, uint32_t max_path_len)
{
    char *scheme_ptr=(char *)url;
    char *host_ptr=(char *)strstr(url, "://");
    uint32_t host_len=0;
    uint32_t path_len;

    char *path_ptr;
    char *fragment_ptr;

    if (host_ptr == NULL) 
    {
        ENTITY_LOGE("Could not find host\r\n");
        return QCLOUD_ERR_HTTP_PARSE;
    }

    if (host_ptr - scheme_ptr + 1 > max_scheme_len) 
    {
        ENTITY_LOGE("Scheme str is too small (%u >= %u)\r\n", max_scheme_len, (uint32_t)(host_ptr - scheme_ptr + 1));
        return QCLOUD_ERR_HTTP_PARSE;
    }
    memcpy(scheme, scheme_ptr, host_ptr - scheme_ptr);//提取HTTP
    scheme[host_ptr - scheme_ptr] = '\0'; 

    host_ptr += 3;

    *port = 0;

    path_ptr = strchr(host_ptr, '/');
    if (NULL == path_ptr) 
    {
        path_ptr = scheme_ptr + (int)strlen(url);//指到最后
        host_len = path_ptr - host_ptr;// "://"后面的长度
        memcpy(host, host_ptr, host_len);//提取域名
        host[host_len] = '\0';

        memcpy(path, "/", 1);
        path[1] = '\0';
        return QCLOUD_RET_SUCCESS;
    }

    if (host_len == 0) 
    {
        host_len = path_ptr - host_ptr;//域名长度
    }

    if (maxhost_len < host_len + 1) //域名过长无法存储
    {
        ENTITY_LOGE("Host str is too long (host_len(%d) >= max_len(%d))\r\n", host_len + 1, maxhost_len);
        return QCLOUD_ERR_HTTP_PARSE;
    }
    memcpy(host, host_ptr, host_len);//提取域名
    host[host_len] = '\0';

    fragment_ptr = strchr(host_ptr, '#');//#号后面的是文件路径
    if (fragment_ptr != NULL) 
    {
        path_len = fragment_ptr - path_ptr;
    } 
    else 
    {
        path_len = strlen(path_ptr);//\后面的都是文件路径
    }

    if (path_len + 1 > max_path_len) 
    {
        ENTITY_LOGE("Path str is too small (%d >= %d)\r\n", max_path_len, path_len + 1);
        return QCLOUD_ERR_HTTP_PARSE;
    }

    memcpy(path, path_ptr, path_len);//提取文件路径

    path[path_len] = '\0';

    return QCLOUD_RET_SUCCESS;
}

/**
*@名称 		Entity_Http_Client_Send_Auth
*@功能 		发送鉴权数据
*@参数 		Entity_Http_Client_t *client,  char *send_buf, int *send_idx
*@返回值 	int
*@使用说明	
*/
static int Entity_Http_Client_Send_Auth(Entity_Http_Client_t *client,  char *send_buf, int *send_idx)
{
    char b_auth[(int)((HTTP_CLIENT_AUTHB_SIZE + 3) * 4 / 3 + 1)];
    char base64buff[HTTP_CLIENT_AUTHB_SIZE + 3];

    Entity_Http_Client_Get_Info(client, (char *)send_buf, send_idx, "Authorization: Basic ", 0);
    snprintf(base64buff, sizeof(base64buff), "%s:%s", client->Auth_User, client->Auth_Password);

    Entity_Http_Client_Base64_Enc(b_auth, base64buff);
    b_auth[strlen(b_auth) + 1] = '\0';
    b_auth[strlen(b_auth)]     = '\n';

    Entity_Http_Client_Get_Info(client, (char *)send_buf, send_idx, b_auth, 0);

    return QCLOUD_RET_SUCCESS;
}


/**
*@名称 		Entity_Http_Client_Send_Header
*@功能 		发送头部请求数据
*@参数 		Entity_Http_Client_t *client, const char *url, Entity_Http_Method_e method,Entity_Http_Client_Data_t *client_dat
*@返回值 	int
*@使用说明	
*/
static int Entity_Http_Client_Send_Header(Entity_Http_Client_t *client, const char *url, Entity_Http_Method_e method, Entity_Http_Client_Data_t *client_data)
{
    char scheme[8]={0};
    char host[HTTP_CLIENT_MAX_HOST_LEN]={0};
    char path[HTTP_CLIENT_MAX_URL_LEN]={0};
    int len;
    char *send_buf = NULL;
    char *buf = NULL;
    char *meth=(method == HTTP_GET) ? "GET" : (method == HTTP_POST) ? "POST" : (method == HTTP_PUT) ? "PUT"
                                 : (method == HTTP_DELETE) ? "DELETE" : (method == HTTP_HEAD) ? "HEAD" : "";
    int rc = QCLOUD_RET_SUCCESS;
    int port;

    int res = Entity_Http_Client_Parse_Url(url, scheme, sizeof(scheme), host, sizeof(host), &port, path, sizeof(path));
    if (res != QCLOUD_RET_SUCCESS) 
    {
        ENTITY_LOGE("httpclient_parse_url returned %d", res);
        rc = res;
        goto cleanup;
    }

    send_buf = (char *)Entity_Mem_Calloc(1, HTTP_CLIENT_SEND_BUF_SIZE);
    buf = (char *)Entity_Mem_Calloc(1, HTTP_CLIENT_SEND_BUF_SIZE);
    if (send_buf == NULL || buf == NULL)
    {
        ENTITY_LOGE("http send header buffer malloc failed\r\n");
        rc = QCLOUD_ERR_FAILURE;
        goto cleanup;
    }

    if (strcmp(scheme, "http") == 0) 
    {
    } 
    else if (strcmp(scheme, "https") == 0) 
    {
    }

    memset(send_buf, 0, HTTP_CLIENT_SEND_BUF_SIZE);
    len = 0;
    //加载域名
    snprintf(buf, HTTP_CLIENT_SEND_BUF_SIZE, "%s %s HTTP/1.1\r\nHost: %s\r\n", meth, path, host);
    rc = Entity_Http_Client_Get_Info(client, (char*)send_buf, &len, buf, strlen(buf));
    if (rc) 
    {
        ENTITY_LOGE("Could not write request\r\n");
        rc = QCLOUD_ERR_HTTP_CONN;
        goto cleanup;
    }
    //加载鉴权
    if (client->Auth_User) 
    {
        Entity_Http_Client_Send_Auth(client, (char*)send_buf, &len);
    }
    //加载头部
    if (client->Header) 
    {
        Entity_Http_Client_Get_Info(client, (char*)send_buf, &len, (char *)client->Header, strlen(client->Header));
    }
    //加载发送数据的长度类型信息，在此之后发送具体数据
    if (client_data->Post_Buf != NULL) 
    {
        snprintf(buf, HTTP_CLIENT_SEND_BUF_SIZE, "Content-Length: %d\r\n", client_data->Post_Buf_Len);
        Entity_Http_Client_Get_Info(client, (char*)send_buf, &len, buf, strlen(buf));

        if (client_data->Post_Content_Type != NULL) 
        {
            snprintf(buf, HTTP_CLIENT_SEND_BUF_SIZE, "Content-Type: %s\r\n", client_data->Post_Content_Type);
            Entity_Http_Client_Get_Info(client, (char*)send_buf, &len, buf, strlen(buf));
        }
    }
    Entity_Http_Client_Get_Info(client, (char*)send_buf, &len, "\r\n", 0);

    // ENTITY_LOGD("REQUEST:\n%s", send_buf);
    //加载完成后发送
    uint32_t written_len = client->Network_Stack.Write(&client->Network_Stack, (char*)send_buf, len, HTTP_SEND_TIMEOUT_MS);
    if (written_len > 0) 
    {
        // ENTITY_LOGD("Written %lu bytes", written_len);
    } 
    else if (written_len == 0) 
    {
        ENTITY_LOGE("written_len == 0,Connection was closed by server\r\n");
        rc = QCLOUD_ERR_HTTP_CLOSED; /* Connection was closed by server */
        goto cleanup;
    } 
    else 
    {
        ENTITY_LOGE("Connection error (send returned %d)\r\n", rc);
        rc = QCLOUD_ERR_HTTP_CONN;
        goto cleanup;
    }

cleanup:
    if (buf) Entity_Mem_Free(buf);
    if (send_buf) Entity_Mem_Free(send_buf);
    return rc;
}

/**
*@名称 		Entity_Http_Client_Send_Userdata
*@功能 		发送POST数据
*@参数 		Entity_Http_Client_t *client, Entity_Http_Client_Data_t *client_data, uint32_t timeout_ms
*@返回值 	int
*@使用说明	
*/
static int Entity_Http_Client_Send_Userdata(Entity_Http_Client_t *client, Entity_Http_Client_Data_t *client_data, uint32_t timeout_ms)
{
    if (client_data->Post_Buf && client_data->Post_Buf_Len) 
    {
        // ENTITY_LOGD("client_data->post_buf: %s", client_data->post_buf);
        {
            size_t written_len = client->Network_Stack.Write(&client->Network_Stack, client_data->Post_Buf, client_data->Post_Buf_Len, timeout_ms);
            if (written_len > 0) 
            {
                // ENTITY_LOGD("Written %d bytes", written_len);
            } 
            else if (written_len == 0) 
            {
                ENTITY_LOGE("written_len == 0,Connection was closed by server\r\n");
                return QCLOUD_ERR_HTTP_CLOSED;
            } 
            else 
            {
                ENTITY_LOGE("Connection error (send returned %d)\r\n", written_len);
                return QCLOUD_ERR_HTTP_CONN;
            }
        }
    }

    return QCLOUD_RET_SUCCESS;
}

/**
*@名称 		Entity_Http_Client_Send_Request
*@功能 		发送请求
*@参数 		Entity_Http_Client_t *client, const char *url, Entity_Http_Method_e method,Entity_Http_Client_Data_t *client_dat
*@返回值 	int
*@使用说明	
*/
static int Entity_Http_Client_Send_Request(Entity_Http_Client_t *client, const char *url, Entity_Http_Method_e method,
                                     Entity_Http_Client_Data_t *client_data)
{
    int rc;

    rc = Entity_Http_Client_Send_Header(client, url, method, client_data);
    if (rc != 0) 
    {
        ENTITY_LOGE("httpclient_send_header is error, rc = %d\r\n", rc);
        return rc;
    }

    if (method == HTTP_POST || method == HTTP_PUT) 
    {
        rc = Entity_Http_Client_Send_Userdata(client, client_data, HTTP_SEND_TIMEOUT_MS);
    }

    return rc;
}

/**
*@名称 		Entity_Http_Client_Recv
*@功能 		HTTP客户端接收数据
*@参数 		Entity_Http_Client_t *client, char *buf, int min_len, int max_len, int *p_read_len,uint32_t timeout_ms, Entity_Http_Client_Data_t *client_data
*@返回值 	int
*@使用说明	
*/
static int Entity_Http_Client_Recv(Entity_Http_Client_t *client, char *buf, int min_len, int max_len, int *p_read_len,
                             uint32_t timeout_ms, Entity_Http_Client_Data_t *client_data)
{
    Entity_Timer_t  timer;
    int recv_size = 0;

    Entity_System_Timer_Countdown_Ms(&timer, (unsigned int)timeout_ms);

    *p_read_len = 0;

    recv_size = client->Network_Stack.Read(&client->Network_Stack, buf, max_len, (uint32_t)Entity_System_Timer_Remain(&timer));
    if (recv_size > 0) //在指定时间内接收到数据
    {
        *p_read_len = recv_size;
    }
	else if (recv_size == 0) 
    {
		//timeout
        ENTITY_LOGI("read timeout.\r\n");
		return QCLOUD_ERR_TCP_READ_TIMEOUT;
	} 
    else if (-1 == recv_size) 
    {
		ENTITY_LOGI("Connection closed.\r\n");
		return QCLOUD_ERR_TCP_READ_TIMEOUT;
	} 
    else 
    {
		ENTITY_LOGI("Connection error (recv returned %d)\r\n", recv_size);
		return QCLOUD_ERR_TCP_NOTHING_TO_READ;
	}
    return QCLOUD_RET_SUCCESS;
}

/**
*@名称 		Entity_Http_Client_Retrieve_Content
*@功能 		HTTP客户端接收数据应答
*@参数 		Entity_Http_Client_t *client, char *data, int len, uint32_t timeout_ms,Entity_Http_Client_Data_t *client_data
*@返回值 	int
*@使用说明	
*/
static int Entity_Http_Client_Retrieve_Content(Entity_Http_Client_t *client, char *data, int len, uint32_t timeout_ms,
                                         Entity_Http_Client_Data_t *client_data)
{
    int   count   = 0;
    int   templen = 0;
    int   crlf_pos;
    Entity_Timer_t timer;
    Entity_System_Timer_Countdown_Ms(&timer, (unsigned int)timeout_ms);

    client_data->Is_More = 1;

    if (client_data->Response_Content_Len == -1 && client_data->Is_Chunked == 0) 
    {
        while (1) 
        {
            int rc, max_len;
            if (count + len < client_data->Response_Buf_Len - 1) 
            {
                memcpy(client_data->Response_Buf + count, data, len);
                count += len;
                client_data->Response_Buf[count] = '\0';
            } 
            else 
            {
                memcpy(client_data->Response_Buf + count, data, client_data->Response_Buf_Len - 1 - count);
                client_data->Response_Buf[client_data->Response_Buf_Len - 1] = '\0';
                return HTTP_RETRIEVE_MORE_DATA;
            }

            max_len = HTTP_CLIENT_MIN(HTTP_CLIENT_CHUNK_SIZE - 1, client_data->Response_Buf_Len - 1 - count);
            rc = Entity_Http_Client_Recv(client, data, 1, max_len, &len, (uint32_t)Entity_System_Timer_Remain(&timer), client_data);

            /* Receive data */
            // Log_d("data len: %d %d", len, count);

            if (rc != QCLOUD_RET_SUCCESS) 
            {
                return rc;
            }
            if (0 == Entity_System_Timer_Remain(&timer)) 
            {
                ENTITY_LOGE("HTTP read timeout!\r\n");
                return QCLOUD_ERR_HTTP_TIMEOUT;
            }

            if (len == 0) 
            {
                /* read no more data */
                ENTITY_LOGD("no more data, len == 0\r\n");
                client_data->Is_More = 0;
                return QCLOUD_RET_SUCCESS;
            }
        }
    }

    while (1) 
    {
        uint32_t readLen = 0;
        if (client_data->Is_Chunked && client_data->Retrieve_Len <= 0) 
        {
            /* Read chunk header */
            bool foundCrlf;
            int  n;
            do {
                foundCrlf = 0;
                crlf_pos  = 0;
                data[len] = 0;
                if (len >= 2) 
                {
                    for (; crlf_pos < len - 2; crlf_pos++) 
                    {
                        if (data[crlf_pos] == '\r' && data[crlf_pos + 1] == '\n')
                        {
                            foundCrlf = 1;
                            break;
                        }
                    }
                }
                if (!foundCrlf) 
                {
                    /* Try to read more */
                    if (len < HTTP_CLIENT_CHUNK_SIZE) 
                    {
                        int new_trf_len, rc;
                        rc = Entity_Http_Client_Recv(client, data + len, 0, HTTP_CLIENT_CHUNK_SIZE - len - 1, &new_trf_len,
                                               Entity_System_Timer_Remain(&timer), client_data);
                        len += new_trf_len;
                        if (rc != QCLOUD_RET_SUCCESS) 
                        {
                            return (rc);
                        } 
                        else 
                        {
                            continue;
                        }
                    } 
                    else 
                    {
                        return (QCLOUD_ERR_HTTP);
                    }
                }
            } while (!foundCrlf);
            data[crlf_pos] = '\0';

            // n = sscanf(data, "%x", &readLen);/* chunk length */
            readLen = strtoul(data, NULL, 16);
            n = (0 == readLen) ? 0 : 1;
            client_data->Retrieve_Len = readLen;
            client_data->Response_Content_Len += client_data->Retrieve_Len;
            if (readLen == 0) 
            {
                client_data->Is_More = 0;
                ENTITY_LOGD("no more (last chunk)\r\n");
            }

            if (n != 1) 
            {
                ENTITY_LOGE("Could not read chunk length\r\n");
                return QCLOUD_ERR_HTTP_UNRESOLVED_DNS;
            }

            memmove(data, &data[crlf_pos + 2], len - (crlf_pos + 2));
            len -= (crlf_pos + 2);

        } 
        else 
        {
            readLen = client_data->Retrieve_Len;
        }

        do {
            templen = HTTP_CLIENT_MIN(len, readLen);
            if (count + templen < client_data->Response_Buf_Len - 1) 
            {
                memcpy(client_data->Response_Buf + count, data, templen);
                count += templen;
                client_data->Response_Buf[count] = '\0';
                client_data->Retrieve_Len -= templen;
            } 
            else 
            {
                memcpy(client_data->Response_Buf + count, data, client_data->Response_Buf_Len - 1 - count);
                client_data->Response_Buf[client_data->Response_Buf_Len - 1] = '\0';
                client_data->Retrieve_Len -= (client_data->Response_Buf_Len - 1 - count);
                return (HTTP_RETRIEVE_MORE_DATA);
            }

            if (len > readLen) 
            {
                ENTITY_LOGD("memmove %d %d %d\n", readLen, len, client_data->Retrieve_Len);
                memmove(data, &data[readLen], len - readLen); /* chunk case, read between two chunks */
                len -= readLen;
                readLen                   = 0;
                client_data->Retrieve_Len = 0;
            } 
            else 
            {
                readLen -= len;
            }

            if (readLen) 
            {
                int rc;
                int max_len = HTTP_CLIENT_MIN(HTTP_CLIENT_CHUNK_SIZE - 1, client_data->Response_Buf_Len - 1 - count);
                max_len     = HTTP_CLIENT_MIN(max_len, readLen);
                rc          = Entity_Http_Client_Recv(client, data, 1, max_len, &len, Entity_System_Timer_Remain(&timer), client_data);
                if (rc != QCLOUD_RET_SUCCESS) 
                {
                    return (rc);
                }
                if (Entity_System_Timer_Remain(&timer) == 0) 
                {
                    ENTITY_LOGE("HTTP read timeout!\r\n");
                    return (QCLOUD_ERR_HTTP_TIMEOUT);
                }
            }
        } while (readLen);

        if (client_data->Is_Chunked) 
        {
            if (len < 2) 
            {
                int new_trf_len, rc;
                /* Read missing chars to find end of chunk */
                rc = Entity_Http_Client_Recv(client, data + len, 2 - len, HTTP_CLIENT_CHUNK_SIZE - len - 1, &new_trf_len,
                                       Entity_System_Timer_Remain(&timer), client_data);
                if ((rc != QCLOUD_RET_SUCCESS) || (0 == Entity_System_Timer_Remain(&timer))) 
                {
                    return (rc);
                }
                len += new_trf_len;
            }

            if ((data[0] != '\r') || (data[1] != '\n')) 
            {
                ENTITY_LOGE("Format error, %s", data); /* after memmove, the beginning of next chunk */
                return(QCLOUD_ERR_HTTP_UNRESOLVED_DNS);
            }
            memmove(data, &data[2], len - 2); /* remove the \r\n */
            len -= 2;
        } 
        else 
        {
            // Log_d("no more (content-length)");
            client_data->Is_More = 0;
            break;
        }
    }

    return(QCLOUD_RET_SUCCESS);
}

/**
*@名称 		Entity_Http_Client_Recv_Response
*@功能 		HTTP客户端接收数据应答
*@参数 		Entity_Http_Client_t *client, char *data, int len, uint32_t timeout_ms,Entity_Http_Client_Data_t *client_data
*@返回值 	int
*@使用说明	
*/
static int Entity_Http_Client_Response_Parse(Entity_Http_Client_t *client, char *data, int len, uint32_t timeout_ms,
                                       Entity_Http_Client_Data_t *client_data)
{
    int   crlf_pos;
    Entity_Timer_t timer;
    char *tmp_ptr, *ptr_body_end;

    Entity_System_Timer_Countdown_Ms(&timer, timeout_ms);

    client_data->Response_Content_Len = -1;

    char *crlf_ptr = strstr(data, "\r\n");
    if (crlf_ptr == NULL) 
    {
        ENTITY_LOGE("\\r\\n not found");
        return (QCLOUD_ERR_HTTP_UNRESOLVED_DNS);
    }

    crlf_pos       = crlf_ptr - data;
    data[crlf_pos] = '\0';

#if 0
    if (sscanf(data, "HTTP/%*d.%*d %d %*[^\r\n]", &(client->Response_Code )) != 1) {
        ENTITY_LOGE("Not a correct HTTP answer : %s\n", data);
        return QCLOUD_ERR_HTTP_UNRESOLVED_DNS;
    }
#endif

    client->Response_Code  = atoi(data + 9);

    if ((client->Response_Code  < 200) || (client->Response_Code  >= 400)) 
    {
        ENTITY_LOGI("Response code %d\r\n", client->Response_Code );

        if (client->Response_Code  == 403)
            return (QCLOUD_ERR_HTTP_AUTH);

        if (client->Response_Code  == 404)
            return (QCLOUD_ERR_HTTP_NOT_FOUND);
    }

    // Log_d("Reading headers : %s", data);

    // remove null character
    memmove(data, &data[crlf_pos + 2], len - (crlf_pos + 2) + 1);
    len -= (crlf_pos + 2);

    client_data->Is_Chunked = 0;

    if (NULL == (ptr_body_end = strstr(data, "\r\n\r\n"))) 
    {
        int new_trf_len, rc;
        rc = Entity_Http_Client_Recv(client, data + len, 1, HTTP_CLIENT_CHUNK_SIZE - len - 1, &new_trf_len, Entity_System_Timer_Remain(&timer),
                               client_data);
        if (rc != QCLOUD_RET_SUCCESS) 
        {
            return (rc);
        }
        len += new_trf_len;
        data[len] = '\0';
        if (NULL == (ptr_body_end = strstr(data, "\r\n\r\n"))) 
        {
            ENTITY_LOGE("parse error: no end of the request body");
            return (QCLOUD_ERR_FAILURE);
        }
    }

    if (NULL != (tmp_ptr = strstr(data, "Content-Length"))) 
    {
        client_data->Response_Content_Len = atoi(tmp_ptr + strlen("Content-Length: "));
        client_data->Retrieve_Len         = client_data->Response_Content_Len;
    } 
    else if (NULL != (tmp_ptr = strstr(data, "Transfer-Encoding"))) 
    {
        int   len_chunk   = strlen("Chunked");
        char *chunk_value = data + strlen("Transfer-Encoding: ");

        if ((!memcmp(chunk_value, "Chunked", len_chunk)) || (!memcmp(chunk_value, "chunked", len_chunk))) 
        {
            client_data->Is_Chunked           = 1;
            client_data->Response_Content_Len = 0;
            client_data->Retrieve_Len         = 0;
        }
    } 
    else 
    {
        ENTITY_LOGE("Could not parse header");
        return (QCLOUD_ERR_HTTP);
    }

    len = len - (ptr_body_end + 4 - data);
    memmove(data, ptr_body_end + 4, len + 1);
    int rc = Entity_Http_Client_Retrieve_Content(client, data, len, Entity_System_Timer_Remain(&timer), client_data);
    return (rc);
}

/**
*@名称 		Entity_Http_Client_Recv_Response
*@功能 		HTTP客户端接收数据应答
*@参数 		Entity_Http_Client_t *client, uint32_t timeout_ms, Entity_Http_Client_Data_t *client_data
*@返回值 	int
*@使用说明	
*/
static int Entity_Http_Client_Recv_Response(Entity_Http_Client_t *client, uint32_t timeout_ms, Entity_Http_Client_Data_t *client_data)
{
    int   reclen = 0, rc = QCLOUD_ERR_HTTP_CONN;
    char *buf = NULL;
    Entity_Timer_t timer;

    Entity_System_Timer_Countdown_Ms(&timer, timeout_ms);

    buf = (char *)Entity_Mem_Calloc(1, HTTP_CLIENT_CHUNK_SIZE);
    if (buf == NULL)
    {
        ENTITY_LOGE("http recv buffer malloc failed\r\n");
        return QCLOUD_ERR_FAILURE;
    }

    if (0 == client->Network_Stack.Handle) 
    {
        ENTITY_LOGE("Connection has not been established\r\n");
        goto cleanup;
    }
    if (client_data->Is_More) 
    {
        client_data->Response_Buf[0] = '\0';
        rc = Entity_Http_Client_Retrieve_Content(client, buf, reclen, Entity_System_Timer_Remain(&timer), client_data);
    } 
    else 
    {
        client_data->Is_More = 1;
        rc = Entity_Http_Client_Recv(client, buf, 1, HTTP_CLIENT_CHUNK_SIZE - 1, &reclen, Entity_System_Timer_Remain(&timer), client_data);

        if (rc != QCLOUD_RET_SUCCESS) 
        {
            goto cleanup;
        }
        // else if(0 == Entity_System_Timer_Remain(&timer)){
        //  return (QCLOUD_ERR_HTTP_TIMEOUT);
        //}

        buf[reclen] = '\0';

        if (reclen) 
        {
            ENTITY_LOGI("RESPONSE:\n%s", buf);
            rc = Entity_Http_Client_Response_Parse(client, buf, reclen, Entity_System_Timer_Remain(&timer), client_data);
        }
    }

cleanup:
    if (buf) Entity_Mem_Free(buf);
    return rc;
}


/**
*@名称 		Entity_Http_Network_Init
*@功能 		网络接口初始化
*@参数 		Entity_Network_t *pNetwork, const char *host, int port, const char *ca_crt_dir
*@返回值 	int
*@使用说明	
*/
static int Entity_Http_Network_Init(Entity_Network_t *pNetwork, const char *host, int port, const char *ca_crt_dir)
{
    int rc = QCLOUD_RET_SUCCESS;
    if (pNetwork == NULL) 
    {
        return QCLOUD_ERR_INVAL;
    }
    pNetwork->Type = NETWORK_TCP;
#ifndef AUTH_WITH_NOTLS
    if (ca_crt_dir != NULL) 
    {
        pNetwork->Ca_Crt     = ca_crt_dir;
        pNetwork->Ca_Crt_Len = strlen(pNetwork->Ca_Crt);
        pNetwork->Type = NETWORK_TLS;
    }
#endif
    pNetwork->Host = host;
    pNetwork->Port = port;
    rc = Entity_Network_Init(pNetwork);
    return rc;
}


/**
*@名称 		Entity_Http_Client_Close
*@功能 		关闭HTTP客户端连接
*@参数 		Entity_Http_Client_t *client
*@返回值 	void
*@使用说明	
*/
void Entity_Http_Client_Close(Entity_Http_Client_t *client)
{
    client->Network_Stack.Disconnect(&client->Network_Stack);  
}

/**
*@名称 		Entity_Http_Client_Connect
*@功能 		建立HTTP客户端连接
*@参数 		Entity_Http_Client_t *client, const char *url, int port, const char *ca_crt
*@返回值 	int
*@使用说明	
*/
int Entity_Http_Client_Connect(Entity_Http_Client_t *client, const char *url, int port, const char *ca_crt)
{
    int  rc;
    char host[HTTP_CLIENT_MAX_HOST_LEN] = {0};
    rc = Entity_Http_Client_Parse_Host(url, host, sizeof(host));
    if (rc != QCLOUD_RET_SUCCESS)
        return rc;

    ENTITY_LOGD("host=%s, port=%d\r\n", host, port);

    rc = Entity_Http_Network_Init(&client->Network_Stack, host, port, ca_crt);
    if (rc != QCLOUD_RET_SUCCESS)
        return rc;

    rc = client->Network_Stack.Connect(&client->Network_Stack);
    if (rc != QCLOUD_RET_SUCCESS) 
    {
        ENTITY_LOGE("HTTP 客户端连接失败，rc=%d\r\n", rc);
        Entity_Http_Client_Close(client);
    } 
    else 
    {
        ENTITY_LOGD("HTTP 客户端连接成功，fd:%d\r\n", client->Network_Stack.Handle);
    }
    return rc;
}

/**
*@名称 		Entity_Http_Client_Common
*@功能 		建立HTTP客户端连接并发起请求，获取头部数据，启动下载
*@参数 		Entity_Http_Client_t *client, const char *url, int port, const char *ca_crt, Entity_Http_Method_e method,Entity_Http_Client_Data_t *client_data
*@返回值 	int
*@使用说明	
*/
int Entity_Http_Client_Common(Entity_Http_Client_t *client, const char *url, int port, const char *ca_crt, Entity_Http_Method_e method,
                              Entity_Http_Client_Data_t *client_data)
{
    int rc;
    Entity_Debug_Sram_Heap_Info();
    if (client->Network_Stack.Handle == 0) 
    {
        rc = Entity_Http_Client_Connect(client, url, port, ca_crt);
        if (rc != QCLOUD_RET_SUCCESS)
            return rc;
    }
    Entity_Debug_Sram_Heap_Info();
    int timeout = 30000; // 30秒超时（毫秒）
    Entity_Net_Set_Option(client->Network_Stack.Handle, ENTITY_NET_OPT_RECV_TIMEOUT_MS, timeout);
    Entity_Net_Set_Option(client->Network_Stack.Handle, ENTITY_NET_OPT_SEND_TIMEOUT_MS, timeout);

    int recv_buf_size = 16 * 1024;
    Entity_Net_Set_Option(client->Network_Stack.Handle, ENTITY_NET_OPT_RECV_BUF_BYTES, recv_buf_size);


    rc = Entity_Http_Client_Send_Request(client, url, method, client_data);
    if (rc != QCLOUD_RET_SUCCESS) {
        ENTITY_LOGE("HTTP 请求发送失败，rc=%d", rc);
        Entity_Http_Client_Close(client);
        return rc;
    }
    ENTITY_LOGI("HTTP 请求发送成功\r\n");
    Entity_Debug_Sram_Heap_Info();

    return QCLOUD_RET_SUCCESS;
}

/**
*@名称 		Entity_Http_Recv_Data
*@功能 		HTTP接收数据
*@参数 		Entity_Http_Client_t *client, uint32_t timeout_ms, Entity_Http_Client_Data_t *client_data
*@返回值 	int
*@使用说明	
*/
int Entity_Http_Recv_Data(Entity_Http_Client_t *client, uint32_t timeout_ms, Entity_Http_Client_Data_t *client_data)
{
    int   rc = QCLOUD_RET_SUCCESS;
    Entity_Timer_t timer;

    Entity_System_Timer_Countdown_Ms(&timer, (unsigned int)timeout_ms);

    if ((NULL != client_data->Response_Buf) && (0 != client_data->Response_Buf_Len)) 
    {
        rc = Entity_Http_Client_Recv_Response(client, Entity_System_Timer_Remain(&timer), client_data);
        if (rc < 0) 
        {
            ENTITY_LOGE("HTTP 响应接收失败，rc=%d\r\n", rc);
            Entity_Http_Client_Close(client);
            return (rc);
        }
    }
    return(QCLOUD_RET_SUCCESS);
}

/**
*@名称 		Entity_Http_Send_Data
*@功能 		HTTP发送数据
*@参数 		Entity_Http_Client_t *client, Entity_Http_Method_e method, uint32_t timeout_ms, Entity_Http_Client_Data_t *client_data
*@返回值 	int
*@使用说明	
*/
int Entity_Http_Send_Data(Entity_Http_Client_t *client, Entity_Http_Method_e method, uint32_t timeout_ms, Entity_Http_Client_Data_t *client_data)
{
    int rc = QCLOUD_ERR_INVAL;
    if (method == HTTP_POST || method == HTTP_PUT) 
    {
        rc = Entity_Http_Client_Send_Userdata(client, client_data, timeout_ms);
    }
    return rc;
}
