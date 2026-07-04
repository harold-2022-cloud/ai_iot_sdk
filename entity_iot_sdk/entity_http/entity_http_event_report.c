#include "entity_http_event_report.h"

#include "entity_http_client.h"
#include "entity_dev_info.h"
#include "entity_error_code.h"
#include "entity_iot_ca.h"
#include "entity_iot_func.h"
#include "entity_log.h"

#include "com_mbedtls.h"

#include <stdio.h>
#include <string.h>

#ifndef ENTITY_HTTP_EVENT_REPORT_URL
#define ENTITY_HTTP_EVENT_REPORT_URL ""
#endif

#ifndef ENTITY_HTTP_EVENT_REPORT_SCHEME
#define ENTITY_HTTP_EVENT_REPORT_SCHEME "http"
#endif

#ifndef ENTITY_HTTP_EVENT_REPORT_PATH
#define ENTITY_HTTP_EVENT_REPORT_PATH "/device/report"
#endif

#define ENTITY_HTTP_EVENT_REPORT_URL_MAX_LEN     256
#define ENTITY_HTTP_EVENT_REPORT_HEADER_MAX_LEN  512
#define ENTITY_HTTP_EVENT_REPORT_RESP_MAX_LEN    512
#define ENTITY_HTTP_EVENT_REPORT_TIMEOUT_MS      15000
/**
 * @brief 构建http事件上报url
 * 
 * @param url url缓冲区
 * @param url_len url缓冲区长度
 * @return int 0 成功 -1 失败
 */
static int Entity_Http_Event_Report_Build_Url(char *url, size_t url_len)
{
    const char *fixed_url = ENTITY_HTTP_EVENT_REPORT_URL;

    if ((url == NULL) || (url_len == 0))
    {
        return QCLOUD_ERR_INVAL;
    }

    if ((fixed_url != NULL) && (fixed_url[0] != '\0'))
    {
        if (snprintf(url, url_len, "%s", fixed_url) >= (int)url_len)
        {
            return QCLOUD_ERR_FAILURE;
        }
        return QCLOUD_RET_SUCCESS;
    }

    ENTITY_LOGE("http report url is not configured; explicit endpoint required");
    return QCLOUD_ERR_FAILURE;
}
/**
 * @brief 构建http事件上报header
 * 
 * @param header header缓冲区
 * @param header_len header缓冲区长度
 * @return int 0 成功 -1 失败
 */
static int Entity_Http_Event_Report_Build_Header(char *header, size_t header_len)
{
    Entity_App_Param_t *app_param = Get_Entity_App_Param();
    if (app_param == NULL)
    {
        return QCLOUD_ERR_FAILURE;
    }

    uint32_t sign_ts = Entity_Get_Time_Stamp();
    char plaintext[128] = {0};
    char sign[41] = {0};

    snprintf(plaintext,
             sizeof(plaintext),
             "uuid=%s,ts=%ld,productId=%s",
             app_param->Dev_Triple_Info.Triple_Info.Uuid,
             (long)sign_ts,
             app_param->Dev_Info.Pid);
    Hmac_Sha1(plaintext,
              app_param->Dev_Triple_Info.Triple_Info.Secret,
              (unsigned char *)sign);

    if (snprintf(header,
                 header_len,
                 "Accept: application/json\r\n"
                 "Connection: close\r\n"
                 "rino-signMethod: hmacSha1\r\n"
                 "rino-timestamp:%ld\r\n"
                 "rino-signature:%s\r\n"
                 "productId:%s\r\n"
                 "uuid:%s\r\n",
                 (long)sign_ts,
                 sign,
                 app_param->Dev_Info.Pid,
                 app_param->Dev_Triple_Info.Triple_Info.Uuid) >= (int)header_len)
    {
        return QCLOUD_ERR_FAILURE;
    }

    return QCLOUD_RET_SUCCESS;
}

int Entity_Http_Event_Report_Post_Json(const char *url, const char *json, size_t json_len)
{
    if ((url == NULL) || (url[0] == '\0') || (json == NULL) || (json_len == 0))
    {
        return QCLOUD_ERR_INVAL;
    }

    int rc = QCLOUD_RET_SUCCESS;
    char header[ENTITY_HTTP_EVENT_REPORT_HEADER_MAX_LEN] = {0};
    char response_buf[ENTITY_HTTP_EVENT_REPORT_RESP_MAX_LEN] = {0};
    Entity_Http_Client_t client = {0};
    Entity_Http_Client_Data_t client_data = {0};
    const char *ca_crt = NULL;

    rc = Entity_Http_Event_Report_Build_Header(header, sizeof(header));
    if (rc != QCLOUD_RET_SUCCESS)
    {
        goto exit;
    }

    client.Header = header;
    client_data.Post_Buf = (char *)json;
    client_data.Post_Buf_Len = json_len;
    client_data.Post_Content_Type = "application/json";
    client_data.Response_Buf = response_buf;
    client_data.Response_Buf_Len = sizeof(response_buf) - 1;

    if (strncmp(url, HTTPS_PREFIX, strlen(HTTPS_PREFIX)) == 0)
    {
        ca_crt = Entity_Iot_Https_Ca_Get();
    }

    rc = Entity_Http_Client_Common(&client,
                                 url,
                                 Entity_Http_Client_Parse_Port(url),
                                 ca_crt,
                                 HTTP_POST,
                                 &client_data);
    if (rc != QCLOUD_RET_SUCCESS)
    {
        ENTITY_LOGE("http event report send failed, rc=%d, url=%s", rc, url);
        goto exit;
    }

    rc = Entity_Http_Recv_Data(&client, ENTITY_HTTP_EVENT_REPORT_TIMEOUT_MS, &client_data);
    if (rc != QCLOUD_RET_SUCCESS)
    {
        ENTITY_LOGE("http event report recv failed, rc=%d, url=%s", rc, url);
        goto exit;
    }

    if ((client.Response_Code < 200) || (client.Response_Code >= 300))
    {
        ENTITY_LOGE("http event report response code=%d, body=%s", client.Response_Code, response_buf);
        rc = QCLOUD_ERR_FAILURE;
        goto exit;
    }

    ENTITY_LOGI("http event report success, code=%d", client.Response_Code);

exit:
    if (client.Network_Stack.Handle)
    {
        Entity_Http_Client_Close(&client);
    }
    return rc;
}

/**
 * @brief 上报http事件
 *
 * @param root cJSON对象
 * @return int 0 成功 -1 失败
 */
int Entity_Http_Event_Report_Post(cJSON *root)
{
    if (root == NULL)
    {
        return QCLOUD_ERR_INVAL;
    }

    int rc = QCLOUD_RET_SUCCESS;
    char url[ENTITY_HTTP_EVENT_REPORT_URL_MAX_LEN] = {0};
    char *json = cJSON_PrintUnformatted(root);

    if (json == NULL)
    {
        ENTITY_LOGE("http event report json encode failed");
        return QCLOUD_ERR_FAILURE;
    }

    rc = Entity_Http_Event_Report_Build_Url(url, sizeof(url));
    if (rc == QCLOUD_RET_SUCCESS)
    {
        rc = Entity_Http_Event_Report_Post_Json(url, json, strlen(json));
    }

    cJSON_free(json);
    return rc;
}
