//bsp_network.c
#include <stdio.h>

#include "bsp_network.h"


#include "mbedtls/platform.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/pk.h"
//#include "mbedtls/certs.h"
#include "mbedtls/x509.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/error.h"
#if defined(ESP_PLATFORM)
#include "mbedtls/esp_debug.h"   /* ESP-specific mbedTLS debug hooks; non-ESP chips don't need it */
#endif


#include <netdb.h>
// Some ESP-IDF/lwIP builds provide netdb symbols in <lwip/netdb.h>
#include <lwip/netdb.h>

/* Ensure declaration for gai_strerror exists for tools/SDKs that don't
 * declare it; redeclaration is harmless when the header provides it.
 */
extern const char *gai_strerror(int err);


#include "bsp_system.h"
#include "bsp_log.h"

//mbedtls_pk_parse_key 待适配

#define NET_WORK_LOG		Log_Debug

#define STRING_PTR_PRINT_SANITY_CHECK(ptr) ((ptr) ? (ptr) : "null")

#define TCP_READ_TIMEOUT_DIAG_INTERVAL_MS 2000U

static uint32_t s_tcp_read_timeout_count = 0U;
static uint32_t s_tcp_read_timeout_last_log_ms = 0U;

static int Bsp_Network_Get_Socket_Error(int fd)
{
    int error = 0;
    socklen_t len = sizeof(error);

    if (fd < 0)
    {
        return -1;
    }

    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) != 0)
    {
        return -1;
    }

    return error;
}

/* Some embedded/lwIP builds don't expose gai_strerror; provide a safe fallback
 * that returns a small string representation of the error code. This avoids
 * implicit declaration errors when gai_strerror isn't available.
 */
static const char *bsp_gai_strerror_safe(int err)
{
    static char buf[32];
    snprintf(buf, sizeof(buf), "gai_err=%d", err);
    return buf;
}


//TCP上下文
struct Tcp_Context {
	int Fd;
    uint32_t Generation;
    uint32_t Active_Reads;
    uint32_t Active_Writes;
    uint32_t Close_Count;
    int Last_Read_Ret;
    int Last_Read_Errno;
    int Last_Read_Select_Ret;
    int Last_Read_So_Error;
    uint32_t Last_Read_Elapsed_Ms;
    int Last_Write_Ret;
    int Last_Write_Errno;
    int Last_Write_Select_Ret;
    int Last_Write_So_Error;
    uint32_t Last_Write_Elapsed_Ms;
};

struct Tls_Context {
	mbedtls_ssl_context Ssl;
	mbedtls_ssl_config Config;
	mbedtls_x509_crt Cacert;
	mbedtls_x509_crt Clicert;
	mbedtls_pk_context Pkey;
	mbedtls_net_context Server_Fd;
	uint32_t Flags;
};



/**
*@名称 		_Time_Left
*@功能 		根据SOCKET获取IP
*@参数 		const int fd, struct sockaddr_in* local_addr
*@返回值 	int
*@使用说明	
*/
static uint32_t _Time_Left(uint32_t t_end, uint32_t t_now)
{
    uint32_t t_left;

    if (t_end > t_now) 
	{
        t_left = t_end - t_now;
    } 
	else 
	{
        t_left = 0;
    }

    return t_left;
}


/**
*@名称 		Get_Socket_Local_Ip
*@功能 		根据SOCKET获取IP
*@参数 		const int fd, struct sockaddr_in* local_addr
*@返回值 	int
*@使用说明	
*/
static int Get_Socket_Local_Ip(const int fd, struct sockaddr_in* local_addr)
{
    if(fd < 0){
        NET_WORK_LOG("socket fd invalid\r\n");
        return -1;
    }
    socklen_t local_len = sizeof(struct sockaddr_in);  
    if (getsockname(fd, (struct sockaddr *)local_addr, &local_len) < 0) {  
        NET_WORK_LOG("getsockname failed: %s\n", STRING_PTR_PRINT_SANITY_CHECK(strerror(errno)));
        return -1;
    }  
    return 0;
}

/**
*@名称 		Get_Socket_Interface_Mac
*@功能 		根据SOCKET获取MAC
*@参数 		const int fd, const char* interface_name, char* buf, const int max_size
*@返回值 	int
*@使用说明	
*/
static int Get_Socket_Interface_Mac(const int fd, const char* interface_name, char* buf, const int max_size)
{
#if 0
    struct ifreq ifr;
    strncpy(ifr.ifr_name, interface_name, IFNAMSIZ - 1);
    ifr.ifr_name[IFNAMSIZ - 1] = '\0';

    if (ioctl(fd, SIOCGIFHWADDR, &ifr) == -1) {
        NET_WORK_LOG("ioctl SIOCGIFHWADDR error:%s\r\n", STRING_PTR_PRINT_SANITY_CHECK(strerror(errno)));
        return -1;
    }

    unsigned char *mac_addr = (unsigned char *)ifr.ifr_hwaddr.sa_data;
    snprintf(buf, max_size, "%02X:%02X:%02X:%02X:%02X:%02X", mac_addr[0], mac_addr[1], mac_addr[2],
                                                                mac_addr[3], mac_addr[4], mac_addr[5]);
#endif
    return 0;
}

/**
*@名称 		Get_Socket_Interface_Name
*@功能 		根据SOCKET获取接口名称
*@参数 		const int fd, const struct sockaddr_in* local_addr, char* buf, const int max_size
*@返回值 	int
*@使用说明	
*/
static int Get_Socket_Interface_Name(const int fd, const struct sockaddr_in* local_addr, char* buf, const int max_size)
{
#if 0
    int result = -1;
    struct ifaddrs *ifaddr = NULL, *ifa = NULL;  

    if(fd < 0){
        NET_WORK_LOG("socket fd invalid\r\n");
        result = -1;
        goto exit;
    }

    if (getifaddrs(&ifaddr) == -1) {  
        NET_WORK_LOG("getifaddrs failed: %s\n", STRING_PTR_PRINT_SANITY_CHECK(strerror(errno)));
        result = -1;
        goto exit;
    }  
  
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) 
	{  
        if (ifa->ifa_addr == NULL)  
            continue;  
  
        if (ifa->ifa_addr->sa_family == AF_INET)
		{ // 检查IPv4地址  
            struct sockaddr_in *s4 = (struct sockaddr_in *)ifa->ifa_addr;  

            char ifa_ip[64] = {0};
            inet_ntop(AF_INET, &s4->sin_addr.s_addr, ifa_ip, sizeof(ifa_ip) - 1);
            NET_WORK_LOG("ifa name:%s, ip addr:%s\r\n", ifa->ifa_name, ifa_ip);

            if (s4->sin_addr.s_addr == local_addr->sin_addr.s_addr) 
			{  
                strncpy(buf, ifa->ifa_name, max_size);
                result = 0;
                goto exit;
            }  
        }  
    }  
  
exit:
    if(ifaddr) freeifaddrs(ifaddr);  
    return result;
#endif 
    return 0;
}

/**
*@名称 		Get_Socket_Interface_Info
*@功能 		根据SOCKET获取接口相关信息
*@参数 		int fd, Network_Interface_Info_t* p_info
*@返回值 	int
*@使用说明	
*/
static int Get_Socket_Interface_Info(int fd, Network_Interface_Info_t* p_info)
{
    if(fd < 0){
        NET_WORK_LOG("socket fd invalid\r\n");
        return -1;
    }

    Network_Interface_Info_t info = {0};

    struct sockaddr_in local_addr;  
    if(Get_Socket_Local_Ip(fd, &local_addr) != 0){
        NET_WORK_LOG("get socket local ip failed\n");
        return -1; 
    }
    inet_ntop(AF_INET, &local_addr.sin_addr.s_addr, info.Ip, sizeof(info.Ip) - 1);
    info.Local_Port = ntohs(local_addr.sin_port);

    if(Get_Socket_Interface_Name(fd, &local_addr, info.Name, sizeof(info.Name) - 1) != 0){
        NET_WORK_LOG("get socket interface name failed.\r\n");
        return -1;
    }

    if(Get_Socket_Interface_Mac(fd, info.Name, info.Mac, sizeof(info.Mac) - 1) != 0){
        NET_WORK_LOG("get socket interface mac failed.\r\n");
        return -1;
    }

    *p_info = info;

    return 0;
}

/**
*@名称 		Network_Tcp_Connect
*@功能 		TCP连接
*@参数 		Network_Context_t *pNetwork, const void *tcp_params, const void *tls_params
*@返回值 	int
*@使用说明	
*/
int Network_Tcp_Connect(Network_Context_t *pNetwork, const void *tcp_params, const void *tls_params)
{
	int ret = 0;
	Tcp_Context_t *tcp_data_params = NULL;
    const int dns_max_retries = 3;
    int dns_retry_count = 0;

	if(NULL == pNetwork) {
		return NET_INVALID_PARM;
	}

	if(NULL != tcp_params) {
		pNetwork->Tcp_Connect_Params = *((Tcp_Connect_Params_t*)tcp_params);
	}

	tcp_data_params = (Tcp_Context_t*)(pNetwork->Tcp_Context);

    if (tcp_data_params->Fd >= 0)
    {
        int old_fd = tcp_data_params->Fd;
        NET_WORK_LOG("[NET_DIAG][PRECONNECT_CLOSE_STALE_FD] fd=%d gen=%u active_r=%u active_w=%u close_count=%u\r\n",
                     old_fd,
                     (unsigned int)tcp_data_params->Generation,
                     (unsigned int)tcp_data_params->Active_Reads,
                     (unsigned int)tcp_data_params->Active_Writes,
                     (unsigned int)tcp_data_params->Close_Count);
        shutdown(old_fd, SHUT_RDWR);
        close(old_fd);
        tcp_data_params->Fd = -1;
        tcp_data_params->Close_Count++;
    }

	struct addrinfo hints, *addr_list, *cur;
	int fd = -1;
	char port_str[6] = {0};
    snprintf(port_str, 6, "%d", pNetwork->Tcp_Connect_Params.Port);
    NET_WORK_LOG("connect host: %s:%s", pNetwork->Tcp_Connect_Params.Host, port_str);
    memset(&hints, 0x00, sizeof(hints));
    hints.ai_family   = AF_INET;       /* IDF 5.5: AF_UNSPEC 會觸發 AAAA query 延遲，強制 IPv4 */
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    for (dns_retry_count = 0; dns_retry_count < dns_max_retries; dns_retry_count++)
    {
        addr_list = NULL;
        ret = getaddrinfo(pNetwork->Tcp_Connect_Params.Host, port_str, &hints, &addr_list);
        if (ret == 0)
        {
            NET_WORK_LOG("getaddrinfo success, connecting.\r\n");
            break;
        }

        NET_WORK_LOG("getaddrinfo %s:%s failed, rc=%d, msg=%s, retry %d/%d\r\n",
                 pNetwork->Tcp_Connect_Params.Host,
                 port_str,
                 ret,
                 bsp_gai_strerror_safe(ret),
                 dns_retry_count + 1,
                 dns_max_retries);
        if (dns_retry_count < dns_max_retries - 1)
        {
            Bsp_Sleep_Ms(1000);
        }
    }

    if (ret != 0)
    {
        return NET_FAILED;
    }

    for (cur = addr_list; cur != NULL; cur = cur->ai_next) 
	{
        fd = (int)socket(cur->ai_family, cur->ai_socktype, cur->ai_protocol);
        if (fd < 0) {
            ret = 0;
            continue;
        } 
		/* IDF 5.5 lwIP：SO_RCVTIMEO / SO_SNDTIMEO 會影響 select() 提前返回 0，
         * 導致 PUBACK / PINGRESP 全部漏收 → MQTTKeepAliveTimeout。
         * Network_Tcp_Read 已用 select(200ms) 自帶超時，移除 socket-level timeout。*/
        int on = 1;
        errno = 0;
        int so_keepalive_ret = setsockopt(fd, SOL_SOCKET,  SO_KEEPALIVE,  (void *)&on, sizeof(int));
        int so_keepalive_errno = errno;
        errno = 0;
        int tcp_nodelay_ret = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY,   (void *)&on, sizeof(int));
        int tcp_nodelay_errno = errno;
        /* TCP keepalive 在 MQTT keepalive(25s) 之前先探測，雙重保障。
         * KEEPIDLE=15s：最後一次 TCP 收發後 15s 開始探測（< MQTT keepalive 25s）
         * KEEPINTVL=3s, KEEPCNT=3：3×3=9s 後仍無回應則關 socket
         * → 死連線最遲 15+9=24s 被 TCP 層偵測，早於 MQTT 層的 25+8=33s */
        int keepidle  = 15;
        int keepintvl =  3;
        int keepcnt   =  3;
        errno = 0;
        int tcp_keepidle_ret = setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE,  &keepidle,  sizeof(keepidle));
        int tcp_keepidle_errno = errno;
        errno = 0;
        int tcp_keepintvl_ret = setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &keepintvl, sizeof(keepintvl));
        int tcp_keepintvl_errno = errno;
        errno = 0;
        int tcp_keepcnt_ret = setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT,   &keepcnt,   sizeof(keepcnt));
        int tcp_keepcnt_errno = errno;
        NET_WORK_LOG("[NET_DIAG][TCP_KEEPALIVE_OPT] fd=%d keepalive_ret=%d errno=%d "
                     "nodelay_ret=%d errno=%d keepidle=%d ret=%d errno=%d "
                     "keepintvl=%d ret=%d errno=%d keepcnt=%d ret=%d errno=%d\r\n",
                     fd,
                     so_keepalive_ret, so_keepalive_errno,
                     tcp_nodelay_ret, tcp_nodelay_errno,
                     keepidle, tcp_keepidle_ret, tcp_keepidle_errno,
                     keepintvl, tcp_keepintvl_ret, tcp_keepintvl_errno,
                     keepcnt, tcp_keepcnt_ret, tcp_keepcnt_errno);

        char srv_ip[64] = {0};
        struct sockaddr_in *addr_in = (struct sockaddr_in *)cur->ai_addr;
        inet_ntop(AF_INET, &addr_in->sin_addr.s_addr, srv_ip, sizeof(srv_ip) - 1);

        if (connect(fd, cur->ai_addr, cur->ai_addrlen) == 0) { 
            memset(&pNetwork->Interface_Info, 0, sizeof(pNetwork->Interface_Info));
            Get_Socket_Interface_Info(fd, &pNetwork->Interface_Info);  
            NET_WORK_LOG("sockfd: %d, local_port: %d", fd, pNetwork->Interface_Info.Local_Port);
            NET_WORK_LOG("server ip: %s port: %d", srv_ip, pNetwork->Tcp_Connect_Params.Port);
            NET_WORK_LOG("interface ip: %s", pNetwork->Interface_Info.Ip);
            NET_WORK_LOG("interface name: %s", pNetwork->Interface_Info.Name);
            NET_WORK_LOG("interface mac: %s", pNetwork->Interface_Info.Mac);
          
            ret = fd;
            tcp_data_params->Fd = fd;
            tcp_data_params->Generation++;
            NET_WORK_LOG("[NET_DIAG][FD_OPEN] fd=%d gen=%u local_port=%d active_r=%u active_w=%u close_count=%u\r\n",
                         fd,
                         (unsigned int)tcp_data_params->Generation,
                         pNetwork->Interface_Info.Local_Port,
                         (unsigned int)tcp_data_params->Active_Reads,
                         (unsigned int)tcp_data_params->Active_Writes,
                         (unsigned int)tcp_data_params->Close_Count);
            break;
        }
		else{
            NET_WORK_LOG("connect failed (error:%d %s) server ip: %s port: %d", errno, STRING_PTR_PRINT_SANITY_CHECK(strerror(errno)), srv_ip, pNetwork->Tcp_Connect_Params.Port);
        }

        close(fd);
        ret = 0;
    }

    freeaddrinfo(addr_list);

    if (ret == 0) {
        NET_WORK_LOG("failed to connect with TCP server: %s:%s", pNetwork->Tcp_Connect_Params.Host, port_str);
        return NET_CONNECT_FAILED;
    } 

	return 0;
}

/**
*@名称 		Network_Tcp_Disconnect
*@功能 		TCP断开连接
*@参数 		Network_Context_t *pNetwork
*@返回值 	int
*@使用说明	
*/
int Network_Tcp_Disconnect(Network_Context_t *pNetwork) 
{
	Tcp_Context_t *tcp_data_params = (pNetwork->Tcp_Context);
    int fd = tcp_data_params->Fd;
    uint32_t gen = tcp_data_params->Generation;

    int so_error = Bsp_Network_Get_Socket_Error(fd);
    NET_WORK_LOG("[NET_DIAG][SOCKET_BEFORE_CLOSE] fd=%d gen=%u so_error=%d "
                 "last_read_ret=%d last_read_errno=%d last_read_select=%d last_read_so_error=%d last_read_elapsed=%u "
                 "last_write_ret=%d last_write_errno=%d last_write_select=%d last_write_so_error=%d last_write_elapsed=%u "
                 "active_r=%u active_w=%u close_count=%u\r\n",
                 fd,
                 (unsigned int)gen,
                 so_error,
                 tcp_data_params->Last_Read_Ret,
                 tcp_data_params->Last_Read_Errno,
                 tcp_data_params->Last_Read_Select_Ret,
                 tcp_data_params->Last_Read_So_Error,
                 (unsigned int)tcp_data_params->Last_Read_Elapsed_Ms,
                 tcp_data_params->Last_Write_Ret,
                 tcp_data_params->Last_Write_Errno,
                 tcp_data_params->Last_Write_Select_Ret,
                 tcp_data_params->Last_Write_So_Error,
                 (unsigned int)tcp_data_params->Last_Write_Elapsed_Ms,
                 (unsigned int)tcp_data_params->Active_Reads,
                 (unsigned int)tcp_data_params->Active_Writes,
                 (unsigned int)tcp_data_params->Close_Count);
    NET_WORK_LOG("[NET_DIAG][FD_CLOSE_BEGIN] fd=%d gen=%u active_r=%u active_w=%u close_count=%u\r\n",
                 fd,
                 (unsigned int)gen,
                 (unsigned int)tcp_data_params->Active_Reads,
                 (unsigned int)tcp_data_params->Active_Writes,
                 (unsigned int)tcp_data_params->Close_Count);
    NET_WORK_LOG("close socket fd: %d\r\n", fd);
    if(fd >= 0) {
        shutdown(fd, SHUT_RDWR);
        close(fd);
        tcp_data_params->Fd = -1;
    }
    tcp_data_params->Close_Count++;
    NET_WORK_LOG("[NET_DIAG][FD_CLOSE_END] fd=%d gen=%u active_r=%u active_w=%u close_count=%u now_fd=%d\r\n",
                 fd,
                 (unsigned int)gen,
                 (unsigned int)tcp_data_params->Active_Reads,
                 (unsigned int)tcp_data_params->Active_Writes,
                 (unsigned int)tcp_data_params->Close_Count,
                 tcp_data_params->Fd);
    memset(&pNetwork->Interface_Info, 0, sizeof(pNetwork->Interface_Info));
	return 0;
}

/**
*@名称 		Network_Tcp_Destroy
*@功能 		TCP反初始化
*@参数 		Network_Context_t *pNetwork
*@返回值 	int
*@使用说明	
*/
int Network_Tcp_Destroy(Network_Context_t *pNetwork)
{
	Tcp_Context_t *tcp_data_params = (pNetwork->Tcp_Context);

	if (tcp_data_params) 
		Bsp_Psram_Free(tcp_data_params);
	pNetwork->Tcp_Context = NULL;
	return 0;
}

/**
*@名称 		Network_Tcp_Write
*@功能 		TCP写数据
*@参数 		Network_Context_t *pNetwork, const unsigned char *pMsg, size_t len
*@返回值 	int
*@使用说明	
*/
int Network_Tcp_Write(Network_Context_t *pNetwork, const unsigned char *pMsg, size_t len)
{ 
    int      ret;
    uint32_t len_sent;
    uint32_t t_end, t_left;
    fd_set   sets;

    if(NULL == pNetwork){
        NET_WORK_LOG("pNetwork is NULL\r\n");
        return NET_INVALID_PARM;
    }

    Tcp_Context_t *tcp_data_params = (Tcp_Context_t*)(pNetwork->Tcp_Context); 
    if(NULL == tcp_data_params){
        NET_WORK_LOG("tcp_data_params is NULL\r\n");
        return NET_INVALID_PARM;
    }

    int fd = tcp_data_params->Fd;
    if(fd < 0){
        NET_WORK_LOG("[NET_DIAG][WRITE_INVALID_FD] fd=%d gen=%u active_r=%u active_w=%u close_count=%u len=%u\r\n",
                     fd,
                     (unsigned int)tcp_data_params->Generation,
                     (unsigned int)tcp_data_params->Active_Reads,
                     (unsigned int)tcp_data_params->Active_Writes,
                     (unsigned int)tcp_data_params->Close_Count,
                     (unsigned int)len);
        NET_WORK_LOG("invalid socket fd: %d\r\n", fd);
        return NET_INVALID_PARM;
    }
    uint32_t fd_generation = tcp_data_params->Generation;
    tcp_data_params->Active_Writes++;
    if (len <= 2U)
    {
        NET_WORK_LOG("[NET_DIAG][WRITE_BEGIN] fd=%d gen=%u len=%u active_r=%u active_w=%u close_count=%u\r\n",
                     (int)fd,
                     (unsigned int)fd_generation,
                     (unsigned int)len,
                     (unsigned int)tcp_data_params->Active_Reads,
                     (unsigned int)tcp_data_params->Active_Writes,
                     (unsigned int)tcp_data_params->Close_Count);
    }

    uint32_t timeout_ms = 200;
    const unsigned char *buf = pMsg;  

    uint32_t start_ms = Bsp_Get_Run_Time_Ms();
    uint32_t select_ready_count = 0;
    uint32_t send_call_count = 0;
    int last_errno = 0;
    int last_select_ret = 0;
    t_end    = Bsp_Get_Run_Time_Ms() + timeout_ms;
    len_sent = 0;
    ret      = 1; /* send one time if timeout_ms is value 0 */

    do {
        t_left = _Time_Left(t_end, Bsp_Get_Run_Time_Ms());  
        if (0 != t_left) 
		{
            struct timeval timeout; 
            FD_ZERO(&sets);
            FD_SET(fd, &sets);

            timeout.tv_sec  = t_left / 1000;
            timeout.tv_usec = (t_left % 1000) * 1000;  
            errno = 0;
            ret = select(fd + 1, NULL, &sets, NULL, &timeout);
            last_select_ret = ret;
            if (ret > 0) 
			{
                select_ready_count++;
                if (0 == FD_ISSET(fd, &sets)) {
                    NET_WORK_LOG("Should NOT arrive\r\n");
                    /* If timeout in next loop, it will not sent any data */
                    ret = 0;
                    continue;
                }
            } 
			else if (0 == ret) 
			{
                ret = NET_TIMEOUT;
                NET_WORK_LOG("[NET_DIAG][WRITE_SELECT_TIMEOUT] fd=%d sent=%u/%u left=%u elapsed=%u\r\n",
                             (int)fd,
                             (unsigned int)len_sent,
                             (unsigned int)len,
                             (unsigned int)t_left,
                             (unsigned int)(Bsp_Get_Run_Time_Ms() - start_ms));
                break;
            } 
			else 
			{
                last_errno = errno;
                if (EINTR == errno) 
				{
                    NET_WORK_LOG("EINTR be caught\r\n");
                    continue;
                }

                ret = NET_FAILED;
                NET_WORK_LOG("select-write fail: %s", STRING_PTR_PRINT_SANITY_CHECK(strerror(errno)));
                break;
            }
        } 
		else 
		{
            NET_WORK_LOG("send timeout.\r\n");
            ret = NET_TIMEOUT;
        }
		//得到控制权
        if (ret > 0) 
		{ 
            size_t want_len = len - len_sent;
            errno = 0;
            ret = send(fd, buf + len_sent, len - len_sent, MSG_NOSIGNAL);
            send_call_count++;
            if (ret > 0)
            {
                if ((size_t)ret < want_len)
                {
                    NET_WORK_LOG("[NET_DIAG][WRITE_PARTIAL_CHUNK] fd=%d chunk=%d want=%u sent_total=%u/%u\r\n",
                                 (int)fd,
                                 ret,
                                 (unsigned int)want_len,
                                 (unsigned int)(len_sent + (uint32_t)ret),
                                 (unsigned int)len);
                }
                len_sent += ret;
            } 
            else if (0 == ret) 
            {
                NET_WORK_LOG("No data be sent. Should NOT arrive\r\n");
            }
            else 
            {
                last_errno = errno;
                if (EINTR == errno)
                {
                    NET_WORK_LOG("EINTR be caught\r\n");
                    continue;
                }

                ret = NET_FAILED;
                NET_WORK_LOG("[NET_DIAG][WRITE_SEND_FAIL] fd=%d gen=%u now_fd=%d now_gen=%u errno=%d(%s) "
                             "sent=%u/%u calls=%u ready=%u active_r=%u active_w=%u close_count=%u\r\n",
                             (int)fd,
                             (unsigned int)fd_generation,
                             tcp_data_params->Fd,
                             (unsigned int)tcp_data_params->Generation,
                             errno,
                             STRING_PTR_PRINT_SANITY_CHECK(strerror(errno)),
                             (unsigned int)len_sent,
                             (unsigned int)len,
                             (unsigned int)send_call_count,
                             (unsigned int)select_ready_count,
                             (unsigned int)tcp_data_params->Active_Reads,
                             (unsigned int)tcp_data_params->Active_Writes,
                             (unsigned int)tcp_data_params->Close_Count);
                break;
            }
        } 
    } while ((len_sent < len) && (_Time_Left(t_end, Bsp_Get_Run_Time_Ms()) > 0));

    if ((tcp_data_params->Fd != fd) || (tcp_data_params->Generation != fd_generation))
    {
        NET_WORK_LOG("[NET_DIAG][WRITE_FD_CHANGED] fd=%d gen=%u now_fd=%d now_gen=%u len=%u sent=%u elapsed=%u\r\n",
                     (int)fd,
                     (unsigned int)fd_generation,
                     tcp_data_params->Fd,
                     (unsigned int)tcp_data_params->Generation,
                     (unsigned int)len,
                     (unsigned int)len_sent,
                     (unsigned int)(Bsp_Get_Run_Time_Ms() - start_ms));
    }

    if (len <= 2U)
    {
        NET_WORK_LOG("[NET_DIAG][WRITE_END] fd=%d gen=%u now_fd=%d now_gen=%u len=%u sent=%u ret=%d elapsed=%u "
                     "active_r=%u active_w=%u close_count=%u\r\n",
                     (int)fd,
                     (unsigned int)fd_generation,
                     tcp_data_params->Fd,
                     (unsigned int)tcp_data_params->Generation,
                     (unsigned int)len,
                     (unsigned int)len_sent,
                     ret,
                     (unsigned int)(Bsp_Get_Run_Time_Ms() - start_ms),
                     (unsigned int)tcp_data_params->Active_Reads,
                     (unsigned int)tcp_data_params->Active_Writes,
                     (unsigned int)tcp_data_params->Close_Count);
    }
    tcp_data_params->Last_Write_Ret = ret;
    tcp_data_params->Last_Write_Errno = last_errno;
    tcp_data_params->Last_Write_Select_Ret = last_select_ret;
    tcp_data_params->Last_Write_So_Error = Bsp_Network_Get_Socket_Error(fd);
    tcp_data_params->Last_Write_Elapsed_Ms = Bsp_Get_Run_Time_Ms() - start_ms;
    if (tcp_data_params->Active_Writes > 0U)
    {
        tcp_data_params->Active_Writes--;
    }

    if(len_sent == len)
    {
        return (int)len_sent;   /* 完整發送：傳輸層契約滿足 */
    }
    else if(len_sent > 0)
    {
        /* 部分發送：coreMQTT 傳輸介面規範要求 send 必須全量返回或返回負值錯誤；
         * 返回正值但 < len 時，sendPacket 只判 < 0，會誤當成功繼續發 payload，
         * 導致 TCP 流損壞 → broker 斷線 → 訂閱斷線（pub 後立即 disconnect 的根因）。
         * 改為返回 NET_TIMEOUT（負值），上層 sendPacket 收到負值即報 MQTTSendFailed，
         * 外層 Mqtt_Client_Publish 返回 0（失敗），呼叫方可重試，不污染 TCP 流。*/
        NET_WORK_LOG("[NET_WRITE] 部分發送 sent=%d/%d，返回 NET_TIMEOUT\r\n",
                     (int)len_sent, (int)len);
        NET_WORK_LOG("[NET_DIAG][WRITE_PARTIAL_RETURN] fd=%d sent=%u/%u elapsed=%u calls=%u ready=%u\r\n",
                     (int)fd,
                     (unsigned int)len_sent,
                     (unsigned int)len,
                     (unsigned int)(Bsp_Get_Run_Time_Ms() - start_ms),
                     (unsigned int)send_call_count,
                     (unsigned int)select_ready_count);
        return NET_TIMEOUT;
    }
    else
    {
        return ret;
    }
}

/**
*@名称 		Network_Tcp_Read
*@功能 		TCP读数据
*@参数 		Network_Context_t *pNetwork, const unsigned char *pMsg, size_t len
*@返回值 	int
*@使用说明	
*/
int Network_Tcp_Read(Network_Context_t *pNetwork, unsigned char *pMsg, size_t len)
{
    int            ret, err_code;
    uint32_t       len_recv;
    uint32_t       t_end, t_left;
    fd_set         sets;
    struct timeval timeout;
    uint32_t       start_ms;
    uint32_t       eagain_count;
    uint32_t       select_ready_count;
    uint32_t       recv_call_count;
    int            last_errno;
    int            last_select_ret;

    Tcp_Context_t *tcp_data_params = (Tcp_Context_t*)(pNetwork->Tcp_Context);
    if(NULL == tcp_data_params){
        NET_WORK_LOG("tcp_data_params is NULL\r\n");
        return NET_INVALID_PARM;
    }

    int fd = tcp_data_params->Fd;
    if(fd < 0){
        NET_WORK_LOG("[NET_DIAG][READ_INVALID_FD] fd=%d gen=%u active_r=%u active_w=%u close_count=%u len=%u\r\n",
                     fd,
                     (unsigned int)tcp_data_params->Generation,
                     (unsigned int)tcp_data_params->Active_Reads,
                     (unsigned int)tcp_data_params->Active_Writes,
                     (unsigned int)tcp_data_params->Close_Count,
                     (unsigned int)len);
        NET_WORK_LOG("invalid socket fd: %d\r\n", fd);
        return NET_INVALID_PARM;
    }
    uint32_t fd_generation = tcp_data_params->Generation;
    tcp_data_params->Active_Reads++;

    uint32_t timeout_ms = 200;
    unsigned char *buf = pMsg;

    t_end    = Bsp_Get_Run_Time_Ms() + timeout_ms;
    len_recv = 0;
    err_code = 0;
    start_ms = Bsp_Get_Run_Time_Ms();
    eagain_count = 0;
    select_ready_count = 0;
    recv_call_count = 0;
    last_errno = 0;
    last_select_ret = 0;

    do {
        t_left = _Time_Left(t_end, Bsp_Get_Run_Time_Ms());
        if (0 == t_left) {
            err_code = NET_TIMEOUT;
            break;
        }
        FD_ZERO(&sets);
        FD_SET(fd, &sets);
        timeout.tv_sec  = t_left / 1000;
        timeout.tv_usec = (t_left % 1000) * 1000;

        errno = 0;
        ret = select(fd + 1, &sets, NULL, NULL, &timeout);
        last_select_ret = ret;
        if (ret > 0) 
		{
            select_ready_count++;
            errno = 0;
            ret = recv(fd, buf + len_recv, len - len_recv, 0);
            recv_call_count++;
            if (ret > 0) 
			{
                len_recv += ret;
            } 
			else if (0 == ret) 
			{
                NET_WORK_LOG("[NET_DIAG][READ_EOF] fd=%d gen=%u recv=%u/%u elapsed=%u ready=%u calls=%u "
                             "active_r=%u active_w=%u close_count=%u\r\n",
                             (int)fd,
                             (unsigned int)fd_generation,
                             (unsigned int)len_recv,
                             (unsigned int)len,
                             (unsigned int)(Bsp_Get_Run_Time_Ms() - start_ms),
                             (unsigned int)select_ready_count,
                             (unsigned int)recv_call_count,
                             (unsigned int)tcp_data_params->Active_Reads,
                             (unsigned int)tcp_data_params->Active_Writes,
                             (unsigned int)tcp_data_params->Close_Count);
                err_code = NET_CONN_EOF;
                break;
            } 
			else
			{
                last_errno = errno;
                if (EINTR == errno) {
                    NET_WORK_LOG("EINTR be caught\r\n");
                    continue;
                }
                /* IDF 5.5 lwIP 在 select() 返回就緒後偶爾仍回 EAGAIN，重試即可 */
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    eagain_count++;
                    continue;
                }
                NET_WORK_LOG("[NET_DIAG][READ_RECV_ERROR] fd=%d gen=%u now_fd=%d now_gen=%u errno=%d(%s) "
                             "recv=%u/%u eagain=%u active_r=%u active_w=%u close_count=%u\r\n",
                             (int)fd,
                             (unsigned int)fd_generation,
                             tcp_data_params->Fd,
                             (unsigned int)tcp_data_params->Generation,
                             errno,
                             STRING_PTR_PRINT_SANITY_CHECK(strerror(errno)),
                             (unsigned int)len_recv,
                             (unsigned int)len,
                             (unsigned int)eagain_count,
                             (unsigned int)tcp_data_params->Active_Reads,
                             (unsigned int)tcp_data_params->Active_Writes,
                             (unsigned int)tcp_data_params->Close_Count);
                err_code = NET_FAILED;
                break;
            }
        } 
		else if (0 == ret) 
		{
            err_code = NET_TIMEOUT;
            break;
        } 
		else 
		{
            last_errno = errno;
            NET_WORK_LOG("[NET_DIAG][READ_SELECT_ERROR] fd=%d gen=%u now_fd=%d now_gen=%u errno=%d(%s) "
                         "recv=%u/%u eagain=%u active_r=%u active_w=%u close_count=%u\r\n",
                         (int)fd,
                         (unsigned int)fd_generation,
                         tcp_data_params->Fd,
                         (unsigned int)tcp_data_params->Generation,
                         errno,
                         STRING_PTR_PRINT_SANITY_CHECK(strerror(errno)),
                         (unsigned int)len_recv,
                         (unsigned int)len,
                         (unsigned int)eagain_count,
                         (unsigned int)tcp_data_params->Active_Reads,
                         (unsigned int)tcp_data_params->Active_Writes,
                         (unsigned int)tcp_data_params->Close_Count);
            err_code = NET_FAILED;
            break;
        }
    } while ((len_recv < len));

    if (eagain_count > 0)
    {
        NET_WORK_LOG("[NET_DIAG][READ_EAGAIN_RETRY] fd=%d count=%u recv=%u/%u elapsed=%u ready=%u calls=%u err=%d\r\n",
                     (int)fd,
                     (unsigned int)eagain_count,
                     (unsigned int)len_recv,
                     (unsigned int)len,
                     (unsigned int)(Bsp_Get_Run_Time_Ms() - start_ms),
                     (unsigned int)select_ready_count,
                     (unsigned int)recv_call_count,
                     err_code);
    }

    if ((err_code == NET_TIMEOUT) && (len_recv == 0U))
    {
        uint32_t now_ms = Bsp_Get_Run_Time_Ms();
        s_tcp_read_timeout_count++;
        if ((s_tcp_read_timeout_count <= 3U) ||
            (now_ms - s_tcp_read_timeout_last_log_ms >= TCP_READ_TIMEOUT_DIAG_INTERVAL_MS))
        {
            s_tcp_read_timeout_last_log_ms = now_ms;
            int so_error = Bsp_Network_Get_Socket_Error(fd);
            NET_WORK_LOG("[NET_DIAG][READ_SELECT_TIMEOUT_ONLY] fd=%d gen=%u now_fd=%d now_gen=%u wanted=%u elapsed=%u "
                         "select_ret=0 timeout_only=1 ready=%u calls=%u eagain=%u timeout_count=%u so_error=%d "
                         "active_r=%u active_w=%u close_count=%u\r\n",
                         (int)fd,
                         (unsigned int)fd_generation,
                         tcp_data_params->Fd,
                         (unsigned int)tcp_data_params->Generation,
                         (unsigned int)len,
                         (unsigned int)(now_ms - start_ms),
                         (unsigned int)select_ready_count,
                         (unsigned int)recv_call_count,
                         (unsigned int)eagain_count,
                         (unsigned int)s_tcp_read_timeout_count,
                         so_error,
                         (unsigned int)tcp_data_params->Active_Reads,
                         (unsigned int)tcp_data_params->Active_Writes,
                         (unsigned int)tcp_data_params->Close_Count);
        }
    }
    else if (len_recv > 0U)
    {
        if (s_tcp_read_timeout_count > 0U)
        {
            NET_WORK_LOG("[NET_DIAG][READ_RECOVER] fd=%d gen=%u recv=%u/%u after_timeout_count=%u "
                         "elapsed=%u ready=%u calls=%u active_r=%u active_w=%u close_count=%u\r\n",
                         (int)fd,
                         (unsigned int)fd_generation,
                         (unsigned int)len_recv,
                         (unsigned int)len,
                         (unsigned int)s_tcp_read_timeout_count,
                         (unsigned int)(Bsp_Get_Run_Time_Ms() - start_ms),
                         (unsigned int)select_ready_count,
                         (unsigned int)recv_call_count,
                         (unsigned int)tcp_data_params->Active_Reads,
                         (unsigned int)tcp_data_params->Active_Writes,
                         (unsigned int)tcp_data_params->Close_Count);
        }
        s_tcp_read_timeout_count = 0U;
    }

    if ((tcp_data_params->Fd != fd) || (tcp_data_params->Generation != fd_generation))
    {
        NET_WORK_LOG("[NET_DIAG][READ_FD_CHANGED] fd=%d gen=%u now_fd=%d now_gen=%u recv=%u/%u err=%d elapsed=%u\r\n",
                     (int)fd,
                     (unsigned int)fd_generation,
                     tcp_data_params->Fd,
                     (unsigned int)tcp_data_params->Generation,
                     (unsigned int)len_recv,
                     (unsigned int)len,
                     err_code,
                     (unsigned int)(Bsp_Get_Run_Time_Ms() - start_ms));
    }
    if (tcp_data_params->Active_Reads > 0U)
    {
        tcp_data_params->Active_Reads--;
    }
    tcp_data_params->Last_Read_Ret = err_code;
    tcp_data_params->Last_Read_Errno = last_errno;
    tcp_data_params->Last_Read_Select_Ret = last_select_ret;
    tcp_data_params->Last_Read_So_Error = Bsp_Network_Get_Socket_Error(fd);
    tcp_data_params->Last_Read_Elapsed_Ms = Bsp_Get_Run_Time_Ms() - start_ms;

    if((err_code != 0) && (err_code != NET_TIMEOUT)){
        return err_code;
    }

    return len_recv;
}


/**
*@名称 		Network_Tcp_Init
*@功能 		TCP网络接口初始化
*@参数 		Network_Context_t *pNetwork, const Tcp_Connect_Params_t *params
*@返回值 	int
*@使用说明	
*/
int Network_Tcp_Init(Network_Context_t *pNetwork, const Tcp_Connect_Params_t *params)
{
	if (NULL == pNetwork) {
		return NET_INVALID_PARM;
	}

	pNetwork->Connect = Network_Tcp_Connect;
	pNetwork->Read = Network_Tcp_Read;
	pNetwork->Write = Network_Tcp_Write;
	pNetwork->Disconnect = Network_Tcp_Disconnect;
	pNetwork->Destroy = Network_Tcp_Destroy;
	pNetwork->Tcp_Connect_Params = *params;

	Tcp_Context_t *tcp_ctx = Bsp_Psram_Calloc(1, sizeof(Tcp_Context_t));
	if(NULL == tcp_ctx) {
		NET_WORK_LOG("tcp_ctx malloc fail\r\n");
		return NET_INVALID_PARM;
	}
    tcp_ctx->Fd = -1;
	pNetwork->Tcp_Context = tcp_ctx;
	return 0;
}

/**
*@名称 		Mbedtls_Random_Port
*@功能 		
*@参数 		void *p_rng, unsigned char *output, size_t output_len
*@返回值 	int
*@使用说明	
*/
static int Mbedtls_Random_Port(void *p_rng, unsigned char *output, size_t output_len)
{
    int i = 0;
    for(i=0; i<output_len; i++){
		output[i] = (unsigned char)(0xff & Bsp_Random());
    }
    return 0;
}


/**
*@名称 		Network_Tls_Connect
*@功能 		TCP网络安全连接
*@参数 		Network_Context_t *pNetwork, const void *tcp_params, const void *tls_params
*@返回值 	int
*@使用说明	
*/
int Network_Tls_Connect(Network_Context_t *pNetwork, const void *tcp_params, const void *tls_params)
{
	int ret = 0;
	Tls_Context_t *tls_data_params = NULL;
	char portBuffer[6];

	if(NULL == pNetwork) {
		return NET_INVALID_PARM;
	}

    if(NULL != tcp_params) {
		pNetwork->Tcp_Connect_Params = *((Tcp_Connect_Params_t*)tcp_params);
	}

	if(NULL != tls_params) {
		pNetwork->Tls_Connect_Params = *((Tls_Connect_Params_t*)tls_params);
	}

	tls_data_params = (Tls_Context_t*)(pNetwork->Tls_Context);

	mbedtls_ssl_init(&(tls_data_params->Ssl));
	mbedtls_ssl_config_init(&(tls_data_params->Config));
	mbedtls_x509_crt_init(&(tls_data_params->Cacert));
	mbedtls_x509_crt_init(&(tls_data_params->Clicert));
	mbedtls_pk_init(&(tls_data_params->Pkey));

	// Log_InfoDD("Loading the CA root certificate...");
	if (pNetwork->Tls_Connect_Params.Cert_Verify == true)
    {
		ret = mbedtls_x509_crt_parse(&(tls_data_params->Cacert),
									 (const unsigned char *)pNetwork->Tls_Connect_Params.Cacert, 
									 pNetwork->Tls_Connect_Params.Cacert_Len);
		if(ret < 0) {
			NET_WORK_LOG(" failed! mbedtls_x509_crt_parse returned -0x%x while parsing root cert\r\n", -ret);
			return NET_FAILED;
		}
		// Log_InfoDD("ok (%d skipped)", ret);

		if (pNetwork->Tls_Connect_Params.Client_Cert && pNetwork->Tls_Connect_Params.Client_Key) 
        {
			NET_WORK_LOG("Loading the client cert. and key...\r\n");
			ret = mbedtls_x509_crt_parse(&(tls_data_params->Clicert), 
										 (const unsigned char *)pNetwork->Tls_Connect_Params.Client_Cert,
										 pNetwork->Tls_Connect_Params.Client_Cert_Len);
			if(ret != 0) {
				NET_WORK_LOG("failed! mbedtls_x509_crt_parse returned -0x%x while parsing device cert\r\n", -ret);
				mbedtls_x509_crt_free(&(tls_data_params->Cacert));
				return NET_FAILED;
			}
			
			ret = mbedtls_pk_parse_key(&(tls_data_params->Pkey), 
									   (const unsigned char *)pNetwork->Tls_Connect_Params.Client_Key, 
									    pNetwork->Tls_Connect_Params.Client_Key_Len, NULL, 0, NULL, NULL);
			if(ret != 0) {
				NET_WORK_LOG("failed! mbedtls_pk_parse_key returned -0x%x while parsing private key\r\n", -ret);
				mbedtls_x509_crt_free(&(tls_data_params->Cacert));
				return NET_FAILED;
			}
			NET_WORK_LOG("ok");
		}
	}
	snprintf(portBuffer, 6, "%d", pNetwork->Tcp_Connect_Params.Port);
	NET_WORK_LOG("=============Connecting to %s/%s...==========\n", pNetwork->Tcp_Connect_Params.Host, portBuffer);
	if((ret = mbedtls_net_connect(&(tls_data_params->Server_Fd), 
								  pNetwork->Tcp_Connect_Params.Host,
								  portBuffer, MBEDTLS_NET_PROTO_TCP)) != 0) 
    {
		NET_WORK_LOG("failed! mbedtls_net_connect returned -0x%x\r\n", -ret);
		mbedtls_x509_crt_free(&(tls_data_params->Cacert));
		switch(ret) 
        {
			case MBEDTLS_ERR_NET_SOCKET_FAILED:
				return NET_SOCKET_FAILED;
			case MBEDTLS_ERR_NET_UNKNOWN_HOST:
				return NET_UNKNOWN_HOST;
			case MBEDTLS_ERR_NET_CONNECT_FAILED:
			default:
				return NET_CONNECT_FAILED;
		};
	}

	ret = mbedtls_net_set_block(&(tls_data_params->Server_Fd));
	if(ret != 0) 
    {
		NET_WORK_LOG(" failed\n  ! net_set_(non)block() returned -0x%x\n\n", -ret);
		mbedtls_x509_crt_free(&(tls_data_params->Cacert));
		return NET_CONNECT_FAILED;
	} 
    NET_WORK_LOG("ok");	
	
	mbedtls_ssl_set_bio(&(tls_data_params->Ssl), &(tls_data_params->Server_Fd), mbedtls_net_send, NULL, mbedtls_net_recv_timeout);
	mbedtls_ssl_conf_read_timeout(&(tls_data_params->Config), pNetwork->Tcp_Connect_Params.Timeout_Ms);

	// Log_InfoDD("Setting up the SSL/TLS structure...");
	if((ret = mbedtls_ssl_config_defaults(&(tls_data_params->Config), MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM,
										  MBEDTLS_SSL_PRESET_DEFAULT)) != 0) 
    {
		NET_WORK_LOG("failed! mbedtls_ssl_config_defaults returned -0x%x\r\n", -ret);
		mbedtls_x509_crt_free(&(tls_data_params->Cacert));
		return NET_CONNECT_FAILED;
	}

	if(pNetwork->Tls_Connect_Params.Cert_Verify == true) 
    {
		mbedtls_ssl_conf_authmode(&(tls_data_params->Config), MBEDTLS_SSL_VERIFY_REQUIRED);
	} 
    else 
    {
		mbedtls_ssl_conf_authmode(&(tls_data_params->Config), MBEDTLS_SSL_VERIFY_OPTIONAL);
	}
	mbedtls_ssl_conf_rng(&(tls_data_params->Config), Mbedtls_Random_Port, NULL);

	mbedtls_ssl_conf_ca_chain(&(tls_data_params->Config), &(tls_data_params->Cacert), NULL);

	if ((pNetwork->Tls_Connect_Params.Client_Cert) && (pNetwork->Tls_Connect_Params.Client_Key)) 
    {
		if((ret = mbedtls_ssl_conf_own_cert(&(tls_data_params->Config), &(tls_data_params->Clicert), &(tls_data_params->Pkey))) !=0) 
        {
			NET_WORK_LOG(" failed! mbedtls_ssl_conf_own_cert returned %d\n\n", ret);
			mbedtls_x509_crt_free(&(tls_data_params->Cacert));
			return NET_CONNECT_FAILED;
		}
	}

	/* Assign the resulting configuration to the SSL context. */
	if((ret = mbedtls_ssl_setup(&(tls_data_params->Ssl), &(tls_data_params->Config))) != 0) {
		NET_WORK_LOG(" failed! mbedtls_ssl_setup returned -0x%x\n\n", -ret);
		mbedtls_x509_crt_free(&(tls_data_params->Cacert));
		return NET_CONNECT_FAILED;
	}

	if((ret = mbedtls_ssl_set_hostname(&(tls_data_params->Ssl), pNetwork->Tcp_Connect_Params.Host)) != 0) 
    {
		NET_WORK_LOG(" failed! mbedtls_ssl_set_hostname returned %d\n\n", ret);
		mbedtls_x509_crt_free(&(tls_data_params->Cacert));
		return NET_CONNECT_FAILED;
	}

	// Log_InfoDD("SSL state connect: %d ", tls_data_params->ssl.state);
	// Log_InfoDD("Performing the SSL/TLS handshake...");
	while((ret = mbedtls_ssl_handshake(&(tls_data_params->Ssl))) != 0) 
    {
		if(ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) 
        {
			NET_WORK_LOG("failed! mbedtls_ssl_handshake returned -0x%x\n", -ret);
			if(ret == MBEDTLS_ERR_X509_CERT_VERIFY_FAILED) {
				NET_WORK_LOG("    Unable to verify the server's certificate. "
							  "Either it is invalid,\n"
							  "    or you didn't set ca_file or ca_path "
							  "to an appropriate value.\n"
							  "    Alternatively, you may want to use "
							  "auth_mode=optional for testing purposes.\n");
			}
			mbedtls_x509_crt_free(&(tls_data_params->Cacert));
			return NET_CONNECT_FAILED;
		}
	}

	mbedtls_x509_crt_free(&(tls_data_params->Cacert));
	if((ret = mbedtls_ssl_get_record_expansion(&(tls_data_params->Ssl))) >= 0) 
    {
		NET_WORK_LOG("    [ Record expansion is %d ]\n", ret);
	} 
    else 
    {
		NET_WORK_LOG("    [ Record expansion is unknown (compression) ]\n");
	}

	return 0;
}


/**
*@名称 		Network_Tls_Disconnect
*@功能 		断开TCP网络安全连接
*@参数 		Network_Context_t *pNetwork
*@返回值 	int
*@使用说明	
*/
int Network_Tls_Disconnect(Network_Context_t *pNetwork) 
{
	int ret = 0;
	Tls_Context_t *tls_data_params = (Tls_Context_t*)(pNetwork->Tls_Context);

	do {
		ret = mbedtls_ssl_close_notify(&tls_data_params->Ssl);
	} while(ret == MBEDTLS_ERR_SSL_WANT_WRITE);

	/* All other negative return values indicate connection needs to be reset.
	 * No further action required since this is disconnect call */

	mbedtls_net_free(&(tls_data_params->Server_Fd));
	mbedtls_x509_crt_free(&(tls_data_params->Clicert));
	mbedtls_pk_free(&(tls_data_params->Pkey));
	mbedtls_ssl_free(&(tls_data_params->Ssl));
	mbedtls_ssl_config_free(&(tls_data_params->Config));

	return 0;
}

/**
*@名称 		Network_Tls_Destroy
*@功能 		销毁TCP网络安全连接
*@参数 		Network_Context_t *pNetwork
*@返回值 	int
*@使用说明	
*/
int Network_Tls_Destroy(Network_Context_t *pNetwork)
{
	Tls_Context_t *tls_data_params = (pNetwork->Tls_Context);

	mbedtls_free(tls_data_params);
	pNetwork->Tls_Context = NULL;

	return 0;
}

/**
*@名称 		Mbedtls_Status_Is_Ssl_In_Progress
*@功能 		网络安全连接正在操作过程中
*@参数 		int ret
*@返回值 	int
*@使用说明	
*/
static int Mbedtls_Status_Is_Ssl_In_Progress(int ret)
{
    return( ret == MBEDTLS_ERR_SSL_WANT_READ ||
            ret == MBEDTLS_ERR_SSL_WANT_WRITE ||
			ret == MBEDTLS_ERR_SSL_CRYPTO_IN_PROGRESS ||
			ret == MBEDTLS_ERR_SSL_TIMEOUT ||
            ret == MBEDTLS_ERR_SSL_ASYNC_IN_PROGRESS );
}

/**
*@名称 		Network_Tls_Write
*@功能 		网络安全连接写数据
*@参数 		Network_Context_t *pNetwork, const unsigned char *pMsg, size_t len
*@返回值 	int
*@使用说明	
*/
int Network_Tls_Write(Network_Context_t *pNetwork, const unsigned char *pMsg, size_t len)
{
	Tls_Context_t *tls_data_params = (Tls_Context_t*)(pNetwork->Tls_Context);
	int rv = mbedtls_ssl_write(&(tls_data_params->Ssl), pMsg, len);
    if (rv < 0) //出错
    {
        if (Mbedtls_Status_Is_Ssl_In_Progress(rv)) 
        {
            NET_WORK_LOG("==============Mbedtls_Status_Is_Ssl_In_Progress return 0===============\n");
            return 0;
        }
        NET_WORK_LOG("====network_tls_write===OPRT_MID_TLS_NET_SOCKET_ERROR=========\n");
        return NET_SOCKET_FAILED;
    }
    return rv;
}

/**
*@名称 		Network_Tls_Read
*@功能 		网络安全连接读数据
*@参数 		Network_Context_t *pNetwork,  unsigned char *pMsg, size_t len
*@返回值 	int
*@使用说明	
*/
int Network_Tls_Read(Network_Context_t *pNetwork, unsigned char *pMsg, size_t len)
{
	Tls_Context_t *tls_data_params = (Tls_Context_t*)(pNetwork->Tls_Context);
	int rv = mbedtls_ssl_read(&(tls_data_params->Ssl), pMsg, len);
    if (rv < 0) //出错
    {
        if (Mbedtls_Status_Is_Ssl_In_Progress(rv)) 
        {
            return 0;
        }
        return NET_SOCKET_FAILED;
    }
    return rv;
}

/**
*@名称 		Network_Tls_Init
*@功能 		网络安全连接初始化
*@参数 		Network_Context_t *pNetwork, const Tcp_Connect_Params_t *tcp_params, const Tls_Connect_Params_t *tls_params
*@返回值 	int
*@使用说明	
*/
int Network_Tls_Init(Network_Context_t *pNetwork, const Tcp_Connect_Params_t *tcp_params, const Tls_Connect_Params_t *tls_params)
{
	if (NULL == pNetwork) {
		return NET_INVALID_PARM;
	}

	pNetwork->Connect = Network_Tls_Connect;
	pNetwork->Read = Network_Tls_Read;
	pNetwork->Write = Network_Tls_Write;
	pNetwork->Disconnect = Network_Tls_Disconnect;
	pNetwork->Destroy = Network_Tls_Destroy;
    pNetwork->Tcp_Connect_Params = *tcp_params;
	pNetwork->Tls_Connect_Params = *tls_params;

	Tls_Context_t* tls_ctx = mbedtls_calloc(1, sizeof(Tls_Context_t));
	if(NULL == tls_ctx) {
		NET_WORK_LOG("tls_ctx malloc fail\r\n");
		return NET_INVALID_PARM;
	}

	tls_ctx->Flags = 0;
	pNetwork->Tls_Context = tls_ctx;
	return 0;
}


//----------------------------------------提供给HTTP的接口---------------------------------------------//

/**
*@名称 		Bsp_Tcp_Connect
*@功能 		TCP建立连接
*@参数 		const char *host, uint16_t port
*@返回值 	uint32_t  socket 成功会>0 
*@使用说明  
*/
uint32_t Bsp_Tcp_Connect(const char *host, uint16_t port)
{
    Log_Info("%s host:%s port:%d\r\n", __func__, host, port);
    struct addrinfo hints;
	struct addrinfo *addrInfoList = NULL;
	struct addrinfo *cur = NULL;
    int fd = 0;
	int rc = 0;
	char service[6]={0};
	const int max_retries = 3;  // 最大重试次数
    int retry_count = 0;

	if (host == NULL || port == 0) {
        Log_Error("Invalid parameters\r\n");
        return 0;
    }

    memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET; //only IPv4
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
    sprintf(service, "%d", port);
	for (retry_count = 0; retry_count < max_retries; retry_count++)
	{
        addrInfoList = NULL;
        rc = getaddrinfo(host, service, &hints, &addrInfoList);
        if (rc != 0)
        {
            Log_Error("getaddrinfo 解析 %s:%s 失败，rc=%d，msg=%s，重试 %d/%d\r\n",
                      host,
                      service,
                      rc,
                      bsp_gai_strerror_safe(rc),
                      retry_count + 1,
                      max_retries);
            if (retry_count < max_retries - 1)
            {
                Bsp_Sleep_Ms(1000);
                continue;
            }
            return 0;
        }

		for (cur = addrInfoList; cur != NULL; cur = cur->ai_next) 
		{
			if (cur->ai_family != AF_INET) 
			{
				Log_Error("socket type error\r\n");
				continue;
			}

			fd = socket(cur->ai_family, cur->ai_socktype, cur->ai_protocol);
			if (fd < 0) 
			{
				Log_Error("create socket error: %d\r\n", errno);
				continue;
			}
			
			int enable = 1;
			if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0) {
                Log_Warn("setsockopt SO_REUSEADDR failed: %s\r\n", strerror(errno));
            }
			#if 0
			// 2. 设置TCP保活参数 
            enable = 1;
            if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &enable, sizeof(enable)) < 0) {
                Log_Warn("setsockopt SO_KEEPALIVE failed: %s\r\n", strerror(errno));
            }
			
            /* TCP keepalive 在 MQTT keepalive(25s) 之前先探測，雙重保障。
             * KEEPIDLE=15s：最後一次 TCP 收發後 15s 開始探測（< MQTT keepalive 25s）
             * KEEPINTVL=3s, KEEPCNT=3：3×3=9s 後仍無回應則關 socket
             * → 死連線最遲 15+9=24s 被 TCP 層偵測，早於 MQTT 層的 25+8=33s */
            int keepalive_idle = 15;      // 15秒后开始发送保活探测（原30s）
            int keepalive_interval = 3;   // 探测包间隔3秒（原5s）
            int keepalive_count = 3;      // 最多发送3次探测
			if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &keepalive_idle, sizeof(keepalive_idle)) < 0) {
                Log_Warn("setsockopt TCP_KEEPIDLE failed: %s\r\n", strerror(errno));
            }
            if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &keepalive_interval, sizeof(keepalive_interval)) < 0) {
                Log_Warn("setsockopt TCP_KEEPINTVL failed: %s\r\n", strerror(errno));
            }
            if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &keepalive_count, sizeof(keepalive_count)) < 0) {
                Log_Warn("setsockopt TCP_KEEPCNT failed: %s\r\n", strerror(errno));
            }
			
			// 3. 设置TCP_NODELAY禁用Nagle算法
            enable = 1;
            if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(enable)) < 0) {
                Log_Warn("setsockopt TCP_NODELAY failed: %s\r\n", strerror(errno));
            }
			// 4. 设置连接超时
            struct timeval timeout;
            timeout.tv_sec = 10;   // 10秒连接超时
            timeout.tv_usec = 0;
            if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
                Log_Warn("setsockopt SO_SNDTIMEO failed: %s\r\n", strerror(errno));
            }
            
            // 5. 设置接收超时
            timeout.tv_sec = 30;   // 30秒接收超时
            if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
                Log_Warn("setsockopt SO_RCVTIMEO failed: %s\r\n", strerror(errno));
            }
			#endif
			if (connect(fd, cur->ai_addr, cur->ai_addrlen) == 0) 
			{
				Log_Info("TCP connected successfully, fd=%d\r\n", fd);
                freeaddrinfo(addrInfoList);
                return (uint32_t)fd;
			}
			close(fd);
            Log_Warn("TCP 连接失败：errno=%d，重试 %d/%d\r\n", errno, retry_count + 1, max_retries);
		}
        freeaddrinfo(addrInfoList);
        addrInfoList = NULL;
		if (retry_count < max_retries - 1) {
            Bsp_Sleep_Ms(1000); // 等待1秒后重试
        }
	}
    Log_Error("TCP 建连失败，已重试 %d 次\r\n", max_retries);
    return 0;
}


/**
*@名称 		Bsp_Tcp_Disconnect
*@功能 		TCP断开连接
*@参数 		uint32_t fd
*@返回值 	int
*@使用说明  
*/
int Bsp_Tcp_Disconnect(uint32_t fd)
{
    int rc = 0;
#if 0
	//Shutdown both send and receive operations.
	rc = shutdown((int) fd, 2);
	if (0 != rc) 
    {
		Log_Error("shutdown error\r\n");
	}
#endif
	rc = close((int) fd);
	if (0 != rc) 
    {
		Log_Error("closesocket error\r\n");
	}
	return rc;
}

/**
*@名称 		Bsp_Tcp_Write
*@功能 		TCP发送数据
*@参数 		int fd, const unsigned char *buf, uint32_t len, uint32_t timeout_ms
*@返回值 	int
*@使用说明  
*/
int Bsp_Tcp_Write(uint32_t fd, const char *buf, uint32_t len, uint32_t timeout_ms)
{
	int ret;
	uint32_t len_sent=0;
	uint64_t t_end, t_left;
	fd_set sets;
	struct timeval timeout;
	if (buf == NULL || len == 0) 
	{
        Log_Error("Invalid parameters: buf=%p, len=%u\r\n", buf, len);
        return -2;
    }

	t_end = Bsp_Get_Run_Time_Ms() + timeout_ms;
	do {
		t_left = _Time_Left(t_end, Bsp_Get_Run_Time_Ms());
		if (t_left == 0) 
		{
            Log_Error("fd:%lu write timeout, sent:%u/%u\r\n", fd, len_sent, len);
            return (len_sent > 0) ? len_sent : -1;
        }
		
		FD_ZERO(&sets);
		FD_SET(fd, &sets);
		timeout.tv_sec = t_left / 1000;
		timeout.tv_usec = (t_left % 1000) * 1000;
		ret = select(fd + 1, NULL, &sets, NULL, &timeout);
		if (ret > 0) //成功获取到信号 
		{
			if (FD_ISSET(fd, &sets)) 
			{
				ret = send(fd, buf + len_sent, len - len_sent, 0);
				if (ret > 0)
				{
					len_sent += ret;
				}	
				else if (0 == ret)
				{
					Log_Error("fd:%lu connection closed during send\r\n", fd);
					return (len_sent > 0) ? len_sent : -2;
				}	
				else 
				{
					if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) 
					{
						//Log_Debug("send would block, continue...\r\n");
						continue;
					}
					Log_Error("fd:%lu send failed, errno:%d, sent:%u/%u\r\n", fd, errno, len_sent, len);
					return (len_sent > 0) ? len_sent : -2;
				}
			}
		} 
		else if (ret == 0) 
		{
            Log_Error("fd:%lu select write timeout, t_left:%llu\r\n", fd, t_left);
            return (len_sent > 0) ? len_sent : -1;
        } 
		else 
		{
            if (errno == EINTR) 
			{
                //Log_Debug("EINTR in select, continue...\r\n");
                continue;
            }
            Log_Error("fd:%lu select write failed, errno:%d\r\n", fd, errno);
            return (len_sent > 0) ? len_sent : -2;
        }
	} while (len_sent < len);
	//Log_Debug("fd:%lu write completed, total:%u bytes\r\n", fd, len_sent);
    return len_sent;
}


/**
*@名称 		Bsp_Tcp_Read
*@功能 		TCP接收数据
*@参数 		unsigned int fd, char *buf, uint32_t len, uint32_t timeout_ms
*@返回值 	int
*@使用说明  
*/
int Bsp_Tcp_Read(unsigned int fd, char *buf, uint32_t len, uint32_t timeout_ms)
{
	int ret;
	uint32_t len_recv;
	uint32_t t_end, t_left;
	fd_set sets;
	struct timeval timeout;

	if (buf == NULL || len == 0) {
        Log_Error("Invalid parameters: buf=%p, len=%u\r\n", buf, len);
        return -4;
    }

	t_end = Bsp_Get_Run_Time_Ms() + timeout_ms ;
	len_recv = 0;

	do {
		t_left = _Time_Left(t_end, Bsp_Get_Run_Time_Ms());
		if (0 == t_left) 
        {
			// timeout
			Log_Error("fd:%d read timeout\r\n", fd);
			return (len_recv > 0) ? len_recv : -3;
		}

		FD_ZERO(&sets);
		FD_SET(fd, &sets);
		timeout.tv_sec = t_left / 1000;
		timeout.tv_usec = (t_left % 1000) * 1000;
		ret = select(fd + 1, &sets, NULL, NULL, &timeout);
		if (ret > 0) 
		{
			ret = recv(fd, buf+len_recv, len-len_recv, 0);
			if (ret > 0) 
			{
				len_recv += ret;
				//Log_Debug("fd:%d received %d bytes, total:%u/%u\r\n", fd, ret, len_recv, len);
			} 
			else if (0 == ret) 
			{
				Log_Error("fd:%d connection closed, received:%u/%u\r\n", fd, len_recv, len);
                return (len_recv > 0) ? len_recv : -1;
			} 
			else 
			{
				if (EINTR == errno) 
				{
					Log_Error("EINTR be caught\r\n");
					continue;
				}
				//Log_Error("fd:%d recv failed, errno:%d, received:%u/%u\r\n", fd, errno, len_recv, len);
                return (len_recv > 0) ? len_recv : -2;
			}
		} 
		else if (0 == ret)
		{
			//Log_Error("fd:%d select timeout, t_left:%u, received:%u/%u\r\n", fd, t_left, len_recv, len);
            return (len_recv > 0) ? len_recv : 0;//select超时，确实没数据可读，不应返回错误
		}
		else 
		{
			if (EINTR == errno) 
			{
				Log_Error("EINTR be caught-------\r\n");
				continue;
			}
			//Log_Error("fd:%d select failed, errno:%d, received:%u/%u\r\n", fd, errno, len_recv, len);
            return (len_recv > 0) ? len_recv : -2;
		}
	} while (len_recv < len);
	
	//Log_Debug("fd:%d read completed, total:%u bytes\r\n", fd, len_recv);
    return len_recv;
}

/**
*@名称 		Bsp_Tcp_Set_Options
*@功能 		设置TCP socket选项
*@参数 		uint32_t fd, int keepalive, int timeout_ms
*@返回值 	int
*/
int Bsp_Tcp_Set_Options(uint32_t fd, int keepalive, int timeout_ms)
{
    int ret = 0;
    
    if (keepalive) {
        int enable = 1;
        ret = setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &enable, sizeof(enable));
        if (ret != 0) {
            Log_Warn("Set keepalive failed: %d\r\n", errno);
        }
    }
    
    if (timeout_ms > 0) {
        struct timeval timeout;
        timeout.tv_sec = timeout_ms / 1000;
        timeout.tv_usec = (timeout_ms % 1000) * 1000;
        
        ret = setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        if (ret != 0) {
            Log_Warn("Set recv timeout failed: %d\r\n", errno);
        }
        
        ret = setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
        if (ret != 0) {
            Log_Warn("Set send timeout failed: %d\r\n", errno);
        }
    }
    
    return ret;
}

/**
*@名称 		Bsp_Tcp_Get_Status
*@功能 		检查TCP连接状态
*@参数 		uint32_t fd
*@返回值 	int 0=正常, -1=错误
*/
int Bsp_Tcp_Get_Status(uint32_t fd)
{
    int error = 0;
    socklen_t len = sizeof(error);
    
    int ret = getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len);
    if (ret != 0) {
        return -1;
    }
    
    if (error != 0) {
        Log_Debug("Socket error: %d\r\n", error);
        return -1;
    }
    
    return 0;
}


/**
*@名称 		Bsp_Udp_Broadcast_Init
*@功能 		UDP广播初始化
*@参数 		int boardcast_addr, unsigned short boardcast_port, void **board_socket_addr
*@返回值 	int
*@使用说明	
*/
int Bsp_Udp_Broadcast_Init(int boardcast_addr, unsigned short boardcast_port, void **board_socket_addr)
{
    //申请UDP套接字
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);//UDP
    if(sockfd == -1)
    {
        Log_Error("%s Failed to create socket:%s\r\n", __func__, strerror(errno));
        goto exit;
    }
    Log_Info("%s Socket_Id:%d, Boardcast_Addr:%x, Boardcast_Port:%d\r\n", __func__, sockfd, boardcast_addr, boardcast_port);
    
    //开启UDP广播功能
    int broadcast = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) == -1)
    {
        Log_Error("%s setsockopt (SO_BROADCAST):%s\r\n", __func__, strerror(errno));
        goto exit;
    }
    //设置广播地址端口
    struct sockaddr_in *broadcast_addr = (struct sockaddr_in *)Bsp_Psram_Malloc(sizeof(struct sockaddr_in));
    if(broadcast_addr == NULL)
    {
        Log_Error("%s malloc faild\r\n", __func__);
        goto exit;
    }
    memset(broadcast_addr, 0, sizeof(struct sockaddr_in));
    broadcast_addr->sin_family = AF_INET;
    broadcast_addr->sin_port = htons(boardcast_port);
    broadcast_addr->sin_addr.s_addr = boardcast_addr;
    *((struct sockaddr_in**)board_socket_addr) = broadcast_addr;
    return sockfd;
exit:
    if(sockfd >= 0) 
    {
        close(sockfd);
    }
    return 0;
}

/**
*@名称 		Bsp_Udp_Broadcast_Send_Bytes
*@功能 		UDP广播发送数据
*@参数 		int sockfd, char *pdata, unsigned int len, void *board_socket_addr
*@返回值 	int
*@使用说明	
*/
int Bsp_Udp_Broadcast_Send_Bytes(int sockfd, char *pdata, unsigned int len, void *board_socket_addr)
{
    struct sockaddr_in *broadcast_addr = (struct sockaddr_in *)board_socket_addr;
    ssize_t send_len = sendto(sockfd, pdata, len, 0, (struct sockaddr *)broadcast_addr, sizeof(struct sockaddr_in));
    if(send_len == -1)
    {
        Log_Error("%s sockfd:%d Failed to send broadcast message:%s\r\n", __func__, sockfd, strerror(errno));
        return -1;
    }
    return 0;
}

/**
*@名称 		Bsp_Udp_Server_Init
*@功能 		UDP服务端初始化
*@参数 		unsigned short server_port, void **server_socket_addr
*@返回值 	int
*@使用说明	
*/
int Bsp_Udp_Server_Init(unsigned short server_port, void **server_socket_addr)
{
    //申请UDP套接字
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);//UDP
    if(sockfd == -1)
    {
        Log_Error("%s Failed to create socket:%s\r\n", __func__, strerror(errno));
        goto exit;
    }
    Log_Info("%s sockfd:%d,Server_Port:%d \r\n", __func__, sockfd, server_port);
    
    //设置广播地址端口
    struct sockaddr_in *server_addr = (struct sockaddr_in *)Bsp_Psram_Malloc(sizeof(struct sockaddr_in));
    if(server_addr == NULL)
    {
        Log_Error("%s malloc faild\r\n", __func__);
        goto exit;
    }
    memset(server_addr, 0, sizeof(struct sockaddr_in));
    server_addr->sin_family = AF_INET;
    server_addr->sin_port = htons(server_port);
    server_addr->sin_addr.s_addr = INADDR_ANY;
    *((struct sockaddr_in**)server_socket_addr) = server_addr;

    //允许在套接字关闭后立即重用相同的地址和端口
    int optval = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) 
    {
        Log_Error("%s sockfd:%d Failed to set SO_REUSEADDR:%s\r\n", __func__, sockfd, strerror(errno));
        goto exit;
    }
    //绑定
    if (bind(sockfd, (struct sockaddr *)server_addr, sizeof(struct sockaddr_in)) == -1)
    {
        Log_Error("%s Bind error:%s", __func__, strerror(errno));
        goto exit;
    }
    Log_Info("%s bind success \r\n", __func__);
    return sockfd;

exit:
    if(sockfd >= 0) 
    {
        close(sockfd);
    }
    return 0;
}

/**
*@名称 		Bsp_Udp_Server_Recv_Bytes
*@功能 		UDP服务接收数据
*@参数 		int sockfd, char *pdata, unsigned int len, void *client_socket_addr
*@返回值 	int
*@使用说明	
*/
int Bsp_Udp_Server_Recv_Bytes(int sockfd, char *pdata, unsigned int *len, void *client_socket_addr)
{
    size_t recv_max = *len;
    char *recv_buf = pdata;
    struct sockaddr_in *client_addr = (struct sockaddr_in *)client_socket_addr;
    socklen_t addrlen=sizeof(struct sockaddr_in);
    struct timeval tv;
    fd_set readfds;

    FD_ZERO(&readfds);
    FD_SET(sockfd, &readfds);

    tv.tv_sec = 1;
    tv.tv_usec = 0;
    int retval = select(sockfd + 1, &readfds, NULL, NULL, &tv);
    if (retval == -1)
    {
        Log_Error("select() error:%s\r\n", strerror(errno));
        goto exit;
    }
    else if (retval)
    {
        if(FD_ISSET(sockfd, &readfds))
        {
            memset(recv_buf, 0, recv_max);
            ssize_t recv_len = recvfrom(sockfd, recv_buf, recv_max, 0, (struct sockaddr *)client_addr, &addrlen);
            if (recv_len > 0)
            {
                *len = recv_len;
                recv_buf[recv_len] = '\0';
                char addr_str[32] = {0};
                inet_ntoa_r(client_addr->sin_addr, addr_str, sizeof(addr_str) - 1);
                unsigned short client_port = ntohs(client_addr->sin_port);
                Log_Info("%s recv msg from ip: %s, port: %d, len:%d\r\n", __func__, addr_str, client_port, *len);
                Log_Info("recv msg data:%s\r\n", recv_buf);
                return 0;
            }
        }
    }
    else //超时
    {
       
    }
exit:
    *len = 0;
    return -1;
}

/**
*@名称 		Bsp_Udp_Server_Send_Bytes
*@功能 		UDP服务端发送数据
*@参数 		int sockfd, char *pdata, unsigned int len, void *client_socket_addr
*@返回值 	int
*@使用说明	
*/
int Bsp_Udp_Server_Send_Bytes(int sockfd, char *pdata, unsigned int len, void *client_socket_addr)
{
    struct sockaddr_in *client_addr = (struct sockaddr_in *)client_socket_addr;
    ssize_t send_len = sendto(sockfd, pdata, len, 0, (struct sockaddr *)client_addr, sizeof(struct sockaddr_in));
    if(send_len == -1)
    {
        Log_Error("%s sockfd:%d Failed to send broadcast message:%s\r\n", __func__, sockfd, strerror(errno));
        return -1;
    }
    return 0;
}

/**
*@名称 		Bsp_Network_Socket_Close
*@功能 		关闭网络套接字
*@参数 		int sockfd
*@返回值 	int
*@使用说明	
*/
int Bsp_Network_Socket_Close(int sockfd)
{
    if(sockfd > 0)
    {
        close(sockfd);
    }
    return 0;
}












