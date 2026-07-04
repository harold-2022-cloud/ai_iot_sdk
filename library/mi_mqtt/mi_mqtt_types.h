// mi_mqtt_types.h - platform-neutral MQTT transport parameter types
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// ⚠ Tcp/Tls_Connect_Params_t 與 platform_os/bsp_network.h 共用同一組 guard 巨集
//   (ENTITY_TCP/TLS_CONNECT_PARAMS_T_DEFINED)，欄位必須逐一保持一致：先 include 者勝，
//   後者被 #ifndef 跳過，欄位若分歧會被 guard 靜默吞掉、佈局取決於 include 順序。改此處務必同步另一檔。
#ifndef ENTITY_TCP_CONNECT_PARAMS_T_DEFINED
#define ENTITY_TCP_CONNECT_PARAMS_T_DEFINED
typedef struct
{
    const char *Host;
    uint16_t Port;
    uint32_t Timeout_Ms;
} Tcp_Connect_Params_t;
#endif

#ifndef ENTITY_TLS_CONNECT_PARAMS_T_DEFINED
#define ENTITY_TLS_CONNECT_PARAMS_T_DEFINED
typedef struct
{
    const uint8_t *Cacert;
    size_t Cacert_Len;
    const uint8_t *Client_Cert;
    size_t Client_Cert_Len;
    const uint8_t *Client_Key;
    size_t Client_Key_Len;
    bool Cert_Verify;
} Tls_Connect_Params_t;
#endif
