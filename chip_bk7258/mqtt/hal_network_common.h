#ifndef HAL_NETWORK_COMMON_H
#define HAL_NETWORK_COMMON_H

/* armino build shim：vendored coreMQTT 引擎（mi_mqtt.h / serializer.h）原樣保留
 * rino 的 #include "hal_network_common.h"。entity SDK 的等價傳輸層在
 * platform_os/bsp_network.h —— 兩者 TransportInterface / Network_Context_t /
 * Transport_Send_f / Transport_Recv_f / Tcp|Tls_Context_t / Tcp|Tls_Connect_Params_t
 * 同源相容（entity 即衍生自同一份 coreMQTT）。此 shim 轉發即可，引擎不需改動。 */

#include "bsp_network.h"

#endif /* HAL_NETWORK_COMMON_H */
