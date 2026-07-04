//entity_wifi.h
#pragma once


#include <inttypes.h>

#define ENTITY_WIFI_SSID_LEN_MAX       32
#define ENTITY_WIFI_KEY_LEN_MAX        64
#define ENTITY_WIFI_MAC_LEN_MAX        6
#define ENTITY_WIFI_SCAN_AP_NUM_MAX    32

typedef enum {
    ENTITY_WIFI_SECURITY_OPEN = 0,                  /*!< OPEN. */
    ENTITY_SECURITY_WEP,                       /*!< WEP. */
    ENTITY_SECURITY_WPA2PSK,                   /*!< WPA2-PSK. */
    ENTITY_SECURITY_WPAPSK_WPA2PSK_MIX,        /*!< WPA/WPA2-PSK MIX. */
    ENTITY_SECURITY_WPAPSK,                    /*!< WPA-PSK. */
    ENTITY_SECURITY_WPA,                       /*!< WPA. */
    ENTITY_SECURITY_WPA2,                      /*!< WPA2. */
    ENTITY_SECURITY_SAE,                       /*!< SAE. */
    ENTITY_SECURITY_WPA3_WPA2_PSK_MIX,         /*!< WPA3/WPA2-PSK MIX. */
    ENTITY_SECURITY_AUTO,
    ENTITY_SECURITY_UNKNOWN                    /*!< UNKNOWN. */
}Entity_Wifi_Auth_Mode_e;

typedef enum {
    ENTITY_WIFI_EVT_UNKNOWN,             /*!< UNKNWON. */
    ENTITY_WIFI_EVT_SCAN_DONE,           /*!< Scan finish. */
    ENTITY_WIFI_EVT_CONNECTED,           /*!< Connected. */
    ENTITY_WIFI_EVT_DISCONNECTED,        /*!< Disconnected. */
    ENTITY_WIFI_EVT_WPS_TIMEOUT,         /*!< WPS timeout. */
    ENTITY_WIFI_EVT_MESH_CONNECTED,      /*!< MESH connected. */
    ENTITY_WIFI_EVT_MESH_DISCONNECTED,   /*!< MESH disconnected. */
    ENTITY_WIFI_EVT_AP_START,            /*!< AP start. CNcomment: */
    ENTITY_WIFI_EVT_STA_CONNECTED,       /*!< STA connected with ap. */
    ENTITY_WIFI_EVT_STA_DISCONNECTED,    /*!< STA disconnected from ap. */
    ENTITY_WIFI_EVT_STA_FCON_NO_NETWORK, /*!< STA connect, but can't find network. */
    ENTITY_WIFI_EVT_MESH_CANNOT_FOUND,   /*!< MESH can't find network. */
    ENTITY_WIFI_EVT_MESH_SCAN_DONE,      /*!< MESH AP scan finish. */
    ENTITY_WIFI_EVT_MESH_STA_SCAN_DONE,  /*!< MESH STA scan finish. */
    ENTITY_WIFI_EVT_AP_SCAN_DONE,        /*!< AP scan finish. */
    ENTITY_WIFI_EVT_IP_CHANGE,           /*!< IP change */
    ENTITY_WIFI_EVT_BUTT
}Entity_Wifi_Event_Type_e;

typedef enum
{
    ENTITY_WIFI_STA_MODE,
    ENTITY_WIFI_AP_MODE,
}Entity_Wifi_Work_Mode_e;

typedef struct {
    char   Ssid[ENTITY_WIFI_SSID_LEN_MAX + 1];        /*!< SSID. */
    uint8_t Bssid[ENTITY_WIFI_MAC_LEN_MAX];               /*!< BSSID. */
    uint32_t Channel;                              /*!< Channel number. */
    Entity_Wifi_Auth_Mode_e Auth;                                   /*!< Authentication type. */
    int8_t Rssi;                                   /*!< Signal Strength. */
    uint8_t Wps_Flag : 1;                          /*!< WPS flag. */
    uint8_t Wps_Session : 1;                       /*!< WPS session:PBC-0/PIN-1. */
    uint8_t Wmm : 1;                               /*!< WMM flag. */
}Entity_Wifi_Ap_Info_t;

typedef struct 
{
    uint32_t Num;
    Entity_Wifi_Ap_Info_t Ap_Infos[ENTITY_WIFI_SCAN_AP_NUM_MAX];
}Entity_Wifi_Sta_Scan_Result_t;



typedef struct 
{
    void (*Wifi_Init)(uint8_t mode);
    int (*Wifi_Sta_Mode_Config)(char *ssid, char *passwd, uint8_t auth_mode);
    int (*Wifi_Sta_Scan_Start)(void* callback);
    void*(*Wifi_Get_Scan_Results)(void);
    int (*Wifi_Copy_Scan_Results)(void *result, unsigned int len);
    void (*Wifi_Get_Macaddr)(uint8_t *mac_addr);
    int (*Wifi_Sta_Conncet)(void);
    int (*Wifi_Sta_Disconnect)(void);
    void (*Wifi_Sta_Auto_Reconnect_Enable)(uint8_t enable);
    void (*Register_Wifi_Event_App_Cb)(void *cb);
    void (*Print_Scan_Result)(void *res);
    int (*Wifi_Ap_Start)(void);
    int (*Wifi_Ap_Stop)(void);
    int (*Wifi_Ap_Mode_Config)(char *ap_ssid, char *ap_key, const char *local_ip, const char *gw_ip, const char * ip_mask);
    int (*Wifi_Load_Signal_Level_Quality)(uint8_t *level, uint8_t *quality);
}Entity_Wifi_Cbs_t;


//WIFI接口函数初始化
void Entity_Wifi_Cbs_Init(Entity_Wifi_Cbs_t *cbs);

//获取WIFI接口函数
Entity_Wifi_Cbs_t *Get_Entity_Wifi_Cbs(void);

//返回WIFI扫描结果AP的数量
unsigned char Entity_Wifi_Get_Scan_Result_Num(void);

//复制WIFI扫描结果快照
int Entity_Wifi_Copy_Scan_Results(Entity_Wifi_Sta_Scan_Result_t *result);

//WIFI初始化
void Entity_Wifi_Init(uint8_t mode);

//初始化网络信息
void Entity_Wifi_Config_Net_Info(char *ssid, char *passwd);

//获取WIFI MAC
int Entity_Get_Wifi_Mac(uint8_t *mac_addr);

//WIFI连接热点,指定密码类型为BSP_SECURITY_WPA2PSK
int Entity_Wifi_Sta_Conncet(void);

//sta断开连接
int Entity_Wifi_Sta_Disconnect(void);

//sta自动重连
void Entity_Wifi_Sta_Auto_Reconnect_Enable(uint8_t enable);

//WIFI扫描
int Entity_Wifi_Sta_Scan_Start(void* callback);

//打印WIFI扫描结果
void Entity_Print_Scan_Result(void *res);

//WiFi扫描缓存：启动预缓存扫描
void Entity_Wifi_Scan_Cache_Start(void);

//WiFi扫描缓存：查询缓存是否就绪
uint8_t Entity_Wifi_Is_Scan_Cache_Ready(void);

//WiFi扫描缓存：清除缓存标志
void Entity_Wifi_Clear_Scan_Cache(void);

//启动AP模式
int Entity_Wifi_Ap_Start(void);

//停止AP模式
int Entity_Wifi_Ap_Stop(void);

//设置AP模式的参数
int Entity_Wifi_Ap_Mode_Config(char *ap_ssid, char *ap_key, const char *local_ip, const char *gw_ip, const char * ip_mask);

//获取当前所连WIFI热点信号水平及质量百分比
int Entity_Wifi_Load_Signal_Level_Quality(uint8_t *level, uint8_t *quality);

//WIFI成功获取IP标志
unsigned char Entity_Wifi_Get_Ip_Success(void);
