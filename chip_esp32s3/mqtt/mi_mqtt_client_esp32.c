/* mi_mqtt_client.c — esp-mqtt 替換實作
 * 對外介面與舊 coreMQTT 版本完全相同；
 * 內部使用 IDF esp_mqtt_client，並由 MQTT/TCP keepalive 偵測死線。
 *
 * 設計決策：
 *   1. disable_auto_reconnect=true：由上層 entity_mqtt_client.c 狀態機管理重連
 *   2. Mqtt_Client_Connect()  同步：等 MQTT_EVENT_CONNECTED（最多 10s）
 *   3. Mqtt_Client_Disconnect() 同步：等 MQTT_EVENT_DISCONNECTED（最多 5s）
 *   4. Mqtt_Client_Yield()   = sleep 100ms + 回傳連線狀態
 *   5. 事件 handler 路由至舊版 On_* callbacks，行為不變
 */
#include "mi_mqtt_client.h"

#include "mqtt_client.h"            /* esp-mqtt */
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "ai_dialog_diag.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <string.h>
#include <inttypes.h>
#include <stdbool.h>

#define MI_MQTT_TAG "MI_MQTT"

static bool mqtt_diag_field_contains(const char *data, int len, const char *needle)
{
    size_t needle_len;

    if (!data || len <= 0 || !needle)
    {
        return false;
    }

    needle_len = strlen(needle);
    if (needle_len == 0U || (size_t)len < needle_len)
    {
        return false;
    }

    for (int i = 0; i <= len - (int)needle_len; i++)
    {
        if (memcmp(data + i, needle, needle_len) == 0)
        {
            return true;
        }
    }

    return false;
}

/* esp-mqtt 內部 task 優先級 */
#define ESP_MQTT_TASK_PRIO      7
/* task stack 縮小至 4096：
 * 省 4096 bytes DRAM（從 8192 降，不需 TLS，回調鏈深度約 8 層 × 150B ≈ 1.2KB，安全） */
#define ESP_MQTT_TASK_STACK     4096
/* 連線等待超時（ms）*/
#define CONNECT_WAIT_MS         10000
/* 斷線等待超時（ms）*/
#define DISCONNECT_WAIT_MS      5000
/* 收發 buffer 設為 4096，避免 IDF 5.5.4 下較長 MQTT frame 讀取/解析被 1024 buffer 限制。 */
#define ESP_MQTT_BUF_SIZE       4096
/* TCP 建連/收發操作超時；MQTT keepalive 使用 esp-mqtt 內建 PINGREQ/PINGRESP。 */
#define ESP_MQTT_NETWORK_TIMEOUT_MS  30000
#define ESP_MQTT_RETRANSMIT_TIMEOUT_MS      10000
#define ESP_MQTT_TCP_KEEPALIVE_IDLE_SEC      30
#define ESP_MQTT_TCP_KEEPALIVE_INTERVAL_SEC  10
#define ESP_MQTT_TCP_KEEPALIVE_COUNT         3
#define MQTT_PUBACK_WARN_MS                  5000
#define MQTT_PUBACK_HARD_TIMEOUT_MS          60000
#define MQTT_RX_IDLE_TIMEOUT_MS              45000
#define MQTT_PUBACK_TRACK_MAX                16
#define MQTT_EVENT_CALLBACK_WARN_MS          100

#define STRING_PTR_PRINT_SANITY_CHECK(ptr) ((ptr) ? (ptr) : "null")

typedef struct
{
    uint16_t msg_id;
    uint32_t sent_ms;
    uint16_t payload_len;
    uint8_t qos;
    uint8_t warn_level;
} Mqtt_Puback_Track_t;

/* ── 内部 context 結構 ─────────────────────────────────────────────────── */
struct Mqtt_Client_Context
{
    esp_mqtt_client_handle_t handle;
    Mqtt_Client_Config_t     config;
    volatile bool            connected;
    bool                     started;       /* esp_mqtt_client_start() 已呼叫過 */
    SemaphoreHandle_t        conn_sem;      /* 等待 CONNECTED 事件 */
    SemaphoreHandle_t        disc_sem;      /* 等待 DISCONNECTED 事件 */
    uint32_t                 connect_seq;   /* 診斷用：每次 Connect 遞增 */
    uint32_t                 connect_start_ms;
    uint32_t                 last_connected_ms;
    uint32_t                 last_disconnected_ms;
    uint32_t                 last_rx_ms;
    uint32_t                 last_yield_fail_log_ms;
    bool                     disconnect_requested;
    const char              *disconnect_reason;
    uint32_t                 disconnect_request_ms;
    uint32_t                 data_frag_drop_count;
    Mqtt_Puback_Track_t      puback_track[MQTT_PUBACK_TRACK_MAX];
    uint32_t                 puback_warn_count;
    uint32_t                 puback_giveup_count;
    uint32_t                 puback_hard_timeout_count;
    uint32_t                 event_error_count;
    uint32_t                 event_count;
    uint32_t                 last_event_ms;
    uint32_t                 callback_start_ms;
    uint32_t                 callback_last_ms;
    uint32_t                 callback_max_ms;
    uint32_t                 callback_slow_count;
    bool                     callback_active;
    portMUX_TYPE             diag_lock;
};

static uint32_t mqtt_now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000LL);
}

static void mqtt_mark_disconnect_requested(struct Mqtt_Client_Context *ctx,
                                           const char *reason)
{
    if (!ctx) { return; }
    ctx->disconnect_requested = true;
    ctx->disconnect_reason = reason ? reason : "unknown";
    ctx->disconnect_request_ms = mqtt_now_ms();
}

static const char *mqtt_transport_str(esp_mqtt_transport_t transport)
{
    switch (transport)
    {
        case MQTT_TRANSPORT_UNKNOWN:  return "UNKNOWN";
        case MQTT_TRANSPORT_OVER_TCP: return "TCP";
        case MQTT_TRANSPORT_OVER_SSL: return "SSL";
        case MQTT_TRANSPORT_OVER_WS:  return "WS";
        case MQTT_TRANSPORT_OVER_WSS: return "WSS";
        default:                      return "INVALID";
    }
}

static const char *mqtt_protocol_str(esp_mqtt_protocol_ver_t protocol)
{
    switch (protocol)
    {
        case MQTT_PROTOCOL_UNDEFINED: return "UNDEFINED";
        case MQTT_PROTOCOL_V_3_1:     return "3.1";
        case MQTT_PROTOCOL_V_3_1_1:   return "3.1.1";
        case MQTT_PROTOCOL_V_5:       return "5.0";
        default:                      return "INVALID";
    }
}

#if defined(CONFIG_MI_MQTT_HAVE_ESP_BROKER_LIVENESS)
static Mqtt_Client_Broker_Liveness_t mqtt_map_broker_liveness(esp_mqtt_broker_liveness_t state)
{
    switch (state)
    {
        case ESP_MQTT_BROKER_LIVENESS_DISCONNECTED: return MQTT_BROKER_LIVENESS_DISCONNECTED;
        case ESP_MQTT_BROKER_LIVENESS_CONNECTING:    return MQTT_BROKER_LIVENESS_CONNECTING;
        case ESP_MQTT_BROKER_LIVENESS_SUBSCRIBING:   return MQTT_BROKER_LIVENESS_SUBSCRIBING;
        case ESP_MQTT_BROKER_LIVENESS_READY:         return MQTT_BROKER_LIVENESS_READY;
        case ESP_MQTT_BROKER_LIVENESS_PROBING:       return MQTT_BROKER_LIVENESS_PROBING;
        case ESP_MQTT_BROKER_LIVENESS_DEAD:          return MQTT_BROKER_LIVENESS_DEAD;
        default:                                     return MQTT_BROKER_LIVENESS_DISCONNECTED;
    }
}
#endif /* CONFIG_MI_MQTT_HAVE_ESP_BROKER_LIVENESS */

static void log_esp_mqtt_client_config(const esp_mqtt_client_config_t *cfg)
{
    if (cfg == NULL) { return; }

    const char *password = cfg->credentials.authentication.password;
    ESP_LOGI(MI_MQTT_TAG,
             "[MQTT_DIAG][ESP_MQTT_CFG][BROKER] uri=%s hostname=%s port=%u "
             "transport=%s(%d) path=%s",
             STRING_PTR_PRINT_SANITY_CHECK(cfg->broker.address.uri),
             STRING_PTR_PRINT_SANITY_CHECK(cfg->broker.address.hostname),
             (unsigned int)cfg->broker.address.port,
             mqtt_transport_str(cfg->broker.address.transport),
             (int)cfg->broker.address.transport,
             STRING_PTR_PRINT_SANITY_CHECK(cfg->broker.address.path));
    ESP_LOGI(MI_MQTT_TAG,
             "[MQTT_DIAG][ESP_MQTT_CFG][VERIFY] use_global_ca_store=%d "
             "crt_bundle=%d certificate=%d certificate_len=%u psk=%d "
             "skip_cn_check=%d alpn=%d",
             cfg->broker.verification.use_global_ca_store ? 1 : 0,
             cfg->broker.verification.crt_bundle_attach ? 1 : 0,
             cfg->broker.verification.certificate ? 1 : 0,
             (unsigned int)cfg->broker.verification.certificate_len,
             cfg->broker.verification.psk_hint_key ? 1 : 0,
             cfg->broker.verification.skip_cert_common_name_check ? 1 : 0,
             cfg->broker.verification.alpn_protos ? 1 : 0);
    ESP_LOGI(MI_MQTT_TAG,
             "[MQTT_DIAG][ESP_MQTT_CFG][CREDENTIALS] username=%s client_id=%s "
             "set_null_client_id=%d password_set=%d password_len=%u "
             "client_cert=%d client_cert_len=%u client_key=%d client_key_len=%u "
             "key_password_set=%d key_password_len=%d secure_element=%d ds_data=%d",
             STRING_PTR_PRINT_SANITY_CHECK(cfg->credentials.username),
             STRING_PTR_PRINT_SANITY_CHECK(cfg->credentials.client_id),
             cfg->credentials.set_null_client_id ? 1 : 0,
             password ? 1 : 0,
             (unsigned int)(password ? strlen(password) : 0),
             cfg->credentials.authentication.certificate ? 1 : 0,
             (unsigned int)cfg->credentials.authentication.certificate_len,
             cfg->credentials.authentication.key ? 1 : 0,
             (unsigned int)cfg->credentials.authentication.key_len,
             cfg->credentials.authentication.key_password ? 1 : 0,
             cfg->credentials.authentication.key_password_len,
             cfg->credentials.authentication.use_secure_element ? 1 : 0,
             cfg->credentials.authentication.ds_data ? 1 : 0);
    ESP_LOGI(MI_MQTT_TAG,
             "[MQTT_DIAG][ESP_MQTT_CFG][SESSION] clean_session=%d "
             "disable_clean_session=%d keepalive=%d disable_keepalive=%d "
             "protocol=%s(%d) retransmit_timeout=%d lwt_topic=%s "
             "lwt_msg_len=%d lwt_qos=%d lwt_retain=%d",
             cfg->session.disable_clean_session ? 0 : 1,
             cfg->session.disable_clean_session ? 1 : 0,
             cfg->session.keepalive,
             cfg->session.disable_keepalive ? 1 : 0,
             mqtt_protocol_str(cfg->session.protocol_ver),
             (int)cfg->session.protocol_ver,
             cfg->session.message_retransmit_timeout,
             STRING_PTR_PRINT_SANITY_CHECK(cfg->session.last_will.topic),
             cfg->session.last_will.msg_len,
             cfg->session.last_will.qos,
             cfg->session.last_will.retain);
    ESP_LOGI(MI_MQTT_TAG,
             "[MQTT_DIAG][ESP_MQTT_CFG][NETWORK] timeout_ms=%d "
             "reconnect_timeout_ms=%d refresh_after_ms=%d disable_auto_reconnect=%d",
             cfg->network.timeout_ms,
             cfg->network.reconnect_timeout_ms,
             cfg->network.refresh_connection_after_ms,
             cfg->network.disable_auto_reconnect ? 1 : 0);
    ESP_LOGI(MI_MQTT_TAG,
             "[MQTT_DIAG][ESP_MQTT_CFG][TASK_BUFFER] task_prio=%d "
             "task_stack=%d buffer_size=%d buffer_out_size=%d",
             cfg->task.priority,
             cfg->task.stack_size,
             cfg->buffer.size,
             cfg->buffer.out_size);
}

static void mqtt_puback_track_clear(struct Mqtt_Client_Context *ctx)
{
    if (!ctx) { return; }
    memset(ctx->puback_track, 0, sizeof(ctx->puback_track));
}

static void mqtt_mark_rx(struct Mqtt_Client_Context *ctx)
{
    if (!ctx) { return; }
    ctx->last_rx_ms = mqtt_now_ms();
}

static void mqtt_puback_track_add(struct Mqtt_Client_Context *ctx,
                                  uint16_t msg_id,
                                  uint8_t qos,
                                  uint16_t payload_len)
{
    if (!ctx || msg_id == 0 || qos == 0) { return; }

    uint32_t now_ms = mqtt_now_ms();
    int free_index = -1;
    int oldest_index = 0;
    uint32_t oldest_ms = UINT32_MAX;

    for (int i = 0; i < MQTT_PUBACK_TRACK_MAX; ++i)
    {
        if (ctx->puback_track[i].msg_id == msg_id)
        {
            ctx->puback_track[i].sent_ms = now_ms;
            ctx->puback_track[i].payload_len = payload_len;
            ctx->puback_track[i].qos = qos;
            ctx->puback_track[i].warn_level = 0;
            return;
        }
        if (ctx->puback_track[i].msg_id == 0 && free_index < 0)
        {
            free_index = i;
        }
        if (ctx->puback_track[i].msg_id != 0 && ctx->puback_track[i].sent_ms < oldest_ms)
        {
            oldest_ms = ctx->puback_track[i].sent_ms;
            oldest_index = i;
        }
    }

    int index = free_index >= 0 ? free_index : oldest_index;
    if (free_index < 0)
    {
        ESP_LOGW(MI_MQTT_TAG,
                 "[MQTT_DIAG][PUBACK_TRACK_OVERWRITE] old_msgid=%u new_msgid=%u",
                 (unsigned int)ctx->puback_track[index].msg_id,
                 (unsigned int)msg_id);
    }
    ctx->puback_track[index].msg_id = msg_id;
    ctx->puback_track[index].sent_ms = now_ms;
    ctx->puback_track[index].payload_len = payload_len;
    ctx->puback_track[index].qos = qos;
    ctx->puback_track[index].warn_level = 0;
    ESP_LOGI(MI_MQTT_TAG,
             "[MQTT_DIAG][PUBACK_TRACK_ADD] msgid=%u qos=%u len=%u warn=%u hard_timeout=%u",
             (unsigned int)msg_id,
             (unsigned int)qos,
             (unsigned int)payload_len,
             (unsigned int)MQTT_PUBACK_WARN_MS,
             (unsigned int)MQTT_PUBACK_HARD_TIMEOUT_MS);
}

static void mqtt_puback_track_done(struct Mqtt_Client_Context *ctx, uint16_t msg_id)
{
    if (!ctx || msg_id == 0) { return; }

    uint32_t now_ms = mqtt_now_ms();
    for (int i = 0; i < MQTT_PUBACK_TRACK_MAX; ++i)
    {
        if (ctx->puback_track[i].msg_id == msg_id)
        {
            uint32_t rtt_ms = now_ms - ctx->puback_track[i].sent_ms;
            ESP_LOGI(MI_MQTT_TAG,
                     "[MQTT_DIAG][PUBACK_TRACK_DONE] msgid=%u rtt=%u len=%u",
                     (unsigned int)msg_id,
                     (unsigned int)rtt_ms,
                     (unsigned int)ctx->puback_track[i].payload_len);
            memset(&ctx->puback_track[i], 0, sizeof(ctx->puback_track[i]));
            return;
        }
    }
    ESP_LOGW(MI_MQTT_TAG,
             "[MQTT_DIAG][PUBACK_TRACK_MISS] msgid=%u",
             (unsigned int)msg_id);
}

static void mqtt_puback_track_deleted(struct Mqtt_Client_Context *ctx, uint16_t msg_id)
{
    if (!ctx || msg_id == 0) { return; }

    uint32_t now_ms = mqtt_now_ms();
    for (int i = 0; i < MQTT_PUBACK_TRACK_MAX; ++i)
    {
        if (ctx->puback_track[i].msg_id == msg_id)
        {
            uint32_t age_ms = now_ms - ctx->puback_track[i].sent_ms;
            ESP_LOGW(MI_MQTT_TAG,
                     "[MQTT_DIAG][PUBACK_TRACK_DELETED] msgid=%u age=%u len=%u reason=outbox_expired",
                     (unsigned int)msg_id,
                     (unsigned int)age_ms,
                     (unsigned int)ctx->puback_track[i].payload_len);
            memset(&ctx->puback_track[i], 0, sizeof(ctx->puback_track[i]));
            return;
        }
    }

    ESP_LOGW(MI_MQTT_TAG,
             "[MQTT_DIAG][PUBACK_TRACK_DELETE_MISS] msgid=%u reason=outbox_expired",
             (unsigned int)msg_id);
}

static bool mqtt_puback_track_check_timeout(struct Mqtt_Client_Context *ctx)
{
    if (!ctx) { return false; }

    uint32_t now_ms = mqtt_now_ms();
    for (int i = 0; i < MQTT_PUBACK_TRACK_MAX; ++i)
    {
        if (ctx->puback_track[i].msg_id == 0) { continue; }

        uint32_t elapsed_ms = now_ms - ctx->puback_track[i].sent_ms;
        if (elapsed_ms >= MQTT_PUBACK_WARN_MS)
        {
            uint32_t last_rx_age_ms = ctx->last_rx_ms ? now_ms - ctx->last_rx_ms : UINT32_MAX;
            if (elapsed_ms >= MQTT_PUBACK_WARN_MS && ctx->puback_track[i].warn_level == 0)
            {
                ctx->puback_warn_count++;
                ctx->puback_track[i].warn_level = 1;
                ESP_LOGW(MI_MQTT_TAG,
                         "[MQTT_DIAG][PUBACK_WAIT] msgid=%u elapsed=%u warn=%u "
                         "hard_timeout=%u last_rx_age=%u qos=%u len=%u connected=%d warn_count=%" PRIu32,
                         (unsigned int)ctx->puback_track[i].msg_id,
                         (unsigned int)elapsed_ms,
                         (unsigned int)MQTT_PUBACK_WARN_MS,
                         (unsigned int)MQTT_PUBACK_HARD_TIMEOUT_MS,
                         (unsigned int)last_rx_age_ms,
                         (unsigned int)ctx->puback_track[i].qos,
                         (unsigned int)ctx->puback_track[i].payload_len,
                         ctx->connected ? 1 : 0,
                         ctx->puback_warn_count);
            }
            if (elapsed_ms >= MQTT_PUBACK_HARD_TIMEOUT_MS &&
                last_rx_age_ms < MQTT_RX_IDLE_TIMEOUT_MS)
            {
                ctx->puback_giveup_count++;
                ESP_LOGW(MI_MQTT_TAG,
                         "[MQTT_DIAG][PUBACK_GIVE_UP_KEEP_CONNECTION] msgid=%u elapsed=%u timeout=%u "
                         "last_rx_age=%u rx_idle_timeout=%u qos=%u len=%u giveup_count=%" PRIu32,
                         (unsigned int)ctx->puback_track[i].msg_id,
                         (unsigned int)elapsed_ms,
                         (unsigned int)MQTT_PUBACK_HARD_TIMEOUT_MS,
                         (unsigned int)last_rx_age_ms,
                         (unsigned int)MQTT_RX_IDLE_TIMEOUT_MS,
                         (unsigned int)ctx->puback_track[i].qos,
                         (unsigned int)ctx->puback_track[i].payload_len,
                         ctx->puback_giveup_count);
                memset(&ctx->puback_track[i], 0, sizeof(ctx->puback_track[i]));
                continue;
            }
            if (elapsed_ms >= MQTT_PUBACK_HARD_TIMEOUT_MS &&
                last_rx_age_ms >= MQTT_RX_IDLE_TIMEOUT_MS)
            {
                ctx->puback_hard_timeout_count++;
                ESP_LOGE(MI_MQTT_TAG,
                         "[MQTT_DIAG][PUBACK_HARD_TIMEOUT] msgid=%u elapsed=%u timeout=%u "
                         "last_rx_age=%u rx_idle_timeout=%u qos=%u len=%u connected=%d timeout_count=%" PRIu32,
                         (unsigned int)ctx->puback_track[i].msg_id,
                         (unsigned int)elapsed_ms,
                         (unsigned int)MQTT_PUBACK_HARD_TIMEOUT_MS,
                         (unsigned int)last_rx_age_ms,
                         (unsigned int)MQTT_RX_IDLE_TIMEOUT_MS,
                         (unsigned int)ctx->puback_track[i].qos,
                         (unsigned int)ctx->puback_track[i].payload_len,
                         ctx->connected ? 1 : 0,
                         ctx->puback_hard_timeout_count);
                mqtt_puback_track_clear(ctx);
                return true;
            }
        }
    }
    return false;
}

static void log_mqtt_error_detail(const char *event_name,
                                  const esp_mqtt_error_codes_t *error_handle)
{
    if (!error_handle)
    {
        ESP_LOGE(MI_MQTT_TAG, "[MQTT_DIAG][%s] error_handle=NULL", event_name);
        return;
    }

    ESP_LOGE(MI_MQTT_TAG,
             "[MQTT_DIAG][%s] type=%d tls_last_err=0x%x tls_stack_err=0x%x "
             "cert_flags=0x%x sock_errno=%d(%s) mqtt_connect_rc=%d",
             event_name,
             (int)error_handle->error_type,
             (unsigned int)error_handle->esp_tls_last_esp_err,
             (unsigned int)error_handle->esp_tls_stack_err,
             (unsigned int)error_handle->esp_tls_cert_verify_flags,
             (int)error_handle->esp_transport_sock_errno,
             STRING_PTR_PRINT_SANITY_CHECK(strerror(error_handle->esp_transport_sock_errno)),
             (int)error_handle->connect_return_code);
}

static void log_mqtt_disconnect_detail(struct Mqtt_Client_Context *ctx,
                                       const esp_mqtt_error_codes_t *error_handle)
{
    if (!error_handle)
    {
        ESP_LOGW(MI_MQTT_TAG,
                 "[MQTT_DIAG][DISCONNECTED_DETAIL] requested=%d reason=%s error_handle=NULL",
                 (ctx && ctx->disconnect_requested) ? 1 : 0,
                 (ctx && ctx->disconnect_reason) ? ctx->disconnect_reason : "none");
        return;
    }

    bool has_transport_error =
        (error_handle->error_type != MQTT_ERROR_TYPE_NONE) ||
        (error_handle->esp_tls_last_esp_err != 0) ||
        (error_handle->esp_tls_stack_err != 0) ||
        (error_handle->esp_tls_cert_verify_flags != 0) ||
        (error_handle->esp_transport_sock_errno != 0) ||
        (error_handle->connect_return_code != 0);

    if (ctx && ctx->disconnect_requested && !has_transport_error)
    {
        ESP_LOGI(MI_MQTT_TAG,
                 "[MQTT_DIAG][DISCONNECTED_DETAIL] requested=1 reason=%s "
                 "transport_error=0 note=active_disconnect_no_tcp_error "
                 "type=%d sock_errno=%d(%s) tls_last_err=0x%x tls_stack_err=0x%x "
                 "cert_flags=0x%x mqtt_connect_rc=%d",
                 ctx->disconnect_reason ? ctx->disconnect_reason : "unknown",
                 (int)error_handle->error_type,
                 (int)error_handle->esp_transport_sock_errno,
                 STRING_PTR_PRINT_SANITY_CHECK(strerror(error_handle->esp_transport_sock_errno)),
                 (unsigned int)error_handle->esp_tls_last_esp_err,
                 (unsigned int)error_handle->esp_tls_stack_err,
                 (unsigned int)error_handle->esp_tls_cert_verify_flags,
                 (int)error_handle->connect_return_code);
        return;
    }

    log_mqtt_error_detail("DISCONNECTED_DETAIL", error_handle);
}

static const char *mqtt_error_type_str(esp_mqtt_error_type_t type)
{
    switch (type)
    {
        case MQTT_ERROR_TYPE_NONE:               return "NONE";
        case MQTT_ERROR_TYPE_TCP_TRANSPORT:      return "TCP_TRANSPORT";
        case MQTT_ERROR_TYPE_CONNECTION_REFUSED: return "CONNECTION_REFUSED";
        case MQTT_ERROR_TYPE_SUBSCRIBE_FAILED:   return "SUBSCRIBE_FAILED";
        default:                                 return "UNKNOWN";
    }
}

static void mqtt_puback_track_snapshot(struct Mqtt_Client_Context *ctx,
                                       uint32_t *pending_count,
                                       uint32_t *oldest_age_ms,
                                       uint16_t *oldest_msg_id)
{
    if (pending_count) { *pending_count = 0; }
    if (oldest_age_ms) { *oldest_age_ms = 0; }
    if (oldest_msg_id) { *oldest_msg_id = 0; }
    if (!ctx) { return; }

    uint32_t now_ms = mqtt_now_ms();
    uint32_t max_age_ms = 0;
    uint16_t max_age_msg_id = 0;
    uint32_t count = 0;

    for (int i = 0; i < MQTT_PUBACK_TRACK_MAX; ++i)
    {
        if (ctx->puback_track[i].msg_id == 0) { continue; }

        uint32_t age_ms = now_ms - ctx->puback_track[i].sent_ms;
        count++;
        if (age_ms >= max_age_ms)
        {
            max_age_ms = age_ms;
            max_age_msg_id = ctx->puback_track[i].msg_id;
        }
    }

    if (pending_count) { *pending_count = count; }
    if (oldest_age_ms) { *oldest_age_ms = max_age_ms; }
    if (oldest_msg_id) { *oldest_msg_id = max_age_msg_id; }
}

static void log_mqtt_event_error_context(struct Mqtt_Client_Context *ctx,
                                         esp_mqtt_event_handle_t event)
{
    uint32_t now_ms = mqtt_now_ms();
    uint32_t last_rx_age_ms = (ctx && ctx->last_rx_ms) ? now_ms - ctx->last_rx_ms : UINT32_MAX;
    uint32_t connected_age_ms = (ctx && ctx->last_connected_ms) ? now_ms - ctx->last_connected_ms : 0;
    uint32_t pending_count = 0;
    uint32_t oldest_age_ms = 0;
    uint16_t oldest_msg_id = 0;
    esp_mqtt_error_codes_t *err = event ? event->error_handle : NULL;

    mqtt_puback_track_snapshot(ctx, &pending_count, &oldest_age_ms, &oldest_msg_id);
    if (ctx) { ctx->event_error_count++; }

    ESP_LOGE(MI_MQTT_TAG,
             "[MQTT_DIAG][EVENT_ERROR_CONTEXT] count=%" PRIu32 " seq=%" PRIu32
             " connected=%d started=%d last_rx_age=%" PRIu32
             " connected_age=%" PRIu32 " pending_puback=%" PRIu32
             " oldest_msgid=%u oldest_age=%" PRIu32
             " event_msgid=%d event_qos=%d event_data_len=%d",
             ctx ? ctx->event_error_count : 0,
             ctx ? ctx->connect_seq : 0,
             (ctx && ctx->connected) ? 1 : 0,
             (ctx && ctx->started) ? 1 : 0,
             last_rx_age_ms,
             connected_age_ms,
             pending_count,
             (unsigned int)oldest_msg_id,
             oldest_age_ms,
             event ? event->msg_id : 0,
             event ? event->qos : 0,
             event ? event->data_len : 0);

    if (err)
    {
        ESP_LOGE(MI_MQTT_TAG,
                 "[MQTT_DIAG][EVENT_ERROR_DETAIL] type=%d(%s) "
                 "tls_last_err=0x%x(%s) tls_stack_err=0x%x cert_flags=0x%x "
                 "sock_errno=%d(%s) mqtt_connect_rc=%d",
                 (int)err->error_type,
                 mqtt_error_type_str(err->error_type),
                 (unsigned int)err->esp_tls_last_esp_err,
                 esp_err_to_name(err->esp_tls_last_esp_err),
                 (unsigned int)err->esp_tls_stack_err,
                 (unsigned int)err->esp_tls_cert_verify_flags,
                 (int)err->esp_transport_sock_errno,
                 STRING_PTR_PRINT_SANITY_CHECK(strerror(err->esp_transport_sock_errno)),
                 (int)err->connect_return_code);
    }
    else
    {
        ESP_LOGE(MI_MQTT_TAG, "[MQTT_DIAG][EVENT_ERROR_DETAIL] error_handle=NULL");
    }
}

static void log_mqtt_callback_cost(struct Mqtt_Client_Context *ctx,
                                   const char *name,
                                   int msg_id,
                                   uint32_t start_ms)
{
    uint32_t cost_ms = mqtt_now_ms() - start_ms;
    if (ctx)
    {
        taskENTER_CRITICAL(&ctx->diag_lock);
        ctx->callback_last_ms = cost_ms;
        if (cost_ms > ctx->callback_max_ms)
        {
            ctx->callback_max_ms = cost_ms;
        }
        ctx->callback_active = false;
        if (cost_ms >= MQTT_EVENT_CALLBACK_WARN_MS)
        {
            ctx->callback_slow_count++;
        }
        taskEXIT_CRITICAL(&ctx->diag_lock);
    }
    if (cost_ms < MQTT_EVENT_CALLBACK_WARN_MS)
    {
        return;
    }

    ESP_LOGW(MI_MQTT_TAG,
             "[MQTT_DIAG][EVENT_CALLBACK_COST] cb=%s msgid=%d cost=%" PRIu32
             "ms warn=%u seq=%" PRIu32 " connected=%d",
             name ? name : "unknown",
             msg_id,
             cost_ms,
             (unsigned int)MQTT_EVENT_CALLBACK_WARN_MS,
             ctx ? ctx->connect_seq : 0,
             (ctx && ctx->connected) ? 1 : 0);
}

static uint32_t mqtt_callback_begin(struct Mqtt_Client_Context *ctx)
{
    uint32_t now_ms = mqtt_now_ms();
    if (ctx)
    {
        taskENTER_CRITICAL(&ctx->diag_lock);
        ctx->callback_start_ms = now_ms;
        ctx->callback_active = true;
        taskEXIT_CRITICAL(&ctx->diag_lock);
    }
    return now_ms;
}

/* ── MQTT_EVENT_DATA：esp-mqtt 提供非 null-terminated 字串 ──────────────── */
static void handle_data_event(struct Mqtt_Client_Context *ctx,
                               esp_mqtt_event_handle_t event)
{
    ESP_LOGI(MI_MQTT_TAG,
             "[MQTT_DIAG][DATA_EVT] seq=%" PRIu32 " msgid=%d qos=%d "
             "topic_len=%d data_len=%d total_len=%d offset=%d connected=%d",
             ctx->connect_seq,
             event->msg_id,
             event->qos,
             event->topic_len,
             event->data_len,
             event->total_data_len,
             event->current_data_offset,
             ctx->connected ? 1 : 0);
    if (mqtt_diag_field_contains(event->topic, event->topic_len, "report_response") ||
        mqtt_diag_field_contains(event->data, event->data_len, "agora_agent_device_access"))
    {
        AI_DIALOG_DIAG_LOGI(MI_MQTT_TAG, "mqtt_sub_data_event",
                            "seq=%" PRIu32 " msgid=%d qos=%d topic_len=%d data_len=%d offset=%d",
                            ctx->connect_seq,
                            event->msg_id,
                            event->qos,
                            event->topic_len,
                            event->data_len,
                            event->current_data_offset);
    }

    if (event->current_data_offset != 0)
    {
        ctx->data_frag_drop_count++;
        ESP_LOGW(MI_MQTT_TAG,
                 "[MQTT_DIAG][DATA_FRAGMENT_DROP] seq=%" PRIu32 " msgid=%d "
                 "offset=%d data_len=%d total_len=%d drop_count=%" PRIu32,
                 ctx->connect_seq,
                 event->msg_id,
                 event->current_data_offset,
                 event->data_len,
                 event->total_data_len,
                 ctx->data_frag_drop_count);
        return;  /* 分段包：僅處理首幀，大 payload 場景可在此擴充重組 */
    }

    if (!ctx->config.On_Message)
    {
        ESP_LOGW(MI_MQTT_TAG,
                 "[MQTT_DIAG][DATA_NO_CALLBACK] seq=%" PRIu32 " msgid=%d len=%d",
                 ctx->connect_seq, event->msg_id, event->data_len);
        return;
    }

    if (event->total_data_len > event->data_len)
    {
        ESP_LOGW(MI_MQTT_TAG,
                 "[MQTT_DIAG][DATA_FRAGMENT_FIRST] seq=%" PRIu32 " msgid=%d "
                 "forward_len=%d total_len=%d buffer=%d",
                 ctx->connect_seq,
                 event->msg_id,
                 event->data_len,
                 event->total_data_len,
                 ESP_MQTT_BUF_SIZE);
    }

    /* 複製 topic + payload，確保 null-terminated */
    uint32_t alloc_copy_start_ms = mqtt_now_ms();
    char    *topic   = heap_caps_malloc((size_t)event->topic_len + 1, MALLOC_CAP_DEFAULT);
    uint8_t *payload = heap_caps_malloc((size_t)event->data_len  + 1, MALLOC_CAP_DEFAULT);
    if (!topic || !payload)
    {
        ESP_LOGE(MI_MQTT_TAG, "OOM in handle_data_event");
        free(topic);
        free(payload);
        return;
    }
    memcpy(topic,   event->topic, (size_t)event->topic_len);
    topic[event->topic_len] = '\0';
    memcpy(payload, event->data,  (size_t)event->data_len);
    payload[event->data_len] = '\0';
    uint32_t alloc_copy_ms = mqtt_now_ms() - alloc_copy_start_ms;

    ESP_LOGI(MI_MQTT_TAG, "[MQTT_RECV] msgid=%d topic=%s payload_len=%d",
             event->msg_id, topic, event->data_len);
    ESP_LOGD(MI_MQTT_TAG, "[MQTT_RECV_PAYLOAD] payload=%.*s",
             event->data_len, (const char *)payload);

    Mqtt_Client_Message_t msg = {
        .Topic   = topic,
        .Payload = payload,
        .Length  = (size_t)event->data_len,
        .Qos     = (Mqtt_Client_Qos_t)event->qos,
    };
    uint32_t cb_start_ms = mqtt_callback_begin(ctx);
    ctx->config.On_Message(ctx, (uint16_t)event->msg_id, &msg,
                           ctx->config.Userdata);
    uint32_t on_message_ms = mqtt_now_ms() - cb_start_ms;
    bool data_cost_slow = (alloc_copy_ms >= MQTT_EVENT_CALLBACK_WARN_MS) ||
                          (on_message_ms >= MQTT_EVENT_CALLBACK_WARN_MS);
    if (data_cost_slow)
    {
        ESP_LOGW(MI_MQTT_TAG,
                 "[MQTT_DIAG][DATA_COST] msgid=%d alloc_copy_ms=%" PRIu32
                 " on_message_ms=%" PRIu32 " warn=%u seq=%" PRIu32
                 " data_len=%d topic_len=%d",
                 event->msg_id,
                 alloc_copy_ms,
                 on_message_ms,
                 (unsigned int)MQTT_EVENT_CALLBACK_WARN_MS,
                 ctx->connect_seq,
                 event->data_len,
                 event->topic_len);
    }
    else
    {
        ESP_LOGI(MI_MQTT_TAG,
                 "[MQTT_DIAG][DATA_COST] msgid=%d alloc_copy_ms=%" PRIu32
                 " on_message_ms=%" PRIu32 " warn=%u seq=%" PRIu32
                 " data_len=%d topic_len=%d",
                 event->msg_id,
                 alloc_copy_ms,
                 on_message_ms,
                 (unsigned int)MQTT_EVENT_CALLBACK_WARN_MS,
                 ctx->connect_seq,
                 event->data_len,
                 event->topic_len);
    }
    log_mqtt_callback_cost(ctx, "DATA", event->msg_id, cb_start_ms);

    free(topic);
    free(payload);
}

/* ── esp-mqtt 事件 handler ─────────────────────────────────────────────── */
static void esp_mqtt_event_handler(void *handler_args,
                                    esp_event_base_t base,
                                    int32_t event_id,
                                    void *event_data)
{
    struct Mqtt_Client_Context *ctx = (struct Mqtt_Client_Context *)handler_args;
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    uint32_t event_now_ms = mqtt_now_ms();
    taskENTER_CRITICAL(&ctx->diag_lock);
    ctx->event_count++;
    ctx->last_event_ms = event_now_ms;
    taskEXIT_CRITICAL(&ctx->diag_lock);

    switch ((esp_mqtt_event_id_t)event_id)
    {
        case MQTT_EVENT_CONNECTED:
        {
            bool was_connected = ctx->connected;
            uint32_t now_ms = mqtt_now_ms();
            uint32_t cost_ms = ctx->connect_start_ms ? now_ms - ctx->connect_start_ms : 0;
            ctx->connected = true;
            ctx->last_connected_ms = now_ms;
            ctx->last_rx_ms = now_ms;
            ctx->disconnect_requested = false;
            ctx->disconnect_reason = "none";
            ctx->disconnect_request_ms = 0;
            mqtt_puback_track_clear(ctx);
            xSemaphoreGive(ctx->conn_sem);
            ESP_LOGI(MI_MQTT_TAG, "mqtt client connected!");
            ESP_LOGI(MI_MQTT_TAG,
                     "[MQTT_DIAG][EVENT_CONNECTED] seq=%" PRIu32 " cost=%" PRIu32
                     "ms was_connected=%d started=%d protocol=%s(%d) session_present=%d",
                     ctx->connect_seq,
                     cost_ms,
                     was_connected ? 1 : 0,
                     ctx->started ? 1 : 0,
                     mqtt_protocol_str(event->protocol_ver),
                     (int)event->protocol_ver,
                     event->session_present);
            if (ctx->config.On_Connected)
            {
                uint32_t cb_start_ms = mqtt_callback_begin(ctx);
                ctx->config.On_Connected(ctx, ctx->config.Userdata);
                log_mqtt_callback_cost(ctx, "CONNECTED", event->msg_id, cb_start_ms);
            }
            break;
        }

        case MQTT_EVENT_DISCONNECTED:
        {
            bool was_connected = ctx->connected;
            uint32_t now_ms = mqtt_now_ms();
            uint32_t uptime_ms = ctx->last_connected_ms ? now_ms - ctx->last_connected_ms : 0;
            ctx->connected = false;
            ctx->last_disconnected_ms = now_ms;
            ctx->last_rx_ms = 0;
            mqtt_puback_track_clear(ctx);
            xSemaphoreGive(ctx->disc_sem);
            ESP_LOGI(MI_MQTT_TAG, "mqtt client disconnected!");
            ESP_LOGW(MI_MQTT_TAG,
                     "[MQTT_DIAG][EVENT_DISCONNECTED] seq=%" PRIu32
                     " uptime=%" PRIu32 "ms was_connected=%d started=%d "
                     "requested=%d reason=%s request_age=%" PRIu32 "ms",
                     ctx->connect_seq,
                     uptime_ms,
                     was_connected ? 1 : 0,
                     ctx->started ? 1 : 0,
                     ctx->disconnect_requested ? 1 : 0,
                     ctx->disconnect_reason ? ctx->disconnect_reason : "none",
                     ctx->disconnect_request_ms ? now_ms - ctx->disconnect_request_ms : 0);
            if (event->error_handle)
            {
                log_mqtt_disconnect_detail(ctx, event->error_handle);
            }
            if (ctx->config.On_Disconnected)
            {
                uint32_t cb_start_ms = mqtt_callback_begin(ctx);
                ctx->config.On_Disconnected(ctx, ctx->config.Userdata);
                log_mqtt_callback_cost(ctx, "DISCONNECTED", event->msg_id, cb_start_ms);
            }
            break;
        }

        case MQTT_EVENT_DATA:
            mqtt_mark_rx(ctx);
            handle_data_event(ctx, event);
            break;

        case MQTT_EVENT_PUBLISHED:
            mqtt_mark_rx(ctx);
            ESP_LOGI(MI_MQTT_TAG, "[MQTT_PUBACK] QoS1 handshake done msgId=%d",
                     event->msg_id);
            mqtt_puback_track_done(ctx, (uint16_t)event->msg_id);
            if (ctx->config.On_Published)
            {
                uint32_t cb_start_ms = mqtt_callback_begin(ctx);
                ctx->config.On_Published(ctx, (uint16_t)event->msg_id,
                                         ctx->config.Userdata);
                log_mqtt_callback_cost(ctx, "PUBLISHED", event->msg_id, cb_start_ms);
            }
            break;

        case MQTT_EVENT_SUBSCRIBED:
            mqtt_mark_rx(ctx);
            ESP_LOGI(MI_MQTT_TAG, "[MQTT_SUB] received SUBACK msgId=%d",
                     event->msg_id);
            if (ctx->config.On_Subscribed)
            {
                uint32_t cb_start_ms = mqtt_callback_begin(ctx);
                ctx->config.On_Subscribed(ctx, (uint16_t)event->msg_id,
                                          ctx->config.Userdata);
                log_mqtt_callback_cost(ctx, "SUBSCRIBED", event->msg_id, cb_start_ms);
            }
            break;

        case MQTT_EVENT_UNSUBSCRIBED:
            mqtt_mark_rx(ctx);
            if (ctx->config.On_Unsubscribed)
            {
                uint32_t cb_start_ms = mqtt_callback_begin(ctx);
                ctx->config.On_Unsubscribed(ctx, (uint16_t)event->msg_id,
                                             ctx->config.Userdata);
                log_mqtt_callback_cost(ctx, "UNSUBSCRIBED", event->msg_id, cb_start_ms);
            }
            break;

        case MQTT_EVENT_DELETED:
            mqtt_puback_track_deleted(ctx, (uint16_t)event->msg_id);
            break;

        case MQTT_EVENT_ERROR:
            log_mqtt_event_error_context(ctx, event);
            log_mqtt_error_detail("EVENT_ERROR", event->error_handle);
            break;

        default:
            break;
    }
}

/* ── 公開 API ───────────────────────────────────────────────────────────── */

void *Mqtt_Client_New(void)
{
    struct Mqtt_Client_Context *ctx =
        (struct Mqtt_Client_Context *)heap_caps_calloc(
            1, sizeof(struct Mqtt_Client_Context), MALLOC_CAP_DEFAULT);
    if (!ctx) { return NULL; }
    ctx->diag_lock = (portMUX_TYPE)portMUX_INITIALIZER_UNLOCKED;
    ctx->conn_sem = xSemaphoreCreateBinary();
    ctx->disc_sem = xSemaphoreCreateBinary();
    if (!ctx->conn_sem || !ctx->disc_sem)
    {
        if (ctx->conn_sem) vSemaphoreDelete(ctx->conn_sem);
        if (ctx->disc_sem) vSemaphoreDelete(ctx->disc_sem);
        free(ctx);
        return NULL;
    }
    return ctx;
}

void Mqtt_Client_Free(void *client_context)
{
    struct Mqtt_Client_Context *ctx = (struct Mqtt_Client_Context *)client_context;
    if (!ctx) { return; }
    if (ctx->conn_sem) vSemaphoreDelete(ctx->conn_sem);
    if (ctx->disc_sem) vSemaphoreDelete(ctx->disc_sem);
    free(ctx);
}

Mqtt_Client_Status_t Mqtt_Client_Init(void *client_context,
                                       const Mqtt_Client_Config_t *config)
{
    struct Mqtt_Client_Context *ctx = (struct Mqtt_Client_Context *)client_context;
    if (!ctx || !config) { return MQTT_STATUS_INVALID_PARAM; }

    ctx->config  = *config;
    ctx->started = false;

    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address = {
                .hostname  = config->Tcp_Connect_Params.Host,
                .transport = MQTT_TRANSPORT_OVER_TCP,
                .port      = (uint32_t)config->Tcp_Connect_Params.Port,
            },
        },
        .credentials = {
            .client_id    = config->Client_Id,
            .username     = config->Username,
            .authentication = {
                .password = config->Password,
            },
        },
        .session = {
            .keepalive                   = (int)config->Keepalive,
            .disable_keepalive           = false,
            .disable_clean_session       = false,
            .protocol_ver                = MQTT_PROTOCOL_V_3_1_1,
            .message_retransmit_timeout  = ESP_MQTT_RETRANSMIT_TIMEOUT_MS,
        },
        .network = {
            /* TCP 建連/收發操作超時。MQTT keepalive 由 esp-mqtt 內建機制維護。 */
            .timeout_ms             = ESP_MQTT_NETWORK_TIMEOUT_MS,
            .disable_auto_reconnect = true,   /* 由 entity 狀態機管理重連 */
        },
        .task = {
            .priority   = ESP_MQTT_TASK_PRIO,
            .stack_size = ESP_MQTT_TASK_STACK,
        },
        .buffer = {
            .size = ESP_MQTT_BUF_SIZE,
        },
    };

    log_esp_mqtt_client_config(&mqtt_cfg);
    ctx->handle = esp_mqtt_client_init(&mqtt_cfg);
    if (!ctx->handle)
    {
        ESP_LOGE(MI_MQTT_TAG, "esp_mqtt_client_init failed");
        return MQTT_STATUS_NETWORK_INIT_FAILED;
    }
    esp_mqtt_client_register_event(ctx->handle, ESP_EVENT_ANY_ID,
                                    esp_mqtt_event_handler, ctx);
    ESP_LOGI(MI_MQTT_TAG,
             "[MQTT_DIAG][INIT] host=%s port=%u mqtt_keepalive=enabled "
             "requested_keepalive=%u protocol=%s(%d) net_timeout=%u buffer=%u "
             "auto_reconnect=0 retransmit=%u tcp_keepalive=%u/%u/%u",
             config->Tcp_Connect_Params.Host ? config->Tcp_Connect_Params.Host : "(null)",
             (unsigned int)config->Tcp_Connect_Params.Port,
             (unsigned int)config->Keepalive,
             mqtt_protocol_str(mqtt_cfg.session.protocol_ver),
             (int)mqtt_cfg.session.protocol_ver,
             (unsigned int)ESP_MQTT_NETWORK_TIMEOUT_MS,
             (unsigned int)ESP_MQTT_BUF_SIZE,
             (unsigned int)ESP_MQTT_RETRANSMIT_TIMEOUT_MS,
             (unsigned int)ESP_MQTT_TCP_KEEPALIVE_IDLE_SEC,
             (unsigned int)ESP_MQTT_TCP_KEEPALIVE_INTERVAL_SEC,
             (unsigned int)ESP_MQTT_TCP_KEEPALIVE_COUNT);
    return MQTT_STATUS_SUCCESS;
}

Mqtt_Client_Status_t Mqtt_Client_Deinit(void *client_context)
{
    struct Mqtt_Client_Context *ctx = (struct Mqtt_Client_Context *)client_context;
    if (!ctx || !ctx->handle) { return MQTT_STATUS_SUCCESS; }
    esp_mqtt_client_destroy(ctx->handle);
    ctx->handle  = NULL;
    ctx->started = false;
    return MQTT_STATUS_SUCCESS;
}

Mqtt_Client_Status_t Mqtt_Client_Connect(void *client_context)
{
    struct Mqtt_Client_Context *ctx = (struct Mqtt_Client_Context *)client_context;
    if (!ctx || !ctx->handle) { return MQTT_STATUS_INVALID_PARAM; }

    /* 清除舊的連線/斷線信號，避免殘留 */
    xSemaphoreTake(ctx->conn_sem, 0);
    xSemaphoreTake(ctx->disc_sem, 0);
    ctx->connected = false;
    ctx->connect_seq++;
    ctx->connect_start_ms = mqtt_now_ms();

    ESP_LOGI(MI_MQTT_TAG,
             "[MQTT_DIAG][CONNECT_START] seq=%" PRIu32 " started=%d host=%s "
             "port=%u mqtt_keepalive=enabled requested_keepalive=%u wait=%u",
             ctx->connect_seq,
             ctx->started ? 1 : 0,
             ctx->config.Tcp_Connect_Params.Host ? ctx->config.Tcp_Connect_Params.Host : "(null)",
             (unsigned int)ctx->config.Tcp_Connect_Params.Port,
             (unsigned int)ctx->config.Keepalive,
             (unsigned int)CONNECT_WAIT_MS);

    esp_err_t ret;
    const char *op_name;
    if (!ctx->started)
    {
        op_name = "start";
        ret = esp_mqtt_client_start(ctx->handle);
        if (ret == ESP_OK) { ctx->started = true; }
    }
    else
    {
        op_name = "reconnect";
        ret = esp_mqtt_client_reconnect(ctx->handle);
    }

    ESP_LOGI(MI_MQTT_TAG,
             "[MQTT_DIAG][CONNECT_CMD] seq=%" PRIu32 " op=%s ret=%d started=%d",
             ctx->connect_seq, op_name, (int)ret, ctx->started ? 1 : 0);

    if (ret != ESP_OK)
    {
        ESP_LOGE(MI_MQTT_TAG, "connect start/reconnect failed: %d", ret);
        return MQTT_STATUS_CONNECT_FAILED;
    }

    /* 同步等待 MQTT_EVENT_CONNECTED */
    if (xSemaphoreTake(ctx->conn_sem, pdMS_TO_TICKS(CONNECT_WAIT_MS)) != pdTRUE)
    {
        uint32_t elapsed_ms = mqtt_now_ms() - ctx->connect_start_ms;
        ESP_LOGE(MI_MQTT_TAG,
                 "connect timeout (%d ms), [MQTT_DIAG][CONNECT_TIMEOUT] seq=%" PRIu32
                 " elapsed=%" PRIu32 "ms connected=%d started=%d",
                 CONNECT_WAIT_MS,
                 ctx->connect_seq,
                 elapsed_ms,
                 ctx->connected ? 1 : 0,
                 ctx->started ? 1 : 0);
        return MQTT_STATUS_CONNECT_FAILED;
    }
    ESP_LOGI(MI_MQTT_TAG,
             "[MQTT_DIAG][CONNECT_DONE] seq=%" PRIu32 " elapsed=%" PRIu32
             "ms connected=%d started=%d",
             ctx->connect_seq,
             mqtt_now_ms() - ctx->connect_start_ms,
             ctx->connected ? 1 : 0,
             ctx->started ? 1 : 0);
    return MQTT_STATUS_SUCCESS;
}

Mqtt_Client_Status_t Mqtt_Client_Disconnect(void *client_context)
{
    struct Mqtt_Client_Context *ctx = (struct Mqtt_Client_Context *)client_context;
    if (!ctx || !ctx->handle) { return MQTT_STATUS_SUCCESS; }

    xSemaphoreTake(ctx->disc_sem, 0);   /* 清除舊信號 */

    mqtt_mark_disconnect_requested(ctx, "api_stop");
    ESP_LOGW(MI_MQTT_TAG,
             "[MQTT_DIAG][DISCONNECT_REQ] seq=%" PRIu32 " connected=%d started=%d reason=%s",
             ctx->connect_seq,
             ctx->connected ? 1 : 0,
             ctx->started ? 1 : 0,
             ctx->disconnect_reason ? ctx->disconnect_reason : "unknown");

    /* esp_mqtt_client_stop() 停止 task，DISCONNECTED 事件異步觸發 */
    esp_err_t stop_ret = esp_mqtt_client_stop(ctx->handle);
    if (stop_ret != ESP_OK)
    {
        ESP_LOGW(MI_MQTT_TAG,
                 "[MQTT_DIAG][DISCONNECT_STOP_FAIL] seq=%" PRIu32 " ret=%d",
                 ctx->connect_seq, (int)stop_ret);
    }

    /* 同步等待 MQTT_EVENT_DISCONNECTED，確保 callback 完成後再返回 */
    BaseType_t got_disc_evt = xSemaphoreTake(ctx->disc_sem, pdMS_TO_TICKS(DISCONNECT_WAIT_MS));
    ctx->connected = false;
    /* stop 後 task 已銷毀，下次必須用 start() 重建，不能用 reconnect() */
    ctx->started = false;
    ESP_LOGW(MI_MQTT_TAG,
             "[MQTT_DIAG][DISCONNECT_DONE] seq=%" PRIu32 " stop_ret=%d disc_evt=%d",
             ctx->connect_seq, (int)stop_ret, got_disc_evt == pdTRUE ? 1 : 0);
    return MQTT_STATUS_SUCCESS;
}

uint16_t Mqtt_Client_Subscribe(void *client_context, const char *topic, uint8_t qos)
{
    struct Mqtt_Client_Context *ctx = (struct Mqtt_Client_Context *)client_context;
    if (!ctx || !ctx->handle || !topic) { return 0; }

    int msgid = esp_mqtt_client_subscribe(ctx->handle, topic, (int)qos);
    ESP_LOGI(MI_MQTT_TAG,
             "[MQTT_SUB] subscribe topic=%s qos=%u msgId=%d [MQTT_DIAG][SUB_REQ] "
             "seq=%" PRIu32 " connected=%d",
             topic, (unsigned)qos, msgid, ctx->connect_seq, ctx->connected ? 1 : 0);
    return (uint16_t)(msgid < 0 ? 0 : msgid);
}

uint16_t Mqtt_Client_Unsubscribe(void *client_context, const char *topic, uint8_t qos)
{
    struct Mqtt_Client_Context *ctx = (struct Mqtt_Client_Context *)client_context;
    if (!ctx || !ctx->handle || !topic) { return 0; }
    (void)qos;

    int msgid = esp_mqtt_client_unsubscribe(ctx->handle, topic);
    return (uint16_t)(msgid < 0 ? 0 : msgid);
}

int Mqtt_Client_Publish(void *client_context, const char *topic,
                        const uint8_t *payload, size_t length, uint8_t qos)
{
    struct Mqtt_Client_Context *ctx = (struct Mqtt_Client_Context *)client_context;
    if (!ctx || !ctx->handle || !topic) { return -1; }

    int msgid = esp_mqtt_client_publish(ctx->handle, topic,
                                         (const char *)payload,
                                         (int)length, (int)qos, 0);
    if (msgid < 0)
    {
        ESP_LOGE(MI_MQTT_TAG,
                 "[MQTT_PUB] publish failed topic=%s [MQTT_DIAG][PUB_FAIL] "
                 "seq=%" PRIu32 " len=%u qos=%u connected=%d",
                 topic,
                 ctx->connect_seq,
                 (unsigned int)length,
                 (unsigned int)qos,
                 ctx->connected ? 1 : 0);
        return -1;
    }
    uint32_t now_ms = mqtt_now_ms();
    ESP_LOGI(MI_MQTT_TAG, "[MQTT_PUB_TS] msgid=%d ts=%" PRIu32 " ms", msgid, now_ms);
    mqtt_puback_track_add(ctx, (uint16_t)msgid, qos, (uint16_t)(length > UINT16_MAX ? UINT16_MAX : length));
    return msgid;
}

Mqtt_Client_Status_t Mqtt_Client_Yield(void *client_context)
{
    struct Mqtt_Client_Context *ctx = (struct Mqtt_Client_Context *)client_context;
    /* esp-mqtt 有自己的 task，此處只需 sleep 讓出 CPU，並回報連線狀態 */
    vTaskDelay(pdMS_TO_TICKS(100));
    if (!ctx || !ctx->connected)
    {
        uint32_t now_ms = mqtt_now_ms();
        if (!ctx || now_ms - ctx->last_yield_fail_log_ms >= 1000)
        {
            if (ctx)
            {
                ctx->last_yield_fail_log_ms = now_ms;
            }
            ESP_LOGW(MI_MQTT_TAG,
                     "[MQTT_DIAG][YIELD_FAIL] ctx=%p connected=%d started=%d seq=%" PRIu32,
                     ctx,
                     (ctx && ctx->connected) ? 1 : 0,
                     (ctx && ctx->started) ? 1 : 0,
                     ctx ? ctx->connect_seq : 0);
        }
        return MQTT_STATUS_NETWORK_TIMEOUT;
    }
    if (mqtt_puback_track_check_timeout(ctx))
    {
        ESP_LOGE(MI_MQTT_TAG,
                 "[MQTT_DIAG][PUBACK_HARD_TIMEOUT_DISCONNECT] seq=%" PRIu32 " started=%d",
                 ctx->connect_seq,
                 ctx->started ? 1 : 0);
        mqtt_mark_disconnect_requested(ctx, "puback_hard_timeout");
        esp_mqtt_client_disconnect(ctx->handle);
        return MQTT_STATUS_NETWORK_TIMEOUT;
    }
    return MQTT_STATUS_SUCCESS;
}

uint32_t Mqtt_Client_Last_Rx_Age_Ms(void *client_context)
{
    struct Mqtt_Client_Context *ctx = (struct Mqtt_Client_Context *)client_context;
    if (!ctx || !ctx->last_rx_ms)
    {
        return UINT32_MAX;
    }

    return mqtt_now_ms() - ctx->last_rx_ms;
}

bool Mqtt_Client_Pending_Qos1_Snapshot(void *client_context,
                                       uint32_t *pending_count,
                                       uint32_t *oldest_age_ms,
                                       uint16_t *oldest_msg_id)
{
    struct Mqtt_Client_Context *ctx = (struct Mqtt_Client_Context *)client_context;
    if (!ctx)
    {
        if (pending_count) { *pending_count = 0; }
        if (oldest_age_ms) { *oldest_age_ms = 0; }
        if (oldest_msg_id) { *oldest_msg_id = 0; }
        return false;
    }

    mqtt_puback_track_snapshot(ctx, pending_count, oldest_age_ms, oldest_msg_id);
    return true;
}

bool Mqtt_Client_Pingresp_Snapshot(void *client_context, bool *wait_ping, uint32_t *pingreq_age_ms)
{
    struct Mqtt_Client_Context *ctx = (struct Mqtt_Client_Context *)client_context;
    if (wait_ping)
    {
        *wait_ping = false;
    }
    if (pingreq_age_ms)
    {
        *pingreq_age_ms = 0;
    }
    if (!ctx || !ctx->handle)
    {
        return false;
    }

#if defined(CONFIG_MI_MQTT_HAVE_ESP_BROKER_LIVENESS)
    return esp_mqtt_client_get_pingresp_state(ctx->handle, wait_ping, pingreq_age_ms);
#else
    /* Stock esp-mqtt: no pingresp introspection API. Degrade to "no data". */
    return false;
#endif
}

bool Mqtt_Client_Pingresp_Diag_Snapshot(void *client_context,
                                        bool *wait_ping,
                                        uint32_t *pingreq_age_ms,
                                        uint32_t *last_pingresp_rtt_ms)
{
    struct Mqtt_Client_Context *ctx = (struct Mqtt_Client_Context *)client_context;
    if (wait_ping)
    {
        *wait_ping = false;
    }
    if (pingreq_age_ms)
    {
        *pingreq_age_ms = 0;
    }
    if (last_pingresp_rtt_ms)
    {
        *last_pingresp_rtt_ms = UINT32_MAX;
    }
    if (!ctx || !ctx->handle)
    {
        return false;
    }

#if defined(CONFIG_MI_MQTT_HAVE_ESP_BROKER_LIVENESS)
    return esp_mqtt_client_get_pingresp_diag(ctx->handle, wait_ping, pingreq_age_ms, last_pingresp_rtt_ms);
#else
    /* Stock esp-mqtt: no pingresp diagnostics API. Degrade to "no data". */
    return false;
#endif
}

const char *Mqtt_Client_Broker_Liveness_Str(Mqtt_Client_Broker_Liveness_t state)
{
    switch (state)
    {
        case MQTT_BROKER_LIVENESS_DISCONNECTED: return "DISCONNECTED";
        case MQTT_BROKER_LIVENESS_CONNECTING:    return "CONNECTING";
        case MQTT_BROKER_LIVENESS_SUBSCRIBING:   return "SUBSCRIBING";
        case MQTT_BROKER_LIVENESS_READY:         return "READY";
        case MQTT_BROKER_LIVENESS_PROBING:       return "PROBING";
        case MQTT_BROKER_LIVENESS_DEAD:          return "DEAD";
        default:                                 return "UNKNOWN";
    }
}

bool Mqtt_Client_Broker_Liveness_Snapshot(void *client_context,
                                          Mqtt_Client_Broker_Liveness_Snapshot_t *snapshot)
{
    struct Mqtt_Client_Context *ctx = (struct Mqtt_Client_Context *)client_context;
    uint32_t now_ms;
    uint32_t core_snapshot_start_ms;
    if (snapshot)
    {
        memset(snapshot, 0, sizeof(*snapshot));
        snapshot->state = MQTT_BROKER_LIVENESS_DISCONNECTED;
        snapshot->last_inbound_alive_age_ms = UINT32_MAX;
        snapshot->last_pingresp_rtt_ms = UINT32_MAX;
    }
    if (!ctx || !ctx->handle || !snapshot)
    {
        return false;
    }

    now_ms = mqtt_now_ms();
    taskENTER_CRITICAL(&ctx->diag_lock);
    snapshot->event_count = ctx->event_count;
    snapshot->last_event_age_ms = ctx->last_event_ms ?
                                  (now_ms - ctx->last_event_ms) :
                                  UINT32_MAX;
    snapshot->callback_active = ctx->callback_active;
    snapshot->callback_active_age_ms =
        (ctx->callback_active && ctx->callback_start_ms) ?
        (now_ms - ctx->callback_start_ms) : 0U;
    snapshot->callback_last_ms = ctx->callback_last_ms;
    snapshot->callback_max_ms = ctx->callback_max_ms;
    snapshot->callback_slow_count = ctx->callback_slow_count;
    taskEXIT_CRITICAL(&ctx->diag_lock);

#if defined(CONFIG_MI_MQTT_HAVE_ESP_BROKER_LIVENESS)
    esp_mqtt_broker_liveness_snapshot_t esp_snapshot = {0};
    core_snapshot_start_ms = mqtt_now_ms();
    bool core_snapshot_ok =
        esp_mqtt_client_get_broker_liveness(ctx->handle, &esp_snapshot);
    snapshot->core_snapshot_wait_ms = mqtt_now_ms() - core_snapshot_start_ms;
    if (!core_snapshot_ok)
    {
        return false;
    }

    snapshot->state = mqtt_map_broker_liveness(esp_snapshot.state);
    snapshot->keepalive_ms = esp_snapshot.keepalive_ms;
    snapshot->pingresp_timeout_ms = esp_snapshot.pingresp_timeout_ms;
    snapshot->wait_ping = esp_snapshot.wait_ping;
    snapshot->pingreq_age_ms = esp_snapshot.pingreq_age_ms;
    snapshot->last_inbound_alive_age_ms = esp_snapshot.last_inbound_alive_age_ms;
    snapshot->last_pingresp_rtt_ms = esp_snapshot.last_pingresp_rtt_ms;
    snapshot->task_progress_age_ms = esp_snapshot.task_progress_age_ms;
    snapshot->task_last_loop_gap_ms = esp_snapshot.task_last_loop_gap_ms;
    snapshot->task_max_loop_gap_ms = esp_snapshot.task_max_loop_gap_ms;
    snapshot->task_last_poll_ms = esp_snapshot.task_last_poll_ms;
    snapshot->task_max_poll_ms = esp_snapshot.task_max_poll_ms;
    snapshot->task_stack_hwm = esp_snapshot.task_stack_hwm;
    snapshot->task_core = esp_snapshot.task_core;
    snapshot->task_priority = esp_snapshot.task_priority;
    return true;
#else
    /* Stock esp-mqtt: no broker-liveness core API. Report the broker as READY
       whenever the MQTT app layer reached this point (already connected), so
       the AI-publish / link-health watchdogs in entity_mqtt_app trust
       esp-mqtt's built-in keepalive instead of force-reconnecting on absent
       liveness data. The mi_mqtt-native diag fields captured above (event /
       callback timing) remain populated; the esp-core fields stay zeroed. */
    (void)core_snapshot_start_ms;
    snapshot->state = MQTT_BROKER_LIVENESS_READY;
    snapshot->last_inbound_alive_age_ms = 0U;
    snapshot->last_pingresp_rtt_ms = 0U;
    return true;
#endif /* CONFIG_MI_MQTT_HAVE_ESP_BROKER_LIVENESS */
}

/* no-op：esp-mqtt 內建 keepalive 會自行維護 PINGREQ/PINGRESP。 */
void Mqtt_Client_Reset_Keepalive(void *client_context)
{
    (void)client_context;
}

char *Mqtt_Client_Interface_Local_Ip(void *client_context, char *buf, int max_size)
{
    (void)client_context;
    if (buf && max_size > 0) { buf[0] = '\0'; }
    return buf;
}

char *Mqtt_Client_Interface_Mac(void *client_context, char *buf, int max_size)
{
    (void)client_context;
    if (buf && max_size > 0) { buf[0] = '\0'; }
    return buf;
}

char *Mqtt_Client_Interface_Name(void *client_context, char *buf, int max_size)
{
    (void)client_context;
    if (buf && max_size > 0) { buf[0] = '\0'; }
    return buf;
}

uint32_t Mqtt_Client_Ping_Rtt_Ms(void *client_context)
{
    (void)client_context;
    return 0;
}
