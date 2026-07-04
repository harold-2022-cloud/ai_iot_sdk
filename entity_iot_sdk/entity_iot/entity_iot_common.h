//entity_iot_common.h
#pragma once


/* IoT C-SDK APPID */
#define QCLOUD_IOT_DEVICE_SDK_APPID     "21010406"
#define QCLOUD_IOT_DEVICE_SDK_APPID_LEN (sizeof(QCLOUD_IOT_DEVICE_SDK_APPID) - 1)

/* MQTT server domain */
#define QCLOUD_IOT_MQTT_DIRECT_DOMAIN  "iotcloud.tencentdevices.com"
#define QCLOUD_IOT_MQTT_US_EAST_DOMAIN "us-east.iotcloud.tencentdevices.com"

#define MQTT_SERVER_PORT_TLS   8883
#define MQTT_SERVER_PORT_NOTLS 1883

/* CoAP server domain */
#define QCLOUD_IOT_COAP_DEIRECT_DOMAIN "iotcloud.tencentdevices.com"

#define COAP_SERVER_PORT 5684

/* server domain for dynamic registering device */
#define DYN_REG_SERVER_URL         "gateway.tencentdevices.com"
#define DYN_REG_SERVER_US_EAST_URL "us-east.gateway.tencentdevices.com"

#define DYN_REG_SERVER_PORT     80
#define DYN_REG_SERVER_PORT_TLS 443

/* URL for doing log upload */
#define LOG_UPLOAD_SERVER_URL            "http://devicelog.iot.cloud.tencent.com/cgi-bin/report-log"
#define LOG_UPLOAD_SERVER_DOMAIN         "http://devicelog.iot.cloud.tencent.com/cgi-bin/report-log"
#define LOG_UPLOAD_SERVER_US_EAST_DOMAIN "http://us-east.devicelog.iot.cloud.tencent.com/cgi-bin/report-log"
#define LOG_UPLOAD_SERVER_PATTEN         "devicelog.iot.cloud.tencent.com"

#define LOG_UPLOAD_SERVER_PORT 80

/* Max size of a host name */
#define HOST_STR_LENGTH 64

/* Max size of base64 encoded PSK = 64, after decode: 64/4*3 = 48*/
#define DECODE_PSK_LENGTH 48

