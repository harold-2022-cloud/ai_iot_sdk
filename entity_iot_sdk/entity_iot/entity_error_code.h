//entity_error_code.h
#pragma once


typedef enum{
    ENTITY_URL_DOWNLOAD_RESCODE_NO_ERROR            =-0,//无错误
    ENTITY_URL_DOWNLOAD_RESCODE_DOWNLOAD_TIMEOUT    =-1,//下载超时
    ENTITY_URL_DOWNLOAD_RESCODE_NO_FILE             =-2,//文件不存在
    ENTITY_URL_DOWNLOAD_RESCODE_SIGN_EXPIRED        =-3,//签名过期
    ENTITY_URL_DOWNLOAD_RESCODE_MD5_ERROR           =-4,//MD5不匹配
}Entity_Url_Download_Rescode_e;

typedef enum{
    OTA_RESCODE_NO_ERROR            =-0,//无错误
    OTA_RESCODE_DOWNLOAD_TIMEOUT    =-1,//下载超时
    OTA_RESCODE_NO_FILE             =-2,//文件不存在
    OTA_RESCODE_SIGN_EXPIRED        =-3,//签名过期
    OTA_RESCODE_MD5_ERROR           =-4,//MD5不匹配
    OTA_RESCODE_UPDATE_FIRMWARE     =-5,//更新固件失败
}OTA_RESCODE_E;





typedef int OPERATE_RET;


/****************************************************************************
            the error code marco define for module GLOBAL 
****************************************************************************/
#define OPRT_OK                                            (-0x0000)  //执行成功
#define OPRT_COM_ERROR                                     (-0x0001)  //通用错误
#define OPRT_INVALID_PARM                                  (-0x0002)  //无效的入参
#define OPRT_MALLOC_FAILED                                 (-0x0003)  //内存分配失败
#define OPRT_NOT_SUPPORTED                                 (-0x0004)  //不支持
#define OPRT_NETWORK_ERROR                                 (-0x0005)  //网络错误
#define OPRT_NOT_FOUND                                     (-0x0006)  //没有找到对象
#define OPRT_CR_CJSON_ERR                                  (-0x0007)  //创建json对象失败
#define OPRT_CJSON_PARSE_ERR                               (-0x0008)  //json解析失败
#define OPRT_CJSON_GET_ERR                                 (-0x0009)  //获取json对象失败
#define OPRT_CR_MUTEX_ERR                                  (-0x000a)  //创建信号量失败
#define OPRT_SOCK_ERR                                      (-0x000b)  //创建socket失败
#define OPRT_SET_SOCK_ERR                                  (-0x000c)  //socket设置失败
#define OPRT_SOCK_CONN_ERR                                 (-0x000d)  //socket连接失败
#define OPRT_SEND_ERR                                      (-0x000e)  //发送失败
#define OPRT_RECV_ERR                                      (-0x000f)  //接收失败
#define OPRT_RECV_DA_NOT_ENOUGH                            (-0x0010)  //接收数据不完整
#define OPRT_KVS_WR_FAIL                                   (-0x0011)  //KV写失败
#define OPRT_KVS_RD_FAIL                                   (-0x0012)  //KV读失败
#define OPRT_CRC32_FAILED                                  (-0x0013)  //CRC校验失败
#define OPRT_TIMEOUT                                       (-0x0014)  //超时
#define OPRT_INIT_MORE_THAN_ONCE                           (-0x0015)  //初始化超过一次
#define OPRT_INDEX_OUT_OF_BOUND                            (-0x0016)  //索引越界
#define OPRT_RESOURCE_NOT_READY                            (-0x0017)  //资源未完善
#define OPRT_EXCEED_UPPER_LIMIT                            (-0x0018)  //超过上限
#define OPRT_FILE_NOT_FIND                                 (-0x0019)  //文件未找到
#define OPRT_UNSUPPORTED_JSON_TYPE                         (-0x0020)  //JSON对象不支持
#define OPRT_INVALID_TYPE                                  (-0x0021)  //错误的类型
#define OPRT_DB_ERR                                        (-0x0022)  //数据库通用错误

#define OPRT_GLOBAL_ERRCODE_MAX_CNT 26


/****************************************************************************
            the error code marco define for module BASE_OS_ADAPTER 
****************************************************************************/
#define OPRT_BASE_OS_ADAPTER_REG_NULL_ERROR                (-0x0100)  //系统适配注册失败
#define OPRT_BASE_OS_ADAPTER_INIT_MUTEX_ATTR_FAILED        (-0x0101)  //初始化同步属性失败
#define OPRT_BASE_OS_ADAPTER_SET_MUTEX_ATTR_FAILED         (-0x0102)  //设置同步属性失败
#define OPRT_BASE_OS_ADAPTER_DESTROY_MUTEX_ATTR_FAILED     (-0x0103)  //销毁同步属性失败
#define OPRT_BASE_OS_ADAPTER_INIT_MUTEX_FAILED             (-0x0104)  //初始化互斥量失败
#define OPRT_BASE_OS_ADAPTER_MUTEX_LOCK_FAILED             (-0x0105)  //互斥量加锁失败
#define OPRT_BASE_OS_ADAPTER_MUTEX_TRYLOCK_FAILED          (-0x0106)  //互斥量尝试加锁失败
#define OPRT_BASE_OS_ADAPTER_MUTEX_LOCK_BUSY               (-0x0107)  //互斥量忙
#define OPRT_BASE_OS_ADAPTER_MUTEX_UNLOCK_FAILED           (-0x0108)  //互斥量解锁失败
#define OPRT_BASE_OS_ADAPTER_MUTEX_RELEASE_FAILED          (-0x0109)  //互斥量释放失败
#define OPRT_BASE_OS_ADAPTER_CR_MUTEX_ERR                  (-0x010a)  //互斥量创建失败
#define OPRT_BASE_OS_ADAPTER_MEM_PARTITION_EMPTY           (-0x010b)  //内存分区空
#define OPRT_BASE_OS_ADAPTER_MEM_PARTITION_FULL            (-0x010c)  //内存分区满
#define OPRT_BASE_OS_ADAPTER_MEM_PARTITION_NOT_FOUND       (-0x010d)  //内存分区不存在
#define OPRT_BASE_OS_ADAPTER_INIT_SEM_FAILED               (-0x010e)  //初始化信号量失败
#define OPRT_BASE_OS_ADAPTER_WAIT_SEM_FAILED               (-0x010f)  //等待信号量失败
#define OPRT_BASE_OS_ADAPTER_POST_SEM_FAILED               (-0x0110)  //释放信号量失败
#define OPRT_BASE_OS_ADAPTER_THRD_STA_UNVALID              (-0x0111)  //线程状态非法
#define OPRT_BASE_OS_ADAPTER_THRD_CR_FAILED                (-0x0112)  //线程创建失败
#define OPRT_BASE_OS_ADAPTER_THRD_JOIN_FAILED              (-0x0113)  //线程JOIN函数调用失败
#define OPRT_BASE_OS_ADAPTER_THRD_SELF_CAN_NOT_JOIN        (-0x0114)  //自身线程不能调用JOIN函数
#define OPRT_BASE_OS_ADAPTER_ERRCODE_MAX_CNT 21


/****************************************************************************
            the error code marco define for module BASE_UTILITIES 
****************************************************************************/
#define OPRT_BASE_UTILITIES_PARTITION_EMPTY                (-0x0200)  //无空闲链表
#define OPRT_BASE_UTILITIES_PARTITION_FULL                 (-0x0201)  //链表已满
#define OPRT_BASE_UTILITIES_PARTITION_NOT_FOUND            (-0x0202)  //链表未遍历到
#define OPRT_BASE_UTILITIES_ERRCODE_MAX_CNT 3


/****************************************************************************
            the error code marco define for module BASE_SECURITY 
****************************************************************************/
#define OPRT_BASE_SECURITY_CRC32_FAILED                    (-0x0300)  //CRC32错误
#define OPRT_BASE_SECURITY_ERRCODE_MAX_CNT 1


/****************************************************************************
            the error code marco define for module MID_TLS 
****************************************************************************/
#define OPRT_MID_TLS_NET_SOCKET_ERROR                      (-0x0a00)  //Failed to open a socket
#define OPRT_MID_TLS_NET_CONNECT_ERROR                     (-0x0a01)  //The connection to the given server / port failed.
#define OPRT_MID_TLS_UNKNOWN_HOST_ERROR                    (-0x0a02)  //Failed to get an IP address for the given hostname.
#define OPRT_MID_TLS_CONNECTION_ERROR                      (-0x0a03)  //TLS连接失败
#define OPRT_MID_TLS_DRBG_ENTROPY_ERROR                    (-0x0a04)  //mbedtls随机种子生成失败
#define OPRT_MID_TLS_X509_ROOT_CRT_PARSE_ERROR             (-0x0a05)  //X509根证书解析失败
#define OPRT_MID_TLS_X509_DEVICE_CRT_PARSE_ERROR           (-0x0a06)  //X509设备证书解析失败
#define OPRT_MID_TLS_CTR_DRBG_ENTROPY_SOURCE_ERROR         (-0x0a07)  //The entropy source failed
#define OPRT_MID_TLS_PK_PRIVATE_KEY_PARSE_ERROR            (-0x0a08)  //秘钥解析失败
#define OPRT_MID_TLS_ERRCODE_MAX_CNT 9


/****************************************************************************
            the error code marco define for module LINK_CORE 
****************************************************************************/
#define OPRT_LINK_CORE_NET_SOCKET_ERROR                    (-0x1e00)  //Failed to open a socket
#define OPRT_LINK_CORE_NET_CONNECT_ERROR                   (-0x1e01)  //The connection to the given server / port failed.
#define OPRT_LINK_CORE_UNKNOWN_HOST_ERROR                  (-0x1e02)  //Failed to get an IP address for the given hostname.
#define OPRT_LINK_CORE_TLS_CONNECTION_ERROR                (-0x1e03)  //TLS连接失败
#define OPRT_LINK_CORE_DRBG_ENTROPY_ERROR                  (-0x1e04)  //mbedtls随机种子生成失败
#define OPRT_LINK_CORE_X509_ROOT_CRT_PARSE_ERROR           (-0x1e05)  //X509根证书解析失败
#define OPRT_LINK_CORE_X509_DEVICE_CRT_PARSE_ERROR         (-0x1e06)  //X509设备证书解析失败
#define OPRT_LINK_CORE_PK_PRIVATE_KEY_PARSE_ERROR          (-0x1e07)  //秘钥解析失败
#define OPRT_LINK_CORE_HTTP_CLIENT_HEADER_ERROR            (-0x1e08)
#define OPRT_LINK_CORE_HTTP_CLIENT_SEND_ERROR              (-0x1e09)
#define OPRT_LINK_CORE_HTTP_RESPONSE_BUFFER_EMPTY          (-0x1e0a)
#define OPRT_LINK_CORE_HTTP_GW_NOT_EXIST                   (-0x1e0b)
#define OPRT_LINK_CORE_MQTT_NOT_AUTHORIZED                 (-0x1e0c)
#define OPRT_LINK_CORE_MQTT_GET_TOKEN_FAIL                 (-0x1e0d)
#define OPRT_LINK_CORE_MQTT_CONNECT_ERROR                  (-0x1e0e)
#define OPRT_LINK_CORE_MQTT_PUBLISH_ERROR                  (-0x1e0f)
#define OPRT_LINK_CORE_ERRCODE_MAX_CNT 16




typedef enum {
    QCLOUD_RET_MQTT_ALREADY_CONNECTED           = 4,  // Already connected with MQTT server
    QCLOUD_RET_MQTT_CONNACK_CONNECTION_ACCEPTED = 3,  // MQTT connection accepted by server
    QCLOUD_RET_MQTT_MANUALLY_DISCONNECTED       = 2,  // Manually disconnected with MQTT server
    QCLOUD_RET_MQTT_RECONNECTED                 = 1,  // Reconnected with MQTT server successfully

    QCLOUD_RET_SUCCESS = 0,  // Successful return

    QCLOUD_ERR_FAILURE  = -1001,  // Generic failure return
    QCLOUD_ERR_INVAL    = -1002,  // Invalid parameter
    QCLOUD_ERR_DEV_INFO = -1003,  // Fail to get device info
    QCLOUD_ERR_MALLOC   = -1004,  // Fail to malloc memory

    QCLOUD_ERR_HTTP_CLOSED         = -3,   // HTTP server close the connection
    QCLOUD_ERR_HTTP                = -4,   // HTTP unknown error
    QCLOUD_ERR_HTTP_PRTCL          = -5,   // HTTP protocol error
    QCLOUD_ERR_HTTP_UNRESOLVED_DNS = -6,   // HTTP DNS resolve failed
    QCLOUD_ERR_HTTP_PARSE          = -7,   // HTTP URL parse failed
    QCLOUD_ERR_HTTP_CONN           = -8,   // HTTP connect failed
    QCLOUD_ERR_HTTP_AUTH           = -9,   // HTTP auth failed
    QCLOUD_ERR_HTTP_NOT_FOUND      = -10,  // HTTP 404
    QCLOUD_ERR_HTTP_TIMEOUT        = -11,  // HTTP timeout

    QCLOUD_ERR_MQTT_PUSH_TO_LIST_FAILED                   = -102,  // Fail to push node to MQTT waiting list
    QCLOUD_ERR_MQTT_NO_CONN                               = -103,  // Not connected with MQTT server
    QCLOUD_ERR_MQTT_UNKNOWN                               = -104,  // MQTT unknown error
    QCLOUD_ERR_MQTT_ATTEMPTING_RECONNECT                  = -105,  // Reconnecting with MQTT server
    QCLOUD_ERR_MQTT_RECONNECT_TIMEOUT                     = -106,  // MQTT reconnect timeout
    QCLOUD_ERR_MQTT_MAX_SUBSCRIPTIONS                     = -107,  // MQTT topic subscription out of range
    QCLOUD_ERR_MQTT_SUB                                   = -108,  // MQTT topic subscription fail
    QCLOUD_ERR_MQTT_NOTHING_TO_READ                       = -109,  // MQTT nothing to read
    QCLOUD_ERR_MQTT_PACKET_READ                           = -110,  // Something wrong when reading MQTT packet
    QCLOUD_ERR_MQTT_REQUEST_TIMEOUT                       = -111,  // MQTT request timeout
    QCLOUD_ERR_MQTT_CONNACK_UNKNOWN                       = -112,  // MQTT connection refused: unknown error
    QCLOUD_ERR_MQTT_CONNACK_UNACCEPTABLE_PROTOCOL_VERSION = -113,  // MQTT connection refused: protocol version invalid
    QCLOUD_ERR_MQTT_CONNACK_IDENTIFIER_REJECTED           = -114,  // MQTT connection refused: identifier rejected
    QCLOUD_ERR_MQTT_CONNACK_SERVER_UNAVAILABLE            = -115,  // MQTT connection refused: service not available
    QCLOUD_ERR_MQTT_CONNACK_BAD_USERDATA                  = -116,  // MQTT connection refused: bad user name or password
    QCLOUD_ERR_MQTT_CONNACK_NOT_AUTHORIZED                = -117,  // MQTT connection refused: not authorized
    QCLOUD_ERR_RX_MESSAGE_INVAL                           = -118,  // MQTT received invalid msg
    QCLOUD_ERR_BUF_TOO_SHORT                              = -119,  // MQTT recv buffer not enough
    QCLOUD_ERR_MQTT_QOS_NOT_SUPPORT                       = -120,  // MQTT QoS level not supported
    QCLOUD_ERR_MQTT_UNSUB_FAIL                            = -121,  // MQTT unsubscribe failed

    QCLOUD_ERR_JSON_PARSE            = -132,  // JSON parsing error
    QCLOUD_ERR_JSON_BUFFER_TRUNCATED = -133,  // JSON buffer truncated
    QCLOUD_ERR_JSON_BUFFER_TOO_SMALL = -134,  // JSON parsing buffer not enough
    QCLOUD_ERR_JSON                  = -135,  // JSON generation error
    QCLOUD_ERR_MAX_JSON_TOKEN        = -136,  // JSON token out of range

    QCLOUD_ERR_MAX_APPENDING_REQUEST = -137,  // appending request out of range
    QCLOUD_ERR_MAX_TOPIC_LENGTH      = -138,  // Topic length oversize

    QCLOUD_ERR_COAP_NULL              = -150,  // COAP null pointer
    QCLOUD_ERR_COAP_DATA_SIZE         = -151,  // COAP data size out of range
    QCLOUD_ERR_COAP_INTERNAL          = -152,  // COAP interval error
    QCLOUD_ERR_COAP_BADMSG            = -153,  // COAP bad msg
    QCLOUD_ERR_DTLS_PEER_CLOSE_NOTIFY = -160,  // DTLS connection is closed

    QCLOUD_ERR_PROPERTY_EXIST     = -201,  // property already exist
    QCLOUD_ERR_NOT_PROPERTY_EXIST = -202,  // property not exist
    QCLOUD_ERR_REPORT_TIMEOUT     = -203,  // update timeout
    QCLOUD_ERR_REPORT_REJECTED    = -204,  // update rejected by server
    QCLOUD_ERR_GET_TIMEOUT        = -205,  // get timeout
    QCLOUD_ERR_GET_REJECTED       = -206,  // get rejected by server

    QCLOUD_ERR_ACTION_EXIST     = -210,  // acion already exist
    QCLOUD_ERR_NOT_ACTION_EXIST = -211,  // acion not exist

    QCLOUD_ERR_GATEWAY_CREATE_SESSION_FAIL = -221,  // Gateway fail to create sub-device session
    QCLOUD_ERR_GATEWAY_SESSION_NO_EXIST    = -222,  // Gateway sub-device session not exist
    QCLOUD_ERR_GATEWAY_SESSION_TIMEOUT     = -223,  // Gateway sub-device session timeout
    QCLOUD_ERR_GATEWAY_SUBDEV_ONLINE       = -224,  // Gateway sub-device online
    QCLOUD_ERR_GATEWAY_SUBDEV_OFFLINE      = -225,  // Gateway sub-device offline

    QCLOUD_ERR_TCP_SOCKET_FAILED   = -601,  // TLS TCP socket connect fail
    QCLOUD_ERR_TCP_UNKNOWN_HOST    = -602,  // TCP unknown host (DNS fail)
    QCLOUD_ERR_TCP_CONNECT         = -603,  // TCP/UDP socket connect fail
    QCLOUD_ERR_TCP_READ_TIMEOUT    = -604,  // TCP read timeout
    QCLOUD_ERR_TCP_WRITE_TIMEOUT   = -605,  // TCP write timeout
    QCLOUD_ERR_TCP_READ_FAIL       = -606,  // TCP read error
    QCLOUD_ERR_TCP_WRITE_FAIL      = -607,  // TCP write error
    QCLOUD_ERR_TCP_PEER_SHUTDOWN   = -608,  // TCP server close connection
    QCLOUD_ERR_TCP_NOTHING_TO_READ = -609,  // TCP socket nothing to read

    QCLOUD_ERR_SSL_INIT            = -701,  // TLS/SSL init fail
    QCLOUD_ERR_SSL_CERT            = -702,  // TLS/SSL certificate issue
    QCLOUD_ERR_SSL_CONNECT         = -703,  // TLS/SSL connect fail
    QCLOUD_ERR_SSL_CONNECT_TIMEOUT = -704,  // TLS/SSL connect timeout
    QCLOUD_ERR_SSL_WRITE_TIMEOUT   = -705,  // TLS/SSL write timeout
    QCLOUD_ERR_SSL_WRITE           = -706,  // TLS/SSL write error
    QCLOUD_ERR_SSL_READ_TIMEOUT    = -707,  // TLS/SSL read timeout
    QCLOUD_ERR_SSL_READ            = -708,  // TLS/SSL read error
    QCLOUD_ERR_SSL_NOTHING_TO_READ = -709,  // TLS/SSL nothing to read

    QCLOUD_ERR_BIND_PARA_ERR        = -801,  // bind sub device param error
    QCLOUD_ERR_BIND_SUBDEV_ERR      = -802,  // sub device not exist or illegal
    QCLOUD_ERR_BIND_SIGN_ERR        = -803,  // signature check err
    QCLOUD_ERR_BIND_SIGN_METHOD_RRR = -804,  // signmethod not supporte
    QCLOUD_ERR_BIND_SIGN_EXPIRED    = -805,  // signature expired
    QCLOUD_ERR_BIND_BEEN_BINDED     = -806,  // sub device has been binded by other gateway
    QCLOUD_ERR_BIND_SUBDEV_FORBID   = -807,  // sub device not allow to bind
    QCLOUD_ERR_BIND_OP_FORBID       = -808,  // operation not permit
    QCLOUD_ERR_BIND_REPEATED_REQ    = -809,  // repeated bind request,has been binded
} IoT_Return_Code;
